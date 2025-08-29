#include "dram_arb.h"
#include <iostream>
#include <stdexcept>

namespace GNN {

DramArb::DramArb(const std::string &_name, int buf_size_, int num_upstreams_)
    : SimObject(_name), 
      buf_size(buf_size_),
      num_upstreams(num_upstreams_),
      // 事件：仲裁和响应发送
      arbEvent([this] { arbitrate(); }, _name + ".arbEvent"),
      sendResponseEvent([this] { sendResponse(); }, _name + ".sendResponseEvent"),
      // 轮询指针：每个bank的读写轮询起点
      rrReadIdx(num_banks, 0),
      rrWriteIdx(num_banks, 0) {
  
  D_INFO("DRAM_ARB", "DramArb构造函数: num_upstreams=%d", num_upstreams);
  
  // 第一步：初始化基本状态
  initializeBasicState();
  
  // 第二步：分配多上游输入缓冲区
  allocateInputBuffers();
  
  // 第三步：创建端口
  createPorts(_name);
}

void DramArb::initializeBasicState() {
  // 初始化每个bank的读请求计数
  for (int bank = 0; bank < num_banks; bank++) {
    nbrOutstandingReads[bank] = 0;
  }
  
  // 初始化每个bank每个上游的重试标志
  for (int bank = 0; bank < num_banks; bank++) {
    for (int up = 0; up < num_upstreams; up++) {
      retryReq[bank][up] = false;    // 请求重试标志
      retryResp[bank][up] = false;   // 响应重试标志
    }
  }
}

void DramArb::allocateInputBuffers() {
  // 为每个bank分配读缓冲区（每个上游一个队列）
  readInBufs.resize(num_banks);
  writeInBufs.resize(num_banks);
  
  for (int bank = 0; bank < num_banks; bank++) {
    readInBufs[bank].resize(num_upstreams);   // 每个bank有num_upstreams个读队列
    writeInBufs[bank].resize(num_upstreams);  // 每个bank有num_upstreams个写队列
  }
}

void DramArb::createPorts(const std::string& name) {
  // 创建响应端口：每个bank有num_upstreams个，供上游连接
  responsePorts.resize(num_banks);
  for (int bank = 0; bank < num_banks; bank++) {
    for (int up = 0; up < num_upstreams; up++) {
      std::string port_name = name + ".response" + std::to_string(bank) + "_" + std::to_string(up);
      responsePorts[bank].emplace_back(port_name, *this, bank, up);
    }
  }
  
  // 创建请求端口：每个bank一个，连接下游DRAM
  for (int bank = 0; bank < num_banks; bank++) {
    std::string port_name = name + ".request" + std::to_string(bank);
    requestPorts.emplace_back(port_name, *this, bank);
  }
}

Port &DramArb::getPort(const std::string &if_name, int idx) {
  // 处理响应端口：格式为 "response<bank>_<upstream>"
  if (if_name.find("response") == 0) {
    return parseResponsePortName(if_name, idx);
  }
  
  // 处理请求端口：格式为 "request<bank>"
  if (if_name.find("request") == 0) {
    return parseRequestPortName(if_name);
  }
  
  throw std::runtime_error("未知端口名: " + if_name);
}

Port &DramArb::parseResponsePortName(const std::string &if_name, int idx) {
  // 从 "response<bank>_<upstream>" 中提取bank和upstream编号
  size_t pos = 8;  // "response" 的长度
  int bank = -1;
  int up = 0;
  
  try {
    // 查找下划线分隔符
    size_t next = if_name.find('_', pos);
    
    // 提取bank编号
    std::string sbank = if_name.substr(pos, 
        next == std::string::npos ? std::string::npos : next - pos);
    bank = std::stoi(sbank);
    
    // 提取upstream编号
    if (next != std::string::npos) {
      // 有下划线：从下划线后提取
      std::string sup = if_name.substr(next + 1);
      up = std::stoi(sup);
    } else if (idx >= 0) {
      // 无下划线但提供了idx参数
      up = idx;
    } else {
      // 无下划线且无idx：默认为0
      up = 0;
    }
  } catch (...) {
    throw std::runtime_error("响应端口名格式错误: " + if_name);
  }
  
  // 验证范围并返回对应端口
  if (bank >= 0 && bank < num_banks && up >= 0 && up < num_upstreams) {
    return responsePorts[bank][up];
  }
  
  throw std::runtime_error("端口索引超出范围: bank=" + std::to_string(bank) + 
                          ", upstream=" + std::to_string(up));
}

Port &DramArb::parseRequestPortName(const std::string &if_name) {
  // 从 "request<bank>" 中提取bank编号
  int bank = std::stoi(if_name.substr(7));  // "request" 的长度
  
  if (bank >= 0 && bank < num_banks) {
    return requestPorts[bank];
  }
  
  throw std::runtime_error("请求端口索引超出范围: bank=" + std::to_string(bank));
}

// 兼容旧接口：默认使用上游0
bool DramArb::recvTimingReq(PacketPtr pkt, int bank_id) {
  return recvTimingReqUp(pkt, bank_id, 0);
}

// 新接口：支持指定上游编号
bool DramArb::recvTimingReqUp(PacketPtr pkt, int bank_id, int upstream_id) {
  // 参数验证
  assert(bank_id >= 0 && bank_id < num_banks);
  assert(upstream_id >= 0 && upstream_id < num_upstreams);
  
  // 检查是否可以接受新请求
  bool can_accept_read = nbrOutstandingReads[bank_id] + responseQueue[bank_id].size() < buf_size;
  
  bool accepted = false;
  
  if (pkt->isRead()) {
    // 处理读请求
    if (can_accept_read) {
      // 将请求放入对应上游的读缓冲区
      readInBufs[bank_id][upstream_id].push_back(pkt);
      
      // 记录待响应的读请求
      outstandingReads[bank_id][pkt->getAddr()].push(pkt);
      outstandingUpstream[bank_id][pkt->getAddr()].push(upstream_id);
      
      nbrOutstandingReads[bank_id]++;
      accepted = true;
      
      D_INFO("DRAM_ARB", "接受读请求: bank=%d, upstream=%d, addr=%d", 
             bank_id, upstream_id, pkt->getAddr());
    }
  } else if (pkt->isWrite()) {
    // 处理写请求
    if (writeInBufs[bank_id][upstream_id].size() < (size_t)buf_size) {
      writeInBufs[bank_id][upstream_id].push_back(pkt);
      accepted = true;
      
      D_INFO("DRAM_ARB", "接受写请求: bank=%d, upstream=%d, addr=%d", 
             bank_id, upstream_id, pkt->getAddr());
    }
  }
  
  if (accepted) {
    // 请求被接受，调度仲裁事件
    if (!arbEvent.scheduled()) {
      schedule(arbEvent, curTick() + 1);
    }
    return true;
  } else {
    // 请求被拒绝，设置重试标志
    retryReq[bank_id][upstream_id] = true;
    
    D_INFO("DRAM_ARB", "拒绝请求: bank=%d, upstream=%d, 读计数=%d, 响应队列=%d", 
           bank_id, upstream_id, nbrOutstandingReads[bank_id], 
           responseQueue[bank_id].size());
    return false;
  }
}

bool DramArb::recvTimingResp(PacketPtr pkt, int bank_id) {
  assert(bank_id >= 0 && bank_id < num_banks);
  
  addr_t addr = pkt->getAddr();
  
  // 查找对应的待响应读请求
  auto p = outstandingReads[bank_id].find(addr);
  assert(p != outstandingReads[bank_id].end());
  
  // 取出待响应的包
  PacketPtr res_pkt = p->second.front();
  p->second.pop();
  
  // 如果该地址没有更多待响应请求，清理记录
  if (p->second.empty()) {
    outstandingReads[bank_id].erase(p);// 需要迭代器来删除
  }
  
  // 更新计数
  assert(nbrOutstandingReads[bank_id] > 0);
  --nbrOutstandingReads[bank_id];
  
  // 找到对应的上游编号
  auto &qUp = outstandingUpstream[bank_id][addr];
  assert(!qUp.empty());
  int up = qUp.front();
  qUp.pop();
  
  // 如果该地址没有更多上游记录，清理
  if (qUp.empty()) {
    outstandingUpstream[bank_id].erase(addr);//删除条目
  }
  
  // 准备发送响应
  accessAndRespond(bank_id, res_pkt, up);
  return true;
}

void DramArb::accessAndRespond(int bank_id, PacketPtr pkt, int upstream_id) {
  // 将响应包和对应的上游编号放入响应队列
  responseQueue[bank_id].push_back({pkt, upstream_id});
  
  D_INFO("DRAM_ARB", "准备响应: addr=%d, bank=%d, upstream=%d", 
         pkt->getAddr(), bank_id, upstream_id);
  
  // 调度响应发送事件
  Tick delay = 1;
  Tick time = curTick() + delay;
  if (!sendResponseEvent.scheduled()) {
    schedule(sendResponseEvent, time);
  }
}

void DramArb::sendResponse() {
  // 遍历所有bank和上游，尝试发送响应
  for (int bank = 0; bank < num_banks; bank++) {
    for (int upstream_id = 0; upstream_id < num_upstreams; upstream_id++) {
      // 检查该上游是否在等待重试
      if (!retryResp[bank][upstream_id]) {
        // 检查是否有待发送的响应
        if (!responseQueue[bank].empty()) {
          auto &front = responseQueue[bank].front();
          PacketPtr pkt = front.first;
          int target_upstream = front.second;  // 响应要发送到的上游
          
          // 尝试发送响应
          bool success = responsePorts[bank][target_upstream].sendTimingResp(pkt);
          
          if (success) {
            // 发送成功，从队列中移除
            responseQueue[bank].pop_front();
            
            // 如果还有更多响应，继续调度
            if (!responseQueue[bank].empty() && !sendResponseEvent.scheduled()) {
              schedule(sendResponseEvent, curTick() + 1);
            }
            
            // 如果该上游在等待重试，发送重试信号
            if (retryReq[bank][upstream_id]) {
              responsePorts[bank][target_upstream].sendRetryReq();
              retryReq[bank][upstream_id] = false;
              
              D_INFO("DRAM_ARB", "发送重试信号: bank=%d, upstream=%d", bank, upstream_id);
            }
          } else {
            // 发送失败，设置重试标志
            D_DEBUG("DRAM_ARB", "响应发送失败: bank=%d, upstream=%d", bank, upstream_id);
            retryResp[bank][upstream_id] = true;
          }
        }
      }
    }
  }
}

void DramArb::handleRespRetry(int bank_id, int upstream_id) {
  assert(bank_id >= 0 && bank_id < num_banks);
  assert(upstream_id >= 0 && upstream_id < num_upstreams);
  
  D_INFO("DRAM_ARB", "收到响应重试: bank=%d, upstream=%d", bank_id, upstream_id);
  
  // 清除重试标志，重新尝试发送
  retryResp[bank_id][upstream_id] = false;
  sendResponse();
}

void DramArb::arbitrate() {
  bool need_resched = false;    // 是否有bank需要重试
  bool has_pending = false;     // 是否还有待处理的请求
  
  // 遍历每个bank进行仲裁
  for (int bank = 0; bank < num_banks; bank++) {
    bool sent_this_bank = false;
    
    // 第一步：仲裁读请求（优先级高于写）
    sent_this_bank = arbitrateReadRequests(bank);
    
    // 第二步：如果读请求没有发送，仲裁写请求
    if (!sent_this_bank) {
      sent_this_bank = arbitrateWriteRequests(bank);
    }
    
    // 检查是否还有待处理的请求
    has_pending = checkPendingRequests(bank);
    
    // 如果本轮没有发送成功，需要重试
    if (!sent_this_bank) {
      need_resched = true;
    }
  }
  
  // 如果有待处理请求或需要重试，调度下一轮仲裁
  if (!arbEvent.scheduled() && (has_pending || need_resched)) {
    schedule(arbEvent, curTick() + 1);
  }
}

bool DramArb::arbitrateReadRequests(int bank) {
  // 从当前轮询位置开始，尝试发送读请求
  for (int k = 0; k < num_upstreams; k++) {
    int up = (rrReadIdx[bank] + k) % num_upstreams;
    
    if (!readInBufs[bank][up].empty()) {
      PacketPtr pkt = readInBufs[bank][up].front();
      
      if (requestPorts[bank].sendTimingReq(pkt)) {
        // 发送成功
        D_INFO("DRAM_ARB", "发送读请求: bank=%d, upstream=%d", bank, up);
        
        readInBufs[bank][up].pop_front();
        rrReadIdx[bank] = (up + 1) % num_upstreams;  // 更新轮询指针
        
        return true;  // 该bank本轮已发送
      }
    }
  }
  
  return false;  // 该bank本轮未发送
}

bool DramArb::arbitrateWriteRequests(int bank) {
  // 从当前轮询位置开始，尝试发送写请求
  for (int k = 0; k < num_upstreams; k++) {
    int up = (rrWriteIdx[bank] + k) % num_upstreams;
    
    if (!writeInBufs[bank][up].empty()) {
      PacketPtr pkt = writeInBufs[bank][up].front();
      
      if (requestPorts[bank].sendTimingReq(pkt)) {
        // 发送成功
        D_INFO("DRAM_ARB", "发送写请求: bank=%d, upstream=%d", bank, up);
        
        writeInBufs[bank][up].pop_front();
        rrWriteIdx[bank] = (up + 1) % num_upstreams;  // 更新轮询指针
        
        return true;  // 该bank本轮已发送
      }
    }
  }
  
  return false;  // 该bank本轮未发送
}

bool DramArb::checkPendingRequests(int bank) {
  // 检查该bank是否还有待处理的请求
  for (int up = 0; up < num_upstreams; up++) {
    if (!readInBufs[bank][up].empty() || !writeInBufs[bank][up].empty()) {
      return true;  // 还有待处理请求
    }
  }
  return false;  // 没有待处理请求
}

void DramArb::scheduleArbEvent(int bank) {
  // 预留接口：可以在这里添加特定bank的仲裁调度逻辑
}

void DramArb::handleReqRetry(int bank_id) {
  assert(bank_id >= 0 && bank_id < num_banks);
  
  D_INFO("DRAM_ARB", "收到请求重试: bank=%d", bank_id);
  
  // 调度仲裁事件，重新尝试发送
  if (!arbEvent.scheduled()) {
    schedule(arbEvent, curTick() + 1);
  }
}

} // namespace GNN