#include "buffer/UpBuffer.h"
#include "common/common.h"
#include "common/debug.h"
#include "common/object.h"
#include "compute/ComputeModule.h"
#include "dma/DmaBuffer.h"
#include "dram/dram.h"
#include "dram/dram_arb.h"
#include "event/eventq.h"
#include "probe/Listenervoid.h"
#include "probe/probe.h"
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>
using namespace GNN;

class PrintEvent : public Event {
public:
  PrintEvent(const std::string &msg, int id,
             EventBase::Priority prio = EventBase::Default_Pri)
      : Event(prio), message(msg), eventId(id) {}

  void process() override {
    std::cout << "[Tick " << when() << "] "
              << "Event #" << eventId << " (prio=" << priority()
              << "): " << message << std::endl;
  }

private:
  std::string message;
  int eventId;
};

void forEachObject(void (SimObject::*mem_func)()) {
  for (auto *obj : SimObject::simObjectList) {
    (obj->*mem_func)();
  }
}

int main() {

    // 矩阵分块：从上到下，再从左到右
    const int weight_row_num = 4096;   // 行数
    const int weight_col_num = 8;      // 列数
    const int block_row = 8;         // 每块的行数
    const int block_col = 2;           // 每块的列数

    const int num_blocks_row = (weight_row_num + block_row - 1) / block_row; // 向上取整
    const int num_blocks_col = (weight_col_num + block_col - 1) / block_col;

    // 记录每个块的起始和结束坐标（行优先，从上到下，再从左到右）
    struct Block {
        int row_start, row_end;
        int col_start, col_end;
    };
    std::vector<Block> blocks;
    for (int col = 0; col < weight_col_num; col += block_col) {
        for (int row = 0; row < weight_row_num; row += block_row) {
            Block blk;
            blk.row_start = row;
            blk.row_end = std::min(row + block_row, weight_row_num);
            blk.col_start = col;
            blk.col_end = std::min(col + block_col, weight_col_num);
            blocks.push_back(blk);
        }
    }
    // 其他配置
    const int all_weight_length = weight_row_num * weight_col_num; // 总长度
    const int token_length = 128;
    const int num_banks = 8;      // Bank 数
    const int dram_buf_size = 64; // DramArb 内部缓冲深度
    const int num_upstreams = 4;  // 上游数量：3个UpBuffer + 1个DMA
    const int weight_length = weight_row_num;

  // DRAMsim3 配置
  const std::string config_file = "./DRAMsim3-master/configs/HBM2_4Gb_x128.ini";
  const std::string output_dir = ".";
  const std::string trace_out_file = "./output/trace_out_file.txt";

  // 全局事件队列与调试开关
  gSim = new EventQueue("main_queue");
  miniDebugLevel = GNN::DBG_INFO; // 只显示 info 及以上
  // miniDebugModules = {"DRAM", "BUFFER", "DRAM_ARB", "DRAM_SIM3",
  // "DRAM_WRAPPER", "EVENTQ","DMA"};
  miniDebugModules = {"", "", "COMPUTE","Compute","main"};

  auto *wrapper = new dramsim3_wrapper(config_file, output_dir, trace_out_file);
  DramArb dramArb("dram_arb", dram_buf_size, num_upstreams);

  // 上游缓冲（人为分配不同起始地址，便于观察）
  UpBuffer up0("up_buffer_0", wrapper, 0);
  UpBuffer up1("up_buffer_1", wrapper, 16 * 1024);
  UpBuffer up2("up_buffer_2", wrapper, 32 * 1024);
  DmaBuffer dma("dma0", 0, block_row, 8); // 每行32个32-bit word，8个bank
  ComputeModule compute("compute0", num_banks);
  // UpBuffer up3("up_buffer_3", wrapper, 48 * 1024);

  // 下游每个 bank 一个 DRAMsim3 实例
  std::vector<DRAMsim3 *> drams;
  drams.reserve(num_banks);
  for (int bank = 0; bank < num_banks; ++bank) {
    drams.push_back(
        new DRAMsim3("dramsim3_" + std::to_string(bank), bank, wrapper));
  }

  // 小工具：双向绑定
  auto bindTwoWay = [](Port &a, Port &b) {
    a.bind(b);
    b.bind(a);
  };

  // 端口绑定：每个 bank 绑定四个上游端口 + 一个下游 DRAM 端口
  for (int bank = 0; bank < num_banks; ++bank) {
    const std::string b = std::to_string(bank);
    bindTwoWay(up0.getPort("buf_side" + b),
               dramArb.getPort("response" + b + "_0"));
    bindTwoWay(up1.getPort("buf_side" + b),
               dramArb.getPort("response" + b + "_1"));
    bindTwoWay(up2.getPort("buf_side" + b),
               dramArb.getPort("response" + b + "_2"));
    bindTwoWay(dma.getPort("dma_side" + b),
               dramArb.getPort("response" + b + "_3"));
    // 计算侧端口绑定：ComputeModule(请求端) ↔ DmaBuffer(响应端)
    bindTwoWay(compute.getPort("comp_side" + b),
               dma.getPort("comp_side" + b));
    // bindTwoWay(up3.getPort("buf_side" + b), dramArb.getPort("response" + b +
    // "_3"));
    bindTwoWay(dramArb.getPort("request" + b),
               drams[bank]->getPort("mem_side"));
  }

  // 调用所有对象的 init 钩子，触发各自初始化逻辑
  forEachObject(&SimObject::init);

  // 发起多个 DMA 命令，每个 block_row 就是一条指令
  DmaBuffer::DmaCommand cmd;

  // 为每个块发送一条DMA指令
  for (size_t i = 0; i <blocks.size() ; i++)
  {
    cmd.base_addr = 512 * i * block_row;  // 每个块的起始地址
    cmd.total_lines = block_row;        // 每个块的行数就是一条指令
    cmd.cmd_id = i + 1;                 // 每个命令有唯一ID
    cmd.completion_callback = [](uint64_t id){ D_INFO("main", "Command %lu completed fetch.", id); };
    dma.enqueueCommand(cmd);
  }

  // 主循环：按事件推进
  std::cout << "---- Simulation Start ----" << std::endl;
  while (!gSim->empty() && gSim->getCurTick() < 1000000) {
    gSim->serviceOne();
  }
  std::cout << "---- Simulation End ----" << std::endl;

  delete gSim;
  return 0;
}