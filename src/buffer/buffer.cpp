/*
 * @Author: wayne 1448119477@qq.com
 * @Date: 2025-08-29 15:43:34
 * @LastEditors: wayne 1448119477@qq.com
 * @LastEditTime: 2025-08-29 15:43:35
 * @FilePath: /sim_v3/src/buffer/buffer.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "buffer/buffer.h"
#include "common/packet.h"

namespace GNN
{

    Buffer::Buffer(const std::string &_name)
        : SimObject(_name), memPort(name() + ".dram_port", this), fetchEvent([this] { /* fetch logic */ }, _name)
    {
    }

    Buffer::~Buffer() {}

    void Buffer::init()
    {
        // 连接端口等初始化
        // 发送请求

    
        D_DEBUG("BUFFER", "Buffer: DRAM 背压，等待 retry\n");
        PacketPtr pkt = new DataPacket(0x1000, 42);
        if (memPort.sendTimingReq(pkt))
        {
        }
    }

    bool Buffer::recvTimingResp(PacketPtr pkt)
    {
        // 处理 DRAM 返回数据
        dataQueue.push(pkt);
        outstandingReqs--;
        // 可能需要唤醒等待的请求
        return true;
    }

    // MemSidePort 实现。
    bool Buffer::MemSidePort::recvTimingResp(PacketPtr pkt)
    {
        return buffer->recvTimingResp(pkt);
    }

    void Buffer::MemSidePort::recvReqRetry()
    {
        waitingRetry = false;
        // 重新尝试发送
        sendRetryResp();
    }

    void Buffer::MemSidePort::sendRetryResp()
    {
        if (waitingRetry)
            return;
        // 这里尝试发送响应，如果对方背压则等待
        // 伪代码: if (!peer->recvTimingResp(pkt)) waitingRetry = true;
    }

} // namespace GNN