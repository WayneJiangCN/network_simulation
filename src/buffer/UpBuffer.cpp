
#include "UpBuffer.h"
#include <iostream>
namespace GNN
{
  UpBuffer::UpBuffer(const std::string &name,
                     GNN::dramsim3_wrapper *dramsim3_wrapper_,addr_t addr_init_)
      : SimObject(name), addr(addr_init_),addr_init(addr_init_), dramsim3_wrapper(dramsim3_wrapper_),buffer_Data(8),
        tickEvent([this]
                  { sendpkt_event(); }, name + ".tickEvent"),
        send_data_retryrespEvent([this]
                                 { send_data_2cal(); }, name + ".send_data_respEvent")
  {
    buf_size = 0;
    for (int i = 0; i < num_ports; ++i)
    {
      bufferdata_num[i] =0;
      retryReq[i] = false;
      requestPorts.emplace_back(name + ".buf_side" + std::to_string(i), *this, i);
    }
  }
  void UpBuffer::init()
  {
    //  每个端口发一个包
    for (int i = 0; i < num_ports; ++i)
    {
      PacketPtr pkt = PacketManager::create_read_packet(addr, 64);
      addr += 64;
      D_INFO("BUFFER", "[UpBuffer] sendTimingReq addr:%d on port:%d",
             pkt->getAddr(), i);
      bool ok = sendTimingReq(pkt, i);
    }

    //  for (int i = 0; i < num_ports; ++i)
    // {
    //   PacketPtr pkt = PacketManager::create_read_packet(addr, 64);
    //   addr += 64;
    //   D_INFO("BUFFER", "[UpBuffer] sendTimingReq addr:%d on port:%d",
    //          pkt->getAddr(), i);
    //   bool ok = sendTimingReq(pkt, i);
    // }
  }
  void UpBuffer::sendpkt_event()
  {
    for (int bank = 0; bank < num_ports; bank++){
      if(bufferdata_num[bank] > buf_size){
        assert(true);
      }
      else{
        PacketPtr pkt = PacketManager::create_read_packet(addr + bank * 64, 64);
        D_INFO("BUFFER", "[UpBuffer] sendpkt_event addr:%d on port:%d",
               pkt->getAddr(), bank);
        bool ok = sendTimingReq(pkt, bank);
        if(!ok)
        retryReq[bank]= true;
      }
    } 
  }
  bool UpBuffer::sendTimingReq(PacketPtr pkt, int port_id)
  {
    // 通过端口发送请求到 DramArb
    bool ok = false;
    if (port_id >= 0 && port_id < num_ports)
      ok = requestPorts[port_id].sendTimingReq(pkt);
    return ok;
  }

  bool UpBuffer::recvTimingResp(PacketPtr pkt, int port_id)
  {
    assert(port_id >= 0 && port_id < num_ports);
    if (bufferdata_num[port_id] > buf_size)
    {
      retryResp[port_id] = true;
      buf_size = 2;
      return false;
    }
    buffer_Data[port_id].push_back(pkt);
    ++bufferdata_num[port_id];
    D_INFO("BUFFER", "[UpBuffer] Received response for addr:%d on port:%d",
           pkt->getAddr(), port_id);
    // if (!tickEvent.scheduled()){
    //   // D_DEBUG("BUFFER", "[UpBuffer] schedule tickEvent");
    //   schedule(tickEvent, curTick() + 1);
    // }
    return true;
  }
  void UpBuffer::send_data_2cal()
  {
    bool sent[num_ports] = {false};
    for (int bank = 0; bank < num_ports; bank++)
    {
      if (!buffer_Data[bank].empty())
      {
        PacketPtr pkt = buffer_Data[bank].front();
        // send data <---- if cal not busy
        buffer_Data[bank].pop_front();
        sent[bank] = true;
      }
    }
    bool all_sent = false;
    for (int bank = 0; bank < num_ports; bank++)
    {
      if (!sent[bank])
      {
        all_sent = true;
        break;
      }
    }
    if (!send_data_retryrespEvent.scheduled() && all_sent)
    {
      schedule(send_data_retryrespEvent, curTick() + 1);
    }
  }
  void UpBuffer::sendRetryReq(int port_id)
  {
  //需要判断哪个通道重新发送的请求，不然会多给下游请求
  PacketPtr pkt = PacketManager::create_read_packet(addr, 64);
  addr += 64;
  D_INFO("BUFFER", "[UpBuffer] sendTimingReq addr:%d on port:%d",
         pkt->getAddr(), port_id);
  bool ok = sendTimingReq(pkt,port_id);
  }
} // namespace GNN
