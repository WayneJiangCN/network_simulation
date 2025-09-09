#include <iostream>
#include <cstdint>
#include <vector>
#include <functional>
#include <cmath>
#include "common/debug.h"
#include "common/common.h"
#include "event/eventq.h"
#include "probe/probe.h"
#include "probe/Listenervoid.h"
#include "common/object.h"
#include "buffer/UpBuffer.h"
#include "dram/dram.h"
#include "dma/DmaBuffer.h"
#include "compute/ComputeModule.h"
#include "dram/dram_arb.h"
using namespace GNN;

class PrintEvent : public Event
{
public:
    PrintEvent(const std::string &msg, int id, EventBase::Priority prio = EventBase::Default_Pri)
        : Event(prio), message(msg), eventId(id) {}

    void process() override
    {
        std::cout << "[Tick " << when() << "] "
                  << "Event #" << eventId
                  << " (prio=" << priority() << "): "
                  << message << std::endl;
    }

private:
    std::string message;
    int eventId;
};

void forEachObject(void (SimObject::*mem_func)())
{
    for (auto *obj : SimObject::simObjectList)
    {
        (obj->*mem_func)();
    }
}

int main()
{
    // 基本配置（可按需调整）
    const int num_banks = 8;          // Bank 数
    const int dram_buf_size = 16;     // DramArb 内部缓冲深度
    const int num_upstreams = 4;      // 上游数量：3个UpBuffer + 1个DMA

    // DRAMsim3 配置
    const std::string config_file = "./DRAMsim3-master/configs/HBM2_4Gb_x128.ini";
    const std::string output_dir = ".";
    const std::string trace_out_file = "./output/trace_out_file.txt";

    // 全局事件队列与调试开关
    gSim = new EventQueue("main_queue");
    miniDebugLevel = GNN::DBG_INFO; // 只显示 info 及以上
    // miniDebugModules = {"DRAM", "BUFFER", "DRAM_ARB", "DRAM_SIM3", "DRAM_WRAPPER", "EVENTQ","DMA"};
 miniDebugModules = { "DRAM_SIM3","DMA","COMPUTE"};

    auto *wrapper = new dramsim3_wrapper(config_file, output_dir, trace_out_file);
    DramArb dramArb("dram_arb", dram_buf_size, num_upstreams);

    // 上游缓冲（人为分配不同起始地址，便于观察）
    UpBuffer up0("up_buffer_0", wrapper, 0);
    UpBuffer up1("up_buffer_1", wrapper, 16 * 1024);
    UpBuffer up2("up_buffer_2", wrapper, 32 * 1024);
    DmaBuffer dma("dma0", 0, 32, 8); // 每行32个32-bit word，8个bank
    ComputeModule compute("compute0", &dma, num_banks, /*poll_interval=*/1);
    // UpBuffer up3("up_buffer_3", wrapper, 48 * 1024);

    // 下游每个 bank 一个 DRAMsim3 实例
    std::vector<DRAMsim3 *> drams;
    drams.reserve(num_banks);
    for (int bank = 0; bank < num_banks; ++bank) {
        drams.push_back(new DRAMsim3("dramsim3_" + std::to_string(bank), bank, wrapper));
    }

    // 小工具：双向绑定
    auto bindTwoWay = [](Port &a, Port &b) {
        a.bind(b);
        b.bind(a);
    };

    // 端口绑定：每个 bank 绑定四个上游端口 + 一个下游 DRAM 端口
    for (int bank = 0; bank < num_banks; ++bank) {
        const std::string b = std::to_string(bank);
        bindTwoWay(up0.getPort("buf_side" + b), dramArb.getPort("response" + b + "_0"));
        bindTwoWay(up1.getPort("buf_side" + b), dramArb.getPort("response" + b + "_1"));
        bindTwoWay(up2.getPort("buf_side" + b), dramArb.getPort("response" + b + "_2"));
        bindTwoWay(dma.getPort("dma_side" + b), dramArb.getPort("response" + b + "_3"));
        // bindTwoWay(up3.getPort("buf_side" + b), dramArb.getPort("response" + b + "_3"));
        bindTwoWay(dramArb.getPort("request" + b), drams[bank]->getPort("mem_side"));
    }

    // 调用所有对象的 init 钩子，触发各自初始化逻辑
    forEachObject(&SimObject::init);

    // 测试 DMA 功能
    std::cout << "---- Testing DMA Functions ----" << std::endl;
    // 测试单 bank 指令
    // dma.enqueueBankCommand(0, 0x1000, 256);      // bank 0 从 0x1000 读取 256 字节
    // dma.enqueueBankCommand(1, 0x2000, 128);      // bank 1 从 0x2000 读取 128 字节
    
    // 测试全局指令（多 bank 并行）
    dma.startStreaming(0x0000,4*8*2);        // 从 0x0000 开始，burst数据，多 bank 并行读取
    
    // 等待一段时间后再次发送指令测试乒乓切换
    // 这里可以添加更多测试代码

    // 主循环：按事件推进
    std::cout << "---- Simulation Start ----" << std::endl;
    while (!gSim->empty() && gSim->getCurTick() < 1000) {
        gSim->serviceOne();
    }
    std::cout << "---- Simulation End ----" << std::endl;
    
    // 检查 DMA 乒乓缓冲区状态
    // std::cout << "---- DMA Buffer Status ----" << std::endl;
    // for (int bank = 0; bank < 8; ++bank) {
    //     bool ping_state = dma.getBankPingPongState(bank);
    //     size_t ping_size = dma.getBankBufferSize(bank, false);
    //     size_t pong_size = dma.getBankBufferSize(bank, true);
        
    //     std::cout << "Bank " << bank << ": 当前使用 " << (ping_state ? "PONG" : "PING") 
    //               << ", PING大小=" << ping_size << ", PONG大小=" << pong_size << std::endl;
    // }

    delete gSim;
    return 0;
}