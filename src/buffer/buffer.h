#pragma once
#include <queue>
#include <string>
#include "event/eventq.h"
#include "common/common.h"
#include "common/port.h"
#include "common/object.h"
#include "common/packet.h"
#include "common/debug.h"
namespace GNN
{

    class Buffer : public SimObject
    {
    public:
        Buffer(const std::string &_name);
        ~Buffer();

        void init() override;
        Port &getPort(const std::string &if_name, int idx=-1) override
        {
            if (if_name == "mem_side")
                return memPort;
            throw std::runtime_error("No such port");
        }

        // 端口
        class MemSidePort : public RequestPort
        {
            Buffer *buffer;
            bool waitingRetry = false;

        public:
            MemSidePort(const std::string &_name, Buffer *buf)
                : RequestPort(_name), buffer(buf), retryRespEvent([this]
                                                                  { sendRetryResp(); }, _name) {}
            bool recvTimingResp(PacketPtr pkt) override;
            void recvReqRetry() override;
            EventFunctionWrapper retryRespEvent;
            void sendRetryResp();
        };
        MemSidePort memPort;

        // Buffer容量管理
        size_t maxSize = 32;
        std::queue<PacketPtr> dataQueue;
        size_t outstandingReqs = 0;

        // 处理DRAM返回
        bool recvTimingResp(PacketPtr pkt);

    protected:
        std::string _name;
        EventFunctionWrapper fetchEvent;
    };

} // namespace GNN