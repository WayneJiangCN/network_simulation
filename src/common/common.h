#ifndef __COMMON_COMMON_H__
#define __COMMON_COMMON_H__
#include <iostream>
#include <cstdint>
#include <vector>
#include <functional>

namespace GNN
{


#define DRAM_MODE
using cycle_t = uint64_t;
using Tick = uint64_t;
using addr_t = uint64_t;
using data_t = uint64_t;
using ReadCallback = std::function<void(uint64_t)>;
using ComputeCallback = std::function<void(uint64_t)>;
extern cycle_t cycle_g;
#define DRAM_EPSILON 100


}
#endif