/*
 * @Author: wayne 1448119477@qq.com
 * @Date: 2025-09-03 14:46:39
 * @LastEditors: wayne 1448119477@qq.com
 * @LastEditTime: 2025-09-03 16:18:29
 * @FilePath: /sim_v3/src/buffer/UpBuffer.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#ifndef GNN_UPBUFFER_H_
#define GNN_UPBUFFER_H_

#include "common/fifo.h"
#include "common/object.h"
#include "common/packet.h"
#include "common/port.h"
#include "dram/dram_arb.h"
#include "dram/dramsim3_wrapper.h"
#include "event/eventq.h"
#include "common/define.h"
#include <deque>
#include <string>
#include <vector>

namespace GNN {

class UpBuffer : public SimObject {
public:
  static constexpr int num_ports = 8;
  UpBuffer(const std::string &name, dramsim3_wrapper *dramsim3_wrapper_,
           addr_t addr_init_);
  void init() override;

  // 端口协议实现
  bool sendTimingReq(PacketPtr pkt, int port_id);
  bool recvTimingResp(PacketPtr pkt, int port_id);
  void sendRetryReq(int port_id);
  // 端口获取
  Port &getPort(const std::string &if_name, int idx = -1) override {
    // 端口名如 "buf_side0" ~ "buf_side7"
    if (if_name.find("buf_side") == 0) {
      int bank = std::stoi(if_name.substr(8));
      if (bank >= 0 && bank < num_ports)
        return requestPorts[bank];
    }
    throw std::runtime_error("No such port");
  }

private:
  // 地址生成
  addr_t addr;
  addr_t addr_init;

  // 与 DRAMsim3 交互
  GNN::dramsim3_wrapper *dramsim3_wrapper;

  // 每端口独立的上行/下行 FIFO（事件驱动）
  std::vector<EventDrivenFIFO<PacketPtr>> req_fifos_;  // 按端口存放待发送请求
  std::vector<EventDrivenFIFO<PacketPtr>> resp_fifos_; // 按端口存放待下发响应

  // 本地简单缓冲（过渡，可替换为计算端接口）
  std::vector<std::deque<PacketPtr>> buffer_Data;
  int bufferdata_num[num_ports]{};
  int buf_size{};

  // 端口
  class UpRequestPort : public RequestPort {
    UpBuffer &owner;
    int port_id;

  public:
    UpRequestPort(const std::string &name, UpBuffer &o, int id)
        : RequestPort(name), owner(o), port_id(id) {}
    bool recvTimingResp(PacketPtr pkt) override {
      return owner.recvTimingResp(pkt, port_id);
    }
    void recvReqRetry() override { owner.sendRetryReq(port_id); }
  };

  std::vector<UpRequestPort> requestPorts;

  // 事件
  EventFunctionWrapper tickEvent;
  EventFunctionWrapper send_2cal_Event;

  // 行为
  void sendpkt_event();
  void send_data_2cal();

  // 标志
  bool retryReq[num_ports]{};
  bool retryResp[num_ports]{};
};

} // namespace GNN

#endif // GNN_UPBUFFER_H_
