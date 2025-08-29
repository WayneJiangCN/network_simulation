
#ifndef UP_BUFFER_H
#define UP_BUFFER_H

#include "common/debug.h"
#include "common/object.h"
#include "common/packet.h"
#include "common/port.h"
#include "dram/dram_arb.h"
#include <string>
#include <vector>
namespace GNN
{
  class UpBuffer : public SimObject
  {
  public:

    GNN::dramsim3_wrapper *dramsim3_wrapper;
    void init() override;
    addr_t addr;
    static constexpr int num_ports = 8;
    // 上游模块的 RequestPort
    class UpRequestPort : public RequestPort
    {
      UpBuffer &owner;
      int port_id;

    public:
      UpRequestPort(const std::string &name, UpBuffer &_owner, int _port_id)
          : RequestPort(name), owner(_owner), port_id(_port_id) {}
      bool recvTimingResp(PacketPtr pkt) override
      {
        //  D_INFO("BUFFER", "Received response for addr:%d on port:%d", pkt->getAddr(), port_id);
        return owner.recvTimingResp(pkt, port_id);
      }
      void recvReqRetry() override { owner.sendRetryReq(port_id); }
    };
    // 端口获取
    Port &getPort(const std::string &if_name, int idx = -1) override
    {
      // 端口名如 "buf_side0" ~ "buf_side7"
      if (if_name.find("buf_side") == 0)
      {
        int bank = std::stoi(if_name.substr(8));
        if (bank >= 0 && bank < num_ports)
          return requestPorts[bank];
      }
      throw std::runtime_error("No such port");
    }
    UpBuffer(const std::string &name, GNN::dramsim3_wrapper *dramsim3_wrapper_,addr_t addr_init_=0);
    std::vector<std::deque<PacketPtr>> buffer_Data;
    unsigned int bufferdata_num[num_ports];
    // 发送请求到下游（DramArb）
    bool sendTimingReq(PacketPtr pkt, int port_id);

    // 接收下游响应
    bool recvTimingResp(PacketPtr pkt, int port_id);
    void sendRetryReq(int port_id);
    void send_data_2cal();

  private:
    void sendpkt_event();
     bool retryReq[num_ports];
     bool retryResp[num_ports];
     addr_t addr_init;
    int buf_size;
    std::vector<UpRequestPort> requestPorts;
    EventFunctionWrapper tickEvent;
    EventFunctionWrapper send_data_retryrespEvent;
  };
} // namespace GNN
#endif
