/*
 * @Author: wayne 1448119477@qq.com
 * @Date: 2025-09-07 19:34:34
 * @LastEditors: wayne 1448119477@qq.com
 * @LastEditTime: 2025-09-08 17:59:19
 * @FilePath: /sim_v3/src/dram/sim_dram_storage.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef SIM_DRAM_STORAGE_H
#define SIM_DRAM_STORAGE_H

#include <vector>
#include <cstdint>
#include <cstring>
#include "common/packet.h"
#include "common/common.h"

namespace GNN {

// 简单的4GB DRAM模拟存储，支持按 burst=64 entries 进行读写
class SimDramStorage {
private:
  static constexpr uint64_t kCapacityBytes = 4ULL * 1024ULL* 1024ULL; // 4GB * 1023ULL * 1024ULL
  static constexpr size_t kBurstEntries = 64;  // 每个burst包含64个uint32_t

  uint64_t base_addr;            // 起始地址（字节）
  uint64_t capacity_bytes;       // 容量（字节）
  std::vector<uint32_t> storage; // 存储单元，按32bit元素存放

  inline bool inRange(addr_t addr, size_t bytes) const {
    if (addr < base_addr) return false;
    uint64_t offset = addr - base_addr;
    return offset + bytes <= capacity_bytes;
  }

  inline uint64_t indexOf(addr_t addr) const {
    return (addr - base_addr) / sizeof(uint32_t);
  }

public:
  SimDramStorage(uint64_t base = 0)
      : base_addr(base), capacity_bytes(kCapacityBytes) {
    uint64_t total_entries = capacity_bytes / sizeof(uint32_t);
    storage.assign(static_cast<size_t>(total_entries), 0u);
  }

  // 单包写：使用PacketPtr中的地址与data
  bool writePacket(PacketPtr pkt) {
    if (!pkt || !pkt->isWrite()) return false;
    addr_t addr = pkt->getAddr();
    const auto &data = pkt->getData();
    size_t bytes = data.size() * sizeof(uint32_t);
    if (!inRange(addr, bytes)) return false;
    uint64_t idx = indexOf(addr);
    for (size_t i = 0; i < data.size(); ++i) {
      storage[idx + i] = data[i];
    }
    return true;
  }

  // 单包读：按packet size读取到packet.data
  bool readPacket(PacketPtr pkt) {
    if (!pkt || !pkt->isRead()) return false;
    addr_t addr = pkt->getAddr();
    size_t words = pkt->getSize();
    size_t bytes = words * sizeof(uint32_t);
    if (!inRange(addr, bytes)) return false;
    uint64_t idx = indexOf(addr);
    std::vector<uint32_t> out(words);
    for (size_t i = 0; i < words; ++i) {
      out[i] = storage[idx + i];
    }
    pkt->setData(out);
    return true;
  }

  // burst写：首地址 + 64个uint32_t
  bool writeBurst(addr_t base, const uint32_t *data64) {
    if (data64 == nullptr) return false;
    size_t bytes = kBurstEntries * sizeof(uint32_t);
    if (!inRange(base, bytes)) return false;
    uint64_t idx = indexOf(base);
    for (size_t i = 0; i < kBurstEntries; ++i) {
      storage[idx + i] = data64[i];
    }
    return true;
  }

  // burst读：首地址 + 64个uint32_t
  bool readBurst(addr_t base, uint32_t *out64) const {
    if (out64 == nullptr) return false;
    size_t bytes = kBurstEntries * sizeof(uint32_t);
    if (!inRange(base, bytes)) return false;
    uint64_t idx = indexOf(base);
    for (size_t i = 0; i < kBurstEntries; ++i) {
      out64[i] = storage[idx + i];
    }
    return true;
  }
};

} // namespace GNN

#endif // SIM_DRAM_STORAGE_H


