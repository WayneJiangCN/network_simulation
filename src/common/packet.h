#ifndef __COMMON_PACKET_H__
#define __COMMON_PACKET_H__

#include <cstdint>
#include <vector>
#include "common/common.h"
// 简单的数据包类，只用于数据搬运
namespace GNN
{
class DataPacket {
private:
    addr_t addr;                    // 内存地址
    size_t size;                      // 数据大小
    std::vector<uint32_t> data;        // 数据内容
    bool is_write;                     // 是否为写操作

public:
    // 构造函数
    DataPacket(addr_t a =0 , size_t s =0 , bool read = true) 
        : addr(a), size(s), is_write(read) {}
    
    // 析构函数
    ~DataPacket() = default;
    
    // 获取地址
    addr_t getAddr() const { return addr; }
    
    // 获取大小
    size_t getSize() const { return size; }
    
    // 获取数据
    const std::vector<uint32_t>& get_data() const { return data; }
    
    // 是否为读操作
    bool isRead() const { return !is_write; }
      // 是否为写操作
    bool isWrite() const { return is_write; }
    // 设置数据
    void setData(const std::vector<uint32_t>& d) {
        data = d;
    }
    
    // 设置地址
    void setAddr(addr_t _addr) { addr = _addr; }
    // 设置读写操作
    void setWrite(addr_t _is_write) { is_write = _is_write; }
    // 设置大小
    void setSize(size_t s) { size = s; }
};

// 简单的指针类型
typedef DataPacket* PacketPtr;

// 简单的队列类
class PacketQueue {
private:
    std::vector<PacketPtr> packets;

public:
    // 构造函数
    PacketQueue() = default;
    
    // 析构函数
    ~PacketQueue() = default;
    
    // 添加数据包
    void push(PacketPtr packet) {
        packets.push_back(packet);
    }
    
    // 获取数据包
    PacketPtr pop() {
        if (packets.empty()) return nullptr;
        PacketPtr packet = packets.front();
        packets.erase(packets.begin());
        return packet;
    }
    
    // 检查是否为空
    bool empty() const {
        return packets.empty();
    }
    
    // 获取大小
    size_t size() const {
        return packets.size();
    }
    
    // 清空队列
    void clear() {
        packets.clear();
    }
    
    // 查看队首数据包（不移除）
    PacketPtr peek() const {
        if (packets.empty()) return nullptr;
        return packets.front();
    }

    public:
     // 创建读数据包
    static PacketPtr create_read_packet(addr_t addr, size_t size) {
        return new DataPacket(addr, size, true);
    }
    
    // 创建写数据包
    static PacketPtr create_write_packet(addr_t addr, const std::vector<uint32_t>& data) {
        auto packet = new DataPacket(addr, data.size(), false);
        packet->setData(data);
        return packet;
    }
    
    // 释放数据包
    static void free_packet(PacketPtr packet) {
        delete packet;
    }
    
    // 批量创建读数据包
    static std::vector<PacketPtr> create_batch_read_packets(
        const std::vector<std::pair<uint64_t, size_t>>& requests) {
        std::vector<PacketPtr> packets;
        for (const auto& req : requests) {
            packets.push_back(create_read_packet(req.first, req.second));
        }
        return packets;
    }
    
    // 批量释放数据包
    static void free_packets(const std::vector<PacketPtr>& packets) {
        for (auto packet : packets) {
            delete packet;
        }
    }
};

// 简单的数据包管理器类
class PacketManager {
public:
    // 创建读数据包
    static PacketPtr create_read_packet(addr_t addr, size_t size) {
        return new DataPacket(addr, size, 0);
    }
    
    // 创建写数据包
    static PacketPtr create_write_packet(addr_t addr, const std::vector<uint32_t>& data) {
        auto packet = new DataPacket(addr, data.size(), 1);
        packet->setData(data);
        return packet;
    }
    
    // 释放数据包
    static void free_packet(PacketPtr packet) {
        delete packet;
    }
    
    // 批量创建读数据包
    static std::vector<PacketPtr> create_batch_read_packets(
        const std::vector<std::pair<uint64_t, size_t>>& requests) {
        std::vector<PacketPtr> packets;
        for (const auto& req : requests) {
            packets.push_back(create_read_packet(req.first, req.second));
        }
        return packets;
    }
    
    // 批量释放数据包
    static void free_packets(const std::vector<PacketPtr>& packets) {
        for (auto packet : packets) {
            delete packet;
        }
    }
};
}

#endif // __SIMPLE_DATA_PACKET_HH__