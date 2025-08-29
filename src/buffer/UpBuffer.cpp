
#include "UpBuffer.h"
#include <iostream>
namespace GNN {
UpBuffer::UpBuffer(const std::string &name,
                   GNN::dramsim3_wrapper *dramsim3_wrapper_, addr_t addr_init_)
    : SimObject(name), addr(addr_init_), addr_init(addr_init_),
      dramsim3_wrapper(dramsim3_wrapper_), buffer_Data(8),
      tickEvent([this] { sendpkt_event(); }, name + ".tickEvent"),
      send_data_retryrespEvent([this] { send_data_2cal(); },
                               name + ".send_data_respEvent") {
  buf_size = 3;
  for (int i = 0; i < num_ports; ++i) {
    bufferdata_num[i] = 0;
    retryReq[i] = false;
    requestPorts.emplace_back(name + ".buf_side" + std::to_string(i), *this, i);
  }
}
void UpBuffer::init() {
  //  每个端口发一个包
  for (int i = 0; i < num_ports; ++i) {
    PacketPtr pkt = PacketManager::create_read_packet(addr, 64);
    addr += 64;
    D_INFO("BUFFER", "[UpBuffer] sendTimingReq addr:%d on port:%d",
           pkt->getAddr(), i);
    bool ok = sendTimingReq(pkt, i);
  }
}
void UpBuffer::sendpkt_event() {
  // 每个上游端口尝试发一个包，如果下游忙则标记重试
  for (int port = 0; port < num_ports; port++) {
    if (bufferdata_num[port] > buf_size) {
      // 本端口本地缓存已满，暂不再发
      retryReq[port] = true;
      continue;
    }
    PacketPtr pkt = PacketManager::create_read_packet(addr + port * 64, 64);
    D_INFO("BUFFER", "[UpBuffer] sendpkt_event addr:%d on port:%d",
           pkt->getAddr(), port);
    bool ok = sendTimingReq(pkt, port);
    if (!ok) {
      // 下游拒绝，等待 recvReqRetry 再重发
      retryReq[port] = true;
    }
  }
}
bool UpBuffer::sendTimingReq(PacketPtr pkt, int port_id) {
  // 通过端口发送请求到 DramArb
  bool ok = false;
  if (port_id >= 0 && port_id < num_ports)
    ok = requestPorts[port_id].sendTimingReq(pkt);
  return ok;
}

bool UpBuffer::recvTimingResp(PacketPtr pkt, int port_id) {
  assert(port_id >= 0 && port_id < num_ports);
  // 如果本地缓冲达到上限，暂时不能接收，等待后续重试
  if (bufferdata_num[port_id] > buf_size) {
    retryResp[port_id] = true;
    return false;
  }
  buffer_Data[port_id].push_back(pkt);
  ++bufferdata_num[port_id];
  D_INFO("BUFFER", "[UpBuffer] Received response for addr:%d on port:%d",
         pkt->getAddr(), port_id);
  // 有新数据到达，触发一次下行发送尝试
  if (!tickEvent.scheduled()) {
    schedule(tickEvent, curTick() + 1);
  }
  return true;
}
void UpBuffer::send_data_2cal() {
  bool sent[num_ports] = {false};
  for (int port = 0; port < num_ports; port++) {
    if (!buffer_Data[port].empty()) {
      PacketPtr pkt = buffer_Data[port].front();
      // 这里可连接计算单元：若计算端不忙则发送
      buffer_Data[port].pop_front();
      --bufferdata_num[port];
      sent[port] = true;
    }
  }
  // 只要还有未清空的数据，就继续调度下一拍尝试
  bool has_pending = false;
  for (int port = 0; port < num_ports; port++) {
    if (!buffer_Data[port].empty()) {
      has_pending = true;
      break;
    }
  }
  if (!send_data_retryrespEvent.scheduled() && has_pending) {
    schedule(send_data_retryrespEvent, curTick() + 1);
  }
}
void UpBuffer::sendRetryReq(int port_id) {
  // 针对指定端口的重试：仅该端口重新发送一个请求
  if (port_id < 0 || port_id >= num_ports)
    return;
  PacketPtr pkt = PacketManager::create_read_packet(addr + port_id * 64, 64);
  D_INFO("BUFFER", "[UpBuffer] sendRetryReq addr:%d on port:%d", pkt->getAddr(),
         port_id);
  bool ok = sendTimingReq(pkt, port_id);
  if (!ok) {
    retryReq[port_id] = true;
  } else {
    retryReq[port_id] = false;
  }
}
} // namespace GNN
