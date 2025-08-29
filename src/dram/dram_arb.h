/*
 * @Author: error: error: git config user.name & please set dead value or
 * install git && error: git config user.email & please set dead value or
 * install git & please set dead value or install git
 * @Date: 2025-07-29 20:49:31
 * @LastEditors: error: error: git config user.name & please set dead value or
 * install git && error: git config user.email & please set dead value or
 * install git & please set dead value or install git
 * @LastEditTime: 2025-08-28 21:44:02
 * @FilePath: /sim_v3/src/dram/dram_arb.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef DRAM_ARB_H
#define DRAM_ARB_H

#include "common/packet.h"
#include "common/port.h"
#include "dram/dramsim3.h"
#include "event/eventq.h"
#include <deque>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace GNN {

class DramArb : public SimObject {
public:
  static constexpr int num_banks = 8;
  static constexpr int num_up = 5;
  // 多上游数量可配置，默认1保持兼容
  DramArb(const std::string &_name, int buf_size, int num_upstreams_ = 1);
  void init() override {}
  // CAM表：addr -> 多个等待响应的请求
  std::unordered_map<addr_t, std::queue<PacketPtr>> outstandingReads[num_banks];
  unsigned int nbrOutstandingReads[num_banks];
  // 响应缓存队列（每个bank一个）
  std::deque<PacketPtr> respQueue[num_banks];

  // 输入缓冲：按 bank 和上游编号分布
  // 读/写各自维护一套，以便不同优先级策略
  std::vector<std::vector<std::deque<PacketPtr>>> readInBufs;  // [bank][up]
  std::vector<std::vector<std::deque<PacketPtr>>> writeInBufs; // [bank][up]

  // 端口
  class ArbResponsePort : public ResponsePort {
    DramArb &arb;
    int bank_id;
    int upstream_id;

  public:
    ArbResponsePort(const std::string &name, DramArb &_arb, int _bank_id,
                    int _up_id)
        : ResponsePort(name), arb(_arb), bank_id(_bank_id),
          upstream_id(_up_id) {}
    bool recvTimingReq(PacketPtr pkt) override {
      return arb.recvTimingReqUp(pkt, bank_id, upstream_id);
    }
    void recvRespRetry() override { arb.handleRespRetry(bank_id, upstream_id); }
  };
  class ArbRequestPort : public RequestPort {
    DramArb &arb;
    int bank_id;

  public:
    ArbRequestPort(const std::string &name, DramArb &_arb, int _bank_id)
        : RequestPort(name), arb(_arb), bank_id(_bank_id) {}
    bool recvTimingResp(PacketPtr pkt) override {
      return arb.recvTimingResp(pkt, bank_id);
    }
    void recvReqRetry() override { arb.handleReqRetry(bank_id); }
  };

  // 多上游响应端口：每个 bank 拥有 num_upstreams 个上游连接点
  std::vector<std::vector<ArbResponsePort>> responsePorts; // [bank][up]
  std::vector<ArbRequestPort> requestPorts;

  Port &getPort(const std::string &if_name, int idx = -1);

  // 老接口保留但不再被端口调用（兼容）：统一转发到上游0
  bool recvTimingReq(PacketPtr pkt, int bank_id);
  // 新接口：携带上游编号
  bool recvTimingReqUp(PacketPtr pkt, int bank_id, int upstream_id);
  bool recvTimingResp(PacketPtr pkt, int bank_id);

  void arbitrate();
  void scheduleArbEvent(int bank);
  void handleReqRetry(int bank_id);
  void handleRespRetry(int bank_id, int upstream_id);
  void accessAndRespond(int bank_id, PacketPtr pkt, int upstream_id);
  void sendResponse();

private:
  // 发送响应事件
  EventFunctionWrapper sendResponseEvent;
  EventFunctionWrapper arbEvent;
  std::deque<std::pair<PacketPtr, int>> responseQueue[num_banks];
  int buf_size;
  bool retryReq[num_banks][num_up];  // 记录每个bank是否等待发送请求的重试
  bool retryResp[num_banks][num_up]; // 记录每个bank是否等待发送响应的重试
  int num_upstreams;
  // 每个 bank 的读/写轮询起点，实现多上游公平仲裁
  std::vector<int> rrReadIdx;  // size = num_banks
  std::vector<int> rrWriteIdx; // size = num_banks
  // 记录每个读请求的来源上游（与 outstandingReads 同步）
  std::unordered_map<addr_t, std::queue<int>> outstandingUpstream[num_banks];

  // 私有辅助方法
  void initializeBasicState();
  void allocateInputBuffers();
  void createPorts(const std::string &name);
  Port &parseResponsePortName(const std::string &if_name, int idx);
  Port &parseRequestPortName(const std::string &if_name);
  bool arbitrateReadRequests(int bank);
  bool arbitrateWriteRequests(int bank);
  bool checkPendingRequests(int bank);
};

} // namespace GNN

#endif