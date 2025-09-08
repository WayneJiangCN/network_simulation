#include "dma/DmaBuffer.h"

namespace GNN {

DmaBuffer::DmaBuffer(const std::string &name, addr_t base_addr, int row_words)
    : SimObject(name), base_addr_(base_addr), row_words_(row_words),
      tickEvent([this] { try_send_event(); }, name + ".tickEvent") {
  requestPorts.reserve(num_ports);
  req_fifos_.reserve(num_ports);
  for (int i = 0; i < num_ports; ++i) {
    retryReq[i] = false;
    requestPorts.emplace_back(name + ".dma_side" + std::to_string(i), *this,
                              i);
    req_fifos_.emplace_back();
  }
}

void DmaBuffer::init() {
  // On init, preload one row per bank as a write
  for (int bank = 0; bank < num_ports; ++bank) {
    std::vector<uint32_t> row;
    row.reserve(row_words_);
    for (int w = 0; w < row_words_; ++w) {
      row.push_back(static_cast<uint32_t>(base_addr_ + bank * 64 + w));
    }
    PacketPtr pkt = PacketManager::create_write_packet(base_addr_ + bank * 64,
                                                       row);
    req_fifos_[bank].push_back(pkt);
  }
  if (!tickEvent.scheduled())
    schedule(tickEvent, curTick() + 1);
}

void DmaBuffer::try_send_event() {
  for (int bank = 0; bank < num_ports; ++bank) {
    if (!req_fifos_[bank].empty()) {
      PacketPtr pkt = req_fifos_[bank].front();
      if (requestPorts[bank].sendTimingReq(pkt)) {
        req_fifos_[bank].pop_front();
      }
    }
  }
  if (!tickEvent.scheduled())
    schedule(tickEvent, curTick() + 1);
}

bool DmaBuffer::sendTimingReq(PacketPtr pkt, int port_id) {
  bool ok = false;
  if (port_id >= 0 && port_id < num_ports)
    ok = requestPorts[port_id].sendTimingReq(pkt);
  return ok;
}

bool DmaBuffer::recvTimingResp(PacketPtr pkt, int port_id) {
  // Consume and delete
  delete pkt;
  return true;
}

void DmaBuffer::sendRetryReq(int port_id) {
  if (!tickEvent.scheduled())
    schedule(tickEvent, curTick() + 1);
}

} // namespace GNN


