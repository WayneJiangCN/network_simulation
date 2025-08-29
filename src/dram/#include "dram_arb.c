#include "dram_arb.h"
#include <iostream>
#include <stdexcept>

namespace GNN
{
  DramArb::DramArb(const std::string &_name, int buf_size_, int num_upstreams_)
      : SimObject(_name), buf_size(buf_size_),
        arbEvent([this]
                 { arbitrate(); }, _name + ".arbEvent"),
        sendResponseEvent([this]
                          { sendResponse(); },
                          _name + ".sendResponseEvent"),
        num_upstreams(num_upstreams_),
        rrReadIdx(num_banks, 0),
        rrWriteIdx(num_banks, 0)
  {
    // 初始化状态
    for (int bank = 0; bank < num_banks; bank++)
    {
      nbrOutstandingReads[bank] = 0;
    }
    for (int bank = 0; bank < num_banks; bank++)
    {
      for (int num_up = 0; num_up < num_upstreams; num_up++)
      {
        retryReq[bank][num_up] = false;
        retryResp[bank][num_up] = false;
      }
    }
    // 分配多上游输入缓冲
    readInBufs.resize(num_banks);
    writeInBufs.resize(num_banks);
    for (int bank = 0; bank < num_banks; bank++)
    {
      readInBufs[bank].resize(num_upstreams);
      writeInBufs[bank].resize(num_upstreams);
    }

    // 构建每个 bank 的上游响应端口集合
    responsePorts.resize(num_banks);
    for (int bank = 0; bank < num_banks; bank++)
    {
      for (int up = 0; up < num_upstreams; up++)
      {
        responsePorts[bank].emplace_back(
            _name + ".response" + std::to_string(bank) + "_" +
                std::to_string(up),
            *this, bank, up);
      }
    }
    // 面向 DRAM 的下游请求端口，每个 bank 一个
    for (int bank = 0; bank < num_banks; bank++)
    {
      requestPorts.emplace_back(_name + ".request" + std::to_string(bank),
                                *this, bank);
    }
  }

  Port &DramArb::getPort(const std::string &if_name, int idx)
  {
    if (if_name.find("response") == 0)
    {
      // 期望格式：response<bank>_<up>
      // 提取 bank
      size_t pos = 8;
      int bank = -1;
      int up = 0;
      try
      {
        // 读 bank 数字
        size_t next = if_name.find('_', pos);
        std::string sbank = if_name.substr(pos, next == std::string::npos
                                                    ? std::string::npos
                                                    : next - pos);
        bank = std::stoi(sbank);

        if (next != std::string::npos)
        {
          std::string sup = if_name.substr(next + 1);
          up = std::stoi(sup);
        }
        else if (idx >= 0)
        {
          up = idx; // 兼容可选 idx 作为上游索引
        }
        else
        {
          up = 0; // 兼容旧格式 response<bank>
        }
      }
      catch (...)
      {
        throw std::runtime_error("Invalid response port name: " + if_name);
      }
         
      if (bank >= 0 && bank < num_banks && up >= 0 && up < num_upstreams)
        return responsePorts[bank][up];
    }
    if (if_name.find("request") == 0)
    {
      int bank = std::stoi(if_name.substr(7));
      if (bank >= 0 && bank < num_banks)
        return requestPorts[bank];
    }
    throw std::runtime_error("No such port: " + if_name);
  }
  bool DramArb::recvTimingReq(PacketPtr pkt, int bank_id)
  {
    // 兼容：默认上游0
    return recvTimingReqUp(pkt, bank_id, 0);
  }

  bool DramArb::recvTimingReqUp(PacketPtr pkt, int bank_id, int upstream_id)
  {
    assert(bank_id >= 0 && bank_id < num_banks);
    assert(upstream_id >= 0 && upstream_id < num_upstreams);
    bool accepted = false;
    bool write_accepted = false;
    bool read_accepted =
        nbrOutstandingReads[bank_id] + responseQueue[bank_id].size() < buf_size;
    if (!pkt->isWrite())
    {
      if (read_accepted)
      {
        readInBufs[bank_id][upstream_id].push_back(pkt);
        outstandingReads[bank_id][pkt->getAddr()].push(pkt);
        outstandingUpstream[bank_id][pkt->getAddr()].push(upstream_id);
        ++nbrOutstandingReads[bank_id];
        accepted = true;
      }
    }
    else if (pkt->isWrite())
    {
      if (writeInBufs[bank_id][upstream_id].size() < (size_t)buf_size)
      {
        writeInBufs[bank_id][upstream_id].push_back(pkt);
        write_accepted = true;
        accepted = true;
      }
    }
    if (accepted)
    {
      if (!arbEvent.scheduled())
      {
        schedule(arbEvent, curTick() + 1);
      }
      return true;
    }
    else
    {
      retryReq[bank_id][upstream_id] = true;
      D_INFO(
          "DRAM_ARB",
          "recvTimingReq bank: %d, retryReq: %d, nbrOutstandingReads[bank_id]: "
          "%d, responseQueue[bank_id].size(): %d",
          bank_id, retryReq[bank_id][upstream_id], nbrOutstandingReads[bank_id],
          responseQueue[bank_id].size());
      return false;
    }
  }

  bool DramArb::recvTimingResp(PacketPtr pkt, int bank_id)
  {
    assert(bank_id >= 0 && bank_id < num_banks);
    addr_t addr = pkt->getAddr();

    auto p = outstandingReads[bank_id].find(addr);
    assert(p != outstandingReads[bank_id].end());
    PacketPtr res_pkt = p->second.front();
    p->second.pop();
    if (p->second.empty())
      outstandingReads[bank_id].erase(p);
    assert(nbrOutstandingReads[bank_id] > 0);
    --nbrOutstandingReads[bank_id];
    // 找到对应的上游
    auto &qUp = outstandingUpstream[bank_id][addr];
    assert(!qUp.empty());
    int up = qUp.front();
    qUp.pop();
    if (qUp.empty())
    {
      outstandingUpstream[bank_id].erase(addr);
    }
    accessAndRespond(bank_id, res_pkt, up);
    return true;
  }

  void DramArb::accessAndRespond(int bank_id, PacketPtr pkt, int upstream_id)
  {
    responseQueue[bank_id].push_back({pkt, upstream_id});
    D_INFO("DRAM_ARB", "addr:%d,retryReq bank: %d upstream: %d", pkt->getAddr(), bank_id, upstream_id);
    Tick delay = 1;
    Tick time = curTick() + delay;
    if (!sendResponseEvent.scheduled())
    {
      schedule(sendResponseEvent, time);
    }
  }
  void DramArb::sendResponse()
  {
    // 八个bank同时发送数据
    for (int bank = 0; bank < num_banks; bank++)
    {
      for (int upstream_id = 0; upstream_id < num_upstreams; upstream_id++)
      {
        if (!retryResp[bank][upstream_id])
        {
          if (!responseQueue[bank].empty())
          {
            auto &front = responseQueue[bank].front();
            PacketPtr pkt = front.first;
            int up = front.second;
            bool success = responsePorts[bank][up].sendTimingResp(pkt);
            
            if (success)
            {
              responseQueue[bank].pop_front();
              if (!responseQueue[bank].empty() && !sendResponseEvent.scheduled())
              {
                schedule(sendResponseEvent, curTick() + 1); // 一次要发多少个数据str
              }
              if (retryReq[bank][upstream_id])
              {
                
                // 对所有上游广播 retryReq，简单实现：逐个发送
                // for (int up2 = 0; up2 < num_upstreams; up2++) {
                responsePorts[bank][up].sendRetryReq();
                // }
                // D_DEBUG("DRAM_ARB", "retryReq bank: %d", bank);
                retryReq[bank][upstream_id] = false;
              }
            }
            else
            {
              D_DEBUG("DRAM_ARB", "sendResponseEvent bank: %d, retryResp: %d", bank,
                      retryResp[bank][upstream_id]);
              retryResp[bank][upstream_id] = true;
            }
          }
        }
      }
    }
  }
  void DramArb::handleRespRetry(int bank_id, int upstream_id)
  {
    assert(bank_id >= 0 && bank_id < num_banks);
    D_INFO("DRAM_ARB", "retryResp bank: %d", bank_id);
    // assert(retryResp[bank_id]);
    retryResp[bank_id][upstream_id] = false;
    sendResponse();
  }
  void DramArb::arbitrate()
  {
    bool need_resched = false;
    bool has_pending = false;
    for (int bank = 0; bank < num_banks; bank++)
    {
      bool sent_this_bank = false;
      // 先仲裁读：多上游轮询
      for (int k = 0; k < num_upstreams && !sent_this_bank; k++)
      {
        int up = (rrReadIdx[bank] + k) % num_upstreams;
        if (!readInBufs[bank][up].empty())
        {
          PacketPtr pkt = readInBufs[bank][up].front();
          if (requestPorts[bank].sendTimingReq(pkt))
          {
            D_INFO("DRAM_ARB", "sendTimingReq READ bank: %d up: %d", bank, up);
            readInBufs[bank][up].pop_front();
            rrReadIdx[bank] = (up + 1) % num_upstreams;
            sent_this_bank = true;
          }
        }
      }
      // 再仲裁写
      if (!sent_this_bank)
      {
        for (int k = 0; k < num_upstreams && !sent_this_bank; k++)
        {
          int up = (rrWriteIdx[bank] + k) % num_upstreams;
          if (!writeInBufs[bank][up].empty())
          {
            PacketPtr pkt = writeInBufs[bank][up].front();
            if (requestPorts[bank].sendTimingReq(pkt))
            {
              D_INFO("DRAM_ARB", "sendTimingReq WRITE bank: %d up: %d", bank, up);
              writeInBufs[bank][up].pop_front();
              rrWriteIdx[bank] = (up + 1) % num_upstreams;
              sent_this_bank = true;
            }
          }
        }
      }
      // 统计是否仍有待仲裁请求（即使本轮已发送，也可能还剩）
      for (int up = 0; up < num_upstreams; up++)
      {
        if (!readInBufs[bank][up].empty() || !writeInBufs[bank][up].empty())
        {
          has_pending = true;
        }
      }
      // 若本 bank 本轮没能发送（下游忙），也需要尽快重试
      if (!sent_this_bank)
      {
        need_resched = true;
      }
    }
    // 只要还有待处理请求，或存在未发送成功的 bank，都应继续调度下一拍仲裁
    if (!arbEvent.scheduled() && (has_pending || need_resched))
    {
      schedule(arbEvent, curTick() + 1);
    }
  }

  void DramArb::scheduleArbEvent(int bank) {}

  void DramArb::handleReqRetry(int bank_id)
  {
    assert(bank_id >= 0 && bank_id < num_banks);
    D_INFO("DRAM_ARB", "handleReqRetry bank: %d", bank_id);
    if (!arbEvent.scheduled())
    {
      schedule(arbEvent, curTick() + 1);
    }
  }

} // namespace GNN