
#include "UpBuffer.h"
#include <iostream>
namespace GNN
{
  UpBuffer::UpBuffer(const std::string &name,
                     GNN::dramsim3_wrapper *dramsim3_wrapper_, addr_t addr_init_)
      : SimObject(name), addr(addr_init_), addr_init(addr_init_),
        dramsim3_wrapper(dramsim3_wrapper_), buffer_Data(8),
        tickEvent([this]
                  { sendpkt_event(); }, name + ".tickEvent"),
        send_2cal_Event([this]
                        { send_data_2cal(); },
                        name + ".send_data_respEvent")
  {
    buf_size = 3;  //
    // 初始化端口、FIFO
    requestPorts.reserve(num_ports);
    req_fifos_.reserve(num_ports);
    resp_fifos_.reserve(num_ports);
    for (int i = 0; i < num_ports; ++i)
    {
      bufferdata_num[i] = 0;
      retryReq[i] = false;
      requestPorts.emplace_back(name + ".buf_side" + std::to_string(i), *this, i);
      req_fifos_.emplace_back(128);
      resp_fifos_.emplace_back(128);
    }
    // FIFO 回调设置：任一端口有数据可发/可下发则调度
    for (int i = 0; i < num_ports; ++i)
    {
      req_fifos_[i].setOnDataAvailable([this]
                                       {
      if (!tickEvent.scheduled())
        schedule(tickEvent, curTick() + 1); });
      resp_fifos_[i].setOnDataAvailable([this]
                                        {
      if (!send_2cal_Event.scheduled())
        schedule(send_2cal_Event, curTick() + 1); });
    }
  }
  void UpBuffer::init()
  {
    // 每个端口先发一个包：进入对应端口的 req FIFO

    int is_write = 1;

    if (is_write)
      for (int i = 0; i < 0; ++i)
      {
        if (i == 0)
        {
          for (size_t j = 0; j < 0; j++)
          {
            std::vector<storage_t> temp_data;
            temp_data.reserve(16);
            for (size_t k = 0; k < 16; k++)
            {
              temp_data.push_back(addr + j * 64 * 8 + k);
            }
            PacketPtr pkt =
                PacketManager::create_write_packet(addr + j * 64 * 8, temp_data);
            req_fifos_[i].push(pkt);
            D_INFO("BUFFER", "[UpBuffer] enqueue init addr:%d on port:%d",
                   pkt->getAddr(), i);
          }
        }
        else
        {
          std::vector<storage_t> temp_data;
          temp_data.reserve(16);
          for (size_t k = 0; k < 16; k++)
          {
            temp_data.push_back(addr + k);
          }
          PacketPtr pkt = PacketManager::create_write_packet(addr, temp_data);
          D_INFO("BUFFER", "[UpBuffer] enqueue init addr:%d on port:%d",
                 pkt->getAddr(), i);
          req_fifos_[i].push(pkt);
        }
        addr += 64;
      }
    else
    {
      for (int i = 0; i < 1; ++i)
      {
        if (i == 0)
        {
          for (size_t j = 0; j < 20; j++)
          {
            PacketPtr pkt =
                PacketManager::create_read_packet(addr + j * 64 * 8, 64);
            req_fifos_[i].push(pkt);
            D_INFO("BUFFER", "[UpBuffer] enqueue init addr:%d on port:%d",
                   pkt->getAddr(), i);
          }
        }
        else
        {
          PacketPtr pkt = PacketManager::create_read_packet(addr, 64);

          D_INFO("BUFFER", "[UpBuffer] enqueue init addr:%d on port:%d",
                 pkt->getAddr(), i);
          req_fifos_[i].push(pkt);
        }
        addr += 64;
      }
    }
  }
  int cout = 0;
  void UpBuffer::sendpkt_event()
  {
    int flag = 0;
    // 轮询所有端口的 req FIFO，尝试发送
    for (int port = 0; port < num_ports; ++port)
    {
      PacketPtr pkt;
      // 尽量清空该端口可发送项，直到下游拒绝或为空
      if (req_fifos_[port].pop(pkt))
      {
        flag = 1;
        D_INFO("BUFFER", "[UpBuffer] try send addr:%d on port:%d", pkt->getAddr(),
               port);
        bool ok = sendTimingReq(pkt, port);
        if (!ok)
        {
          // 下游忙，放回该端口的队尾，等待 recvReqRetry 再试
          req_fifos_[port].push(pkt);
          break;
        }
      }
      else
      {
        if (!tickEvent.scheduled())
        {
          schedule(tickEvent, curTick() + 1);
        }
      }
    }
    // if (cout < 3)
    //   if (!flag) {
    //     if (addr > 0)
    //       addr = addr - 64;
    //     for (int i = 0; i < 1; ++i) {
    //       if (i == 0) {
    //         for (size_t j = 0; j < 2; j++) {
    //           PacketPtr pkt =
    //               PacketManager::create_read_packet(addr + j * 64 * 8, 64);
    //           req_fifos_[i].push(pkt);
    //           D_INFO("BUFFER", "[UpBuffer] enqueue init addr:%d on port:%d",
    //                  pkt->getAddr(), i);
    //         }
    //       } else {
    //         PacketPtr pkt = PacketManager::create_read_packet(addr, 64);

    //         D_INFO("BUFFER", "[UpBuffer] enqueue init addr:%d on port:%d",
    //                pkt->getAddr(), i);
    //         req_fifos_[i].push(pkt);
    //       }
    //       addr += 64;
    //     }
    //     cout += 1;
    //   }
  }
  bool UpBuffer::sendTimingReq(PacketPtr pkt, int port_id)
  {
    bool ok = false;
    if (port_id >= 0 && port_id < num_ports)
      ok = requestPorts[port_id].sendTimingReq(pkt);
    return ok;
  }

  bool UpBuffer::recvTimingResp(PacketPtr pkt, int port_id)
  {
    assert(port_id >= 0 && port_id < num_ports);
    // 如果本地容量限制，先尝试缓存到该端口的 resp FIFO，满了则返回 false
    if (pkt->isWrite())
    {
      req_fifos_[port_id].push(pkt);
      return true;
    }
    if (resp_fifos_[port_id].full())
    {
      retryResp[port_id] = true;
      return false;
    }
    resp_fifos_[port_id].push(pkt);
    std::vector<storage_t> temp_data;
    temp_data = pkt->getData();

    D_INFO("BUFFER", "[UpBuffer] enqueue resp addr:%d on port:%d", pkt->getAddr(),
           port_id);
    std::cout << "Packet Data (addr: " << pkt->getAddr() << "): ";
    for (size_t j = 0; j < 16; j++)
    {
      std::cout << temp_data[j] << " ";
    }
    std::cout << "" << std::endl;
    return true;
  }
  void UpBuffer::send_data_2cal()
  {
    // 从各端口 resp FIFO 消费，模拟向计算侧下发
    for (int port = 0; port < num_ports; ++port)
    {
      PacketPtr pkt;
      while (resp_fifos_[port].pop(pkt))
      {
        D_INFO("BUFFER", "[UpBuffer] send resp addr:%d on port:%d",
               pkt->getAddr(), port);
        delete pkt;
      }
    }
    // 若任一端口仍有数据，持续调度
    bool has_pending = false;
    for (int port = 0; port < num_ports; ++port)
    {
      if (!resp_fifos_[port].empty())
      {
        has_pending = true;
        break;
      }
    }
    if (has_pending && !send_2cal_Event.scheduled())
    {
      schedule(send_2cal_Event, curTick() + 1);
    }
  }
  void UpBuffer::sendRetryReq(int port_id)
  {
    // 下游通知该端口可重试。触发发送事件。
    if (!tickEvent.scheduled())
      schedule(tickEvent, curTick() + 1);
  }
} // namespace GNN
