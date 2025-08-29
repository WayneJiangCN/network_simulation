#ifndef DRAMSIM3_WRAPPER
#define DRAMSIM3_WRAPPER
#include "buffer/buffer.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <functional>
#include <unordered_map>
#include "define.h"
#include "memory_system.h"
#include "common/object.h"
#include "common/debug.h"
#include "event/eventq.h"
#include "common/common.h"
#define QUEUE_SIZE 64
namespace GNN
{
    class Buffer; // 前向声明
    class dramsim3_wrapper : public SimObject
    {
    private:
        unsigned int depth;
        unsigned int burst_length;
        unsigned int bandwidth;
        double frequency;

        std::ofstream trace_out_file_;

        bool vld4repeate_ch[CHANNEL_NUM][64];
        bool channle_vld[CHANNEL_NUM] = {false};

        bool is_ch_rd_send[CHANNEL_NUM] = {false};
        bool is_ch_wr_send[CHANNEL_NUM] = {false};

        // 事件驱动集成：记录每个请求地址等待的Buffer
        std::unordered_map<uint64_t, Buffer *> waitingAddrToBuf;

        // 多通道回调
        std::vector<std::function<void(addr_t,data_t)>> read_callbacks;
        std::vector<std::function<void(addr_t,data_t)>> write_callbacks;

    public:
        int cycle_num;
        void init() override;
        dramsim3::MemorySystem *memory_system_1;

        dramsim3_wrapper(const std::string &config_file, const std::string &output_dir, const std::string &trace_out_file) : SimObject("dramsim3_wrapper"), tickEvent([this]
                                                                                                                                                                      { tick(); }, name())
        {
            memory_system_1 = (new dramsim3::MemorySystem(config_file, output_dir,
                                                          std::bind(&dramsim3_wrapper::global_read_callback, this, std::placeholders::_1),
                                                          std::bind(&dramsim3_wrapper::global_write_callback, this, std::placeholders::_1)));
            burst_length = memory_system_1->GetBurstLength();
            bandwidth = memory_system_1->GetQueueSize();
            frequency = 1 / (memory_system_1->GetTCK());
            std::cout << "burst_length:" << burst_length << " bandwidth:" << bandwidth << " frequency:" << frequency << std::endl;
            for (int i = 0; i < CHANNEL_NUM; i++)
            {
                for (int j = 0; j < 64; j++)
                {
                    vld4repeate_ch[i][j] = false;
                }
            }
            read_callbacks.resize(CHANNEL_NUM);
            write_callbacks.resize(CHANNEL_NUM);
        }
        void global_read_callback(uint64_t addr)
        {
            data_t data = 0;
            // std::cout << "[Wrapper]    回调函数被触发！::" << addr<<std::endl;
            int ch = this->get_channel(addr);
            if (read_callbacks[ch])
            {
                //    std::cout << "[Wrapper]    回调函数被触发！ch:" << ch<<std::endl;
                read_callbacks[ch](addr,data);
            }
        }

        void global_write_callback(uint64_t addr)
        {
           data_t data = 0;
            int ch = this->get_channel(addr);
            if (write_callbacks[ch])
            {
                write_callbacks[ch](addr,data);
            }
        }
        ~dramsim3_wrapper()
        {
        }

        // 注册回调
        void set_read_callback(int channel, std::function<void(addr_t,data_t)> cb)
        {
            if (channel >= 0 && channel < CHANNEL_NUM)
                read_callbacks[channel] = cb;
        }
        void set_write_callback(int channel, std::function<void(addr_t,data_t)> cb)
        {
            if (channel >= 0 && channel < CHANNEL_NUM)
                write_callbacks[channel] = cb;
        }

        void WriteCallBack(uint64_t addr)
        {
        }

        void request_read(uint64_t addr)
        {

            int ch = this->get_channel(addr);
            std::cout << "[Wrapper]    回调函数被触发！" << std::endl;
            // 1. 查表找到等待该地址的Buffer
            auto it = waitingAddrToBuf.find(addr);
            if (it != waitingAddrToBuf.end() && it->second)
            {
                Buffer *buf = it->second;

                int data = rand() % 100;
                // buf->DramDataReady(addr, data);
                // 3. 清理登记
                waitingAddrToBuf.erase(it);
            }
            else
            {
                // 没有登记，可能是bug或多余回调
                std::cerr << "[dramsim3_wrapper] Warning: No Buffer waiting for addr " << addr << std::endl;
            }

            //  std::cout << "read_call_back:" << "addr:" << addr << ", ch:" << ch << " clk:" << cycle_g << std::endl;
            if (!channle_vld[ch])
            {
                channle_vld[ch] = true;
            }
            else
            {
                bool is_send = false;
                for (int i = 0; i < 128; i++)
                {
                    if (!vld4repeate_ch[ch][i])
                    {
                        vld4repeate_ch[ch][i] = true;
                        is_send = true;
                        break;
                    }
                }
                if (!is_send)
                {
                    throw std::invalid_argument("repeate full");
                }
            }
        }

        EventFunctionWrapper tickEvent;

        void print_stats();
        void reset_stats();
        bool can_accept(uint64_t addr, bool is_write);
        void send_request(uint64_t addr, bool is_write); // 已被替换

        unsigned int get_busrt_length() const;
        unsigned int get_bandwidth() const;
        double get_frequency() const;
        unsigned int get_channel(address_t address) const;

        unsigned int validate_dram_reads(address_t *read_address);

        void tick();
    };
}
#endif // DRAMSIM3_WRAPPER_H
