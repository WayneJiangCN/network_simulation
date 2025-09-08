/*
 * Simple DMA upstream: exposes one port per DRAM bank.
 * Each bank holds one row of data and issues a write on init.
 */

#ifndef GNN_DMABUFFER_H_
#define GNN_DMABUFFER_H_

#include "common/object.h"
#include "common/packet.h"
#include "common/port.h"
#include "event/eventq.h"
#include <deque>
#include <string>
#include <vector>

namespace GNN {

class DmaBuffer : public SimObject {
public:
  // One port per DRAM bank
  static constexpr int num_ports = 8;

  // row_words: number of 32-bit words per row
  DmaBuffer(const std::string &name, addr_t base_addr, int row_words);

  void init() override;

  // Ports API
  bool sendTimingReq(PacketPtr pkt, int port_id);
  bool recvTimingResp(PacketPtr pkt, int port_id);
  void sendRetryReq(int port_id);

  Port &getPort(const std::string &if_name, int idx = -1) override {
    // port names: "dma_side<bank>"
    if (if_name.find("dma_side") == 0) {
      int bank = std::stoi(if_name.substr(8));
      if (bank >= 0 && bank < num_ports)
        return requestPorts[bank];
    }
    throw std::runtime_error("No such port");
  }

private:
  // Base address for bank 0; bank i uses base + i*64
  addr_t base_addr_;
  int row_words_;

  // Per-port queues
  std::vector<std::deque<PacketPtr>> req_fifos_;

  // Ports
  class DmaRequestPort : public RequestPort {
    DmaBuffer &owner;
    int port_id;

  public:
    DmaRequestPort(const std::string &name, DmaBuffer &o, int id)
        : RequestPort(name), owner(o), port_id(id) {}
    bool recvTimingResp(PacketPtr pkt) override {
      return owner.recvTimingResp(pkt, port_id);
    }
    void recvReqRetry() override { owner.sendRetryReq(port_id); }
  };

  std::vector<DmaRequestPort> requestPorts;

  // Events
  EventFunctionWrapper tickEvent;

  // Behavior
  void try_send_event();

  // Flags
  bool retryReq[num_ports]{};
};

} // namespace GNN

#endif // GNN_DMABUFFER_H_


