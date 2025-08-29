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
    const int num_banks = 8;
    const int buf_size = 10;
    const int num_upstreams = 4;
    std::string config_file = "./DRAMsim3-master/configs/HBM2_4Gb_x128.ini";
    std::string output_dir = ".";
    std::string trace_out_file = "./output/trace_out_file.txt";
    gSim = new EventQueue("main_queue");
    dramsim3_wrapper *dramsim3_wrapper_ = new dramsim3_wrapper(config_file, output_dir, trace_out_file);
    miniDebugLevel = DBG_INFO;                                                                // 只显示 info 及以上
    miniDebugModules = {"DRAM", "BUFFER", "DRAM_ARB", "DRAM_SIM3", "DRAM_WRAPPER", "EVENTQ"}; // 只显示这两个模块的日志

    // 1. 创建 DramArb
    DramArb dram_arb("dram_arb", buf_size, num_upstreams);
    UpBuffer up_buffer_0("up_buffer_0", dramsim3_wrapper_, 0);
    UpBuffer up_buffer_1("up_buffer_1", dramsim3_wrapper_, 16384);
    UpBuffer up_buffer_2("up_buffer_2", dramsim3_wrapper_, 32768);
    UpBuffer up_buffer_3("up_buffer_3", dramsim3_wrapper_, 49152);
    std::vector<DRAMsim3 *> dramsim3_vec;
    for (int i = 0; i < num_banks; ++i)
    {
        dramsim3_vec.push_back(new DRAMsim3("dramsim3_" + std::to_string(i), i, dramsim3_wrapper_));
    }

    // 辅助函数：双向绑定两个端口
    auto bindPorts = [](Port& port1, Port& port2) {
        port1.bind(port2);
        port2.bind(port1);
    };
    // 绑定上游buffer到dram_arb的响应端口
    for (int i = 0; i < num_banks; ++i) {
        bindPorts(up_buffer_0.getPort("buf_side" + std::to_string(i)), 
                  dram_arb.getPort("response" + std::to_string(i) + "_0"));
        bindPorts(up_buffer_1.getPort("buf_side" + std::to_string(i)), 
                  dram_arb.getPort("response" + std::to_string(i) + "_1"));
        bindPorts(up_buffer_2.getPort("buf_side" + std::to_string(i)), 
                  dram_arb.getPort("response" + std::to_string(i) + "_2"));
        bindPorts(up_buffer_3.getPort("buf_side" + std::to_string(i)), 
                  dram_arb.getPort("response" + std::to_string(i) + "_3"));
        // 绑定dram_arb的请求端口到DRAM
        bindPorts(dram_arb.getPort("request" + std::to_string(i)), 
                  dramsim3_vec[i]->getPort("mem_side"));
    }
    forEachObject(&SimObject::init);
    // 端口绑定
    // for (int i = 0; i < 8; ++i)
    // {
    //     up_buffers[i]->getPort().bind(dram_arb.getPort("response" + std::to_string(i)));
    // }
    // 发送请求示例

    int addr = 0;
    std::cout << "---- Simulation Start ----" << std::endl;
    while (!gSim->empty() && gSim->getCurTick() < 1000)
    {

        gSim->serviceOne();
    }
    std::cout << "---- Simulation End ----" << std::endl;
    delete gSim;
    return 0;
}