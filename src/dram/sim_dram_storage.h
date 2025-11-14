/*
 * @Author: wayne 1448119477@qq.com
 * @Date: 2025-09-07 19:34:34
 * @LastEditors: wayne 1448119477@qq.com
 * @LastEditTime: 2025-11-12 23:21:58
 * @FilePath: /sim_v3/src/dram/sim_dram_storage.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef SIM_DRAM_STORAGE_H
#define SIM_DRAM_STORAGE_H

#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include "common/packet.h"
#include "common/common.h"
#include "common/file_read.h"
#include "common/define.h"
namespace GNN
{
  extern uint64_t storage_addr_max;
  extern uint64_t storage_number;
  // 简单的4GB DRAM模拟存储，支持按 burst=64 entries 进行读写
  class SimDramStorage : public FileReader
  {
  public:
    // 声明已由下面的内联定义提供，避免重复的同签名构造函数

    static constexpr uint64_t kCapacityBytes = 1ULL * 1024ULL * 1024ULL * 4 * 1024ULL; // 4GB * 1023ULL * 1024ULL
    static constexpr storage_t kBurstEntries = 32;                                     // 每个burst包含16个uint16_t

    uint64_t base_addr;            // 起始地址（字节）
    uint64_t capacity_bytes;       // 容量（字节）
    std::vector<uint16_t> storage; // 存储单元，按16bit元素存放

    inline bool inRange(addr_t addr, storage_t bytes) const
    {
      if (addr < base_addr)
        return false;
      uint64_t offset = addr - base_addr;
      return offset + bytes <= capacity_bytes;
    }

    inline uint64_t indexOf(addr_t addr) const
    {
      return (addr - base_addr) / sizeof(storage_t);
    }

public:
  SimDramStorage(uint64_t base = 0,const std::string& base_path = "./data/",const std::string& data_file_suffix = ".txt")
      : FileReader(TOTAL_SLICE_NUM_CFG, TOTAL_INST_NUM_CFG), base_addr(base), capacity_bytes(kCapacityBytes) {
    setDataFilePathTemplate(base_path, data_file_suffix);
    // 为模拟DRAM分配存储空间（以16位元素为单位）
    const uint64_t total_words = capacity_bytes / sizeof(storage_t);
    storage.resize(total_words, 0);
    D_INFO("SIM_DRAM_STORAGE", "Total words: %lld",total_words);
  }
  uint64_t total_words_num = 0;
  void readDataFile()  {
    readDataResult result;
    uint64_t total_words = 0;
    for (int inst_index = 0; inst_index < TOTAL_INST_NUM_CFG; inst_index++) {
    
     result = readBitmapData(inst_index);
     for (int nbr_index = 0; nbr_index < result.current_vertex_num; nbr_index++) {
      storage[total_words+ nbr_index] = result.data[nbr_index];
     }
     total_words += result.current_vertex_num;
    }
    D_INFO("SIM_DRAM_STORAGE", "Read data file successfully");
    D_INFO("SIM_DRAM_STORAGE", "Total words: %lld",total_words);
  }

  // 读取layer_0文件夹下所有子文件夹的数据
  uint64_t  readLayer0AllFoldersData(const std::string& layer0_path) {
    D_INFO("SIM_DRAM_STORAGE", "Reading all folders from layer_0: %s", layer0_path.c_str());
    std::map<std::string, std::vector<std::vector<storage_t>>> folder_data_map = readLayer0AllFolders(layer0_path);
    
    uint64_t total_files = 0;
    uint64_t total_data_points = 0;
    for (const auto& pair : folder_data_map) {
      total_files += pair.second.size();
      uint64_t folder_data_points = 0;
      for (const auto& file_data : pair.second) {
          for(int word_index = 0; word_index < file_data.size(); word_index++) {
            storage[total_words_num] = file_data[word_index];
            total_words_num++;
           
        }
        
        folder_data_points += file_data.size();
        total_data_points += file_data.size();
      }
      D_INFO("SIM_DRAM_STORAGE", "Folder %s: %d files, total data points: %lld", 
             pair.first.c_str(), pair.second.size(), folder_data_points);
             D_INFO("SIM_DRAM_STORAGE", "Total burst: %lld",total_data_points/32);
    }
    storage_addr_max = total_data_points*sizeof(storage_t);
  storage_number= total_data_points;
    
    D_INFO("SIM_DRAM_STORAGE", "Read layer_0 data successfully");
    D_INFO("SIM_DRAM_STORAGE", "Total folders: %d, Total files: %lld, Total data points: %lld", 
           folder_data_map.size(), total_files, total_data_points);
    
    return total_data_points/(BURST_NUM); // 返回总数据点数除以32，即总burst数
  }
// 读取指定文件夹中的所有txt文件
std::vector<std::vector<storage_t>> readFolderData(const std::string& folder_path) const {
  std::vector<std::vector<storage_t>> all_data;
  DIR* dir = opendir(folder_path.c_str());
  if (dir == nullptr) {
      D_ERROR("FILE_READ", "Cannot open directory: %s", folder_path.c_str());
      return all_data;
  }

  std::vector<std::string> file_names;
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
      std::string file_name = entry->d_name;
      // 只处理.txt文件
      if (file_name.length() > 4 && file_name.substr(file_name.length() - 4) == ".txt") {
          file_names.push_back(file_name);
      }
  }
  closedir(dir);

  // 对文件名进行排序，按照row_col的数值顺序排序（0_0, 0_1, 0_2, ..., 0_10, ...）
  std::sort(file_names.begin(), file_names.end(), compareFileNameByRowCol);

  // 读取每个文件
  for (const auto& file_name : file_names) {
      std::string full_path = folder_path + "/" + file_name;
      std::vector<storage_t> file_data = readBitmapALLROWDataFromFile(full_path);

      if (!file_data.empty()) {
          all_data.push_back(file_data);
          D_INFO("FILE_READ", "Read file: %s, data size: %d", file_name.c_str(), file_data.size());
      }
  }

  D_INFO("FILE_READ", "Read %d files from folder: %s", all_data.size(), folder_path.c_str());
  return all_data;
}
  // 单包写：使用PacketPtr中的地址与data
  bool writePacket(PacketPtr pkt) {
    if (!pkt || !pkt->isWrite()) return false;
    addr_t addr = pkt->getAddr();
    const auto &data = pkt->getData();
    storage_t bytes = data.size() * sizeof(storage_t);
    if (!inRange(addr, bytes)) return false;
    uint64_t idx = indexOf(addr);
     for(storage_t i = 0; i < data.size(); ++i) {
      storage[idx + i] = data[i];
    }
    return true;
  }

    // 单包读：按packet size读取到packet.data
    bool readPacket(PacketPtr pkt)
    {
      if (!pkt || !pkt->isRead())
        return false;
      addr_t addr = pkt->getAddr();
      storage_t words = pkt->getSize();
      storage_t bytes = words * sizeof(storage_t);
      if (!inRange(addr, bytes))
        return false;
      uint64_t idx = indexOf(addr);
      std::vector<uint16_t> out(words);

      for (storage_t i = 0; i < words; ++i)
      {
        out[i] = storage[idx + i];
      }
      pkt->setData(out);
      return true;
    }

    // burst写：首地址 + 16个uint32_t
    bool writeBurst(addr_t base, const storage_t *data)
    {
      if (data == nullptr)
        return false;
      storage_t bytes = kBurstEntries * sizeof(storage_t);
      if (!inRange(base, bytes))
        return false;
      uint64_t idx = indexOf(base);
      for (storage_t i = 0; i < kBurstEntries; ++i)
      {
        storage[idx + i] = data[i];
      }
      return true;
    }

    // burst读：首地址 + 32个storage_t
    bool readBurst(addr_t base, storage_t *out) const
    {
      if (out == nullptr)
        return false;
      storage_t bytes = kBurstEntries * sizeof(storage_t);
      if (!inRange(base, bytes))
        return false;
      uint64_t idx = indexOf(base);
      for (storage_t i = 0; i < kBurstEntries; ++i)
      {
        out[i] = storage[idx + i];
      }
      return true;
    }
  };

  
} // namespace GNN

#endif // SIM_DRAM_STORAGE_H
