
#include "dram/dramsim3.h"

namespace GNN {
DRAMsim3::DRAMsim3(const std::string &name_, int channel,
                   dramsim3_wrapper *wrapper)
    : SimObject(name_), port(name() + ".port", *this), channel_id(channel),
      wrapper(wrapper), retryReq(false), retryResp(false), startTick(0),
      nbrOutstandingReads(0), nbrOutstandingWrites(0),
      sendResponseEvent([this] { sendResponse(); }, name()),
      tickEvent([this] { tick(); }, name()) {
  wrapper->set_read_callback(
      channel_id, [this](addr_t addr,data_t data=0) { this->readComplete(addr); });
  wrapper->set_write_callback(
      channel_id, [this](addr_t addr,data_t data=0) { this->writeComplete(addr); });
  // Register a callback to compensate for the destructor not
  // being called. The callback prints the DRAMsim3 stats.
  // registerExitCallback([this]() { wrapper->printStats(); });
}

void DRAMsim3::init() {
  if (!port.isConnected()) {
    D_ERROR("DRAM", "DRAMsim3 %s is unconnected!\n", name());
  }
  // startup();
}

void DRAMsim3::startup() {

  // D_DEBUG("DRAM","tickEvent,cycle:%d ", curTick());
}

void DRAMsim3::resetStats() {
  // wrapper->resetStats();
}

void DRAMsim3::sendResponse() {
  assert(!retryResp);
  assert(!responseQueue.empty());
  bool success = port.sendTimingResp(responseQueue.front());

  if (success) {
    responseQueue.pop_front();
  } else {
    retryResp = true;
  }
}

unsigned int DRAMsim3::nbrOutstanding() const // 返回所有outstanding的请求数
{
  return nbrOutstandingReads + nbrOutstandingWrites + responseQueue.size();
}

void DRAMsim3::tick() {

  if (retryReq) {
    retryReq = false;
    port.sendRetryReq();
  }
  //    D_DEBUG("DRAM","tickEvent,cycle:%d ", curTick());
  // schedule(tickEvent, curTick() + 1);
}

bool DRAMsim3::recvTimingReq(PacketPtr pkt) {
   D_DEBUG("DRAM_SIM3", "recvTimingReq:");
  // keep track of the transaction
  bool can_accept = wrapper->can_accept(pkt->getAddr(), pkt->isWrite());
  if (!pkt->isWrite()) {
    if (can_accept) {
      outstandingReads[pkt->getAddr()].push(pkt);
      ++nbrOutstandingReads;
    }
  } else {
    if (can_accept) {
      outstandingWrites[pkt->getAddr()].push(pkt);
      ++nbrOutstandingWrites;
      pendingDelete.reset(pkt);
    }
  }
  // D_DEBUG("DRAM_SIM3", "can_accept: %d", can_accept);
  if (can_accept) {
    wrapper->send_request(pkt->getAddr(), pkt->isWrite());
    return true;
  } else {
    schedule(tickEvent, curTick() + 1);
    retryReq = true;
    return false;
  }
}

void DRAMsim3::recvRespRetry() // 被调用上游
{
  // DPRINTF(DRAMsim3, "Retrying\n");
  // 接收数据的dramarb不可能忙
  assert(retryResp);
  retryResp = false;
  sendResponse();
}

void DRAMsim3::accessAndRespond(PacketPtr pkt) {
  // DPRINTF(DRAMsim3, "Access for address %lld\n", pkt->getAddr());

  Tick delay = 1;
  Tick time = curTick() + delay;
  responseQueue.push_back(pkt);
  if (!retryResp && !sendResponseEvent.scheduled()) {
    schedule(sendResponseEvent, time);
  }
}

void DRAMsim3::readComplete(addr_t addr,data_t data) {
  D_INFO("DRAM_SIM3", "[Recv DRAMSIM3],channel_id: %d,readComplete addr: %d",
          channel_id, addr);
  // get the outstanding reads for the address in question
  auto p = outstandingReads.find(addr);
  assert(p != outstandingReads.end());
  // first in first out, which is not necessarily true, but it is
  // the best we can do at this point
  PacketPtr pkt = p->second.front();
  p->second.pop();
  if (p->second.empty())
    outstandingReads.erase(p);

  // no need to check for drain here as the next call will add a
  // response to the response queue straight away
  assert(nbrOutstandingReads != 0);
  --nbrOutstandingReads;

  // perform the actual memory access
  accessAndRespond(pkt);
}

void DRAMsim3::writeComplete(addr_t addr,data_t data) {

  auto p = outstandingWrites.find(addr);
  assert(p != outstandingWrites.end());

  p->second.pop();
  if (p->second.empty())
    outstandingWrites.erase(p);
  assert(nbrOutstandingWrites != 0);
  --nbrOutstandingWrites;
}

DRAMsim3::MemoryPort::MemoryPort(const std::string &_name, DRAMsim3 &_memory)
    : ResponsePort(_name), mem(_memory) {}

bool DRAMsim3::MemoryPort::recvTimingReq(PacketPtr pkt) {
  // pass it to the memory controller

  return mem.recvTimingReq(pkt);
}

void DRAMsim3::MemoryPort::recvRespRetry() { mem.recvRespRetry(); }
} // namespace GNN
