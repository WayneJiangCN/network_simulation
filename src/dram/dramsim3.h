#ifndef __MEM_DRAMSIM3_HH__
#define __MEM_DRAMSIM3_HH__

#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

#include "dram/dramsim3_wrapper.h"
#include "common/port.h"
#include "probe/named.h"
#include "probe/probe.h"
#include "common/object.h"
#include "common/packet.h"
#include "event/eventq.h"
#include "common/debug.h"
namespace GNN
{
// DRAMsim3 内存控制器类
class DRAMsim3:  public SimObject{
  public:
  void init() override;
        Port &getPort(const std::string &if_name, int idx=-1) override
        {
            if (if_name == "mem_side")
                return port;
            throw std::runtime_error("No such port");
        }
  private:
  int channel_id;
    // 内存端口，负责流控，避免端口自身隐式创建无限存储
    class MemoryPort : public ResponsePort
    {
      private:
        DRAMsim3& mem;
      public:
        MemoryPort(const std::string& _namer,DRAMsim3& _memory);
      protected:
        void recvFunctional(PacketPtr pkt);
        bool recvTimingReq(PacketPtr pkt);
        void recvRespRetry() override;
    };

    // 回调函数
    std::function<void(uint64_t)> read_cb;
    std::function<void(uint64_t)> write_cb;

    // 实际的 DRAMsim3 封装
    dramsim3_wrapper* wrapper;

    // 端口是否在等待重试
    bool retryReq;
    // 是否等待发送响应的重试
    bool retryResp;
    // 记录 wrapper 启动时刻
    cycle_t startTick;
    // 记录每个地址未完成的读写事务队列
    std::unordered_map<addr_t, std::queue<PacketPtr> > outstandingReads;
    std::unordered_map<addr_t, std::queue<PacketPtr> > outstandingWrites;
    // 统计未完成的事务数，用于流控
    unsigned int nbrOutstandingReads;
    unsigned int nbrOutstandingWrites;
    // 响应队列，等待可以发送时返回
    std::deque<PacketPtr> responseQueue;

    unsigned int nbrOutstanding() const;
    // 事务完成后，生成响应包并返回
    void accessAndRespond(PacketPtr pkt);
    void sendResponse();
    // 发送响应事件
    EventFunctionWrapper sendResponseEvent;
    // 推进控制器一个时钟周期
    void tick();
    // 时钟事件
    EventFunctionWrapper tickEvent;
    // 上游 cache 需要此包直到返回 true，暂存待删除
    std::unique_ptr<DataPacket> pendingDelete;

  public:
    MemoryPort port; //对外绑定接口

    DRAMsim3(const std::string &name_, int channel, dramsim3_wrapper* wrapper);
    // 读完成回调
    void readComplete( addr_t addr,data_t data=0);
    // 写完成回调
    void writeComplete( addr_t addr,data_t data=0);

    void startup() ;
    void resetStats() ;

  protected:
    void recvFunctional(PacketPtr pkt);
    bool recvTimingReq(PacketPtr pkt);
    void recvRespRetry() ;
};
}
#endif // __MEM_DRAMSIM3_HH__
