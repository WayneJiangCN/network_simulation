#include "dma/DmaBuffer.h"
#include "common/packet.h" // 假设PacketManager用于创建数据包

namespace GNN {
    // 为静态成员变量提供定义
    constexpr int DmaBuffer::num_ports;
    constexpr int DmaBuffer::addr_stride;

DmaBuffer::DmaBuffer(const std::string &name, addr_t base_addr, int row_words,
                     int active_banks)
    : SimObject(name), base_addr_(base_addr), row_words_(row_words),
      active_banks_(std::min(active_banks, num_ports)), trans_state_(IDLE),
      addr_stride_(active_banks * addr_stride),
      tickEvent([this] { tick(); }, name + ".tickEvent"),
      realseEvent([this] { realse(); }, name + ".realseEvent") {
    
    // 初始化端口和请求队列
    requestPorts.reserve(num_ports);
    req_fifos_.resize(num_ports);
    for (int i = 0; i < num_ports; ++i) {
        requestPorts.emplace_back(name + ".dma_side" + std::to_string(i), *this, i);
    }
    
    // 初始化每个活动bank的控制器和缓冲区
    bank_controllers_.resize(active_banks_);
    for (int i = 0; i < active_banks_; ++i) {
        bank_controllers_[i].buffers[0].data.reserve(row_words_); // Ping buffer
        bank_controllers_[i].buffers[1].data.reserve(row_words_); // Pong buffer
    }
}

void DmaBuffer::init() {
    // if (!realseEvent.scheduled()) {
    //     schedule(realseEvent, curTick() + 45);
    // }
    // 初始化时保持空闲，等待 startStreaming 指令
}

void DmaBuffer::startStreaming(addr_t base_addr, int total_lines) {
    if (trans_state_ != IDLE) {
        // 如果DMA正忙，则忽略新的传输请求
        D_WARN("DMA", "DMA is busy, ignoring new streaming request.");
        return;
    }
    global_cmd_.base_addr = base_addr;
    global_cmd_.total_lines_remaining = total_lines;
    // 计算最终地址，用于判断传输何时结束
    global_cmd_.final_addr = base_addr + static_cast<addr_t>(total_lines) * addr_stride;

    trans_state_ = CONFIG;
    D_INFO("DMA", "Starting new stream: base_addr=%#x, lines=%d", base_addr, total_lines);
    schedule_tick_if_needed();
}
void DmaBuffer::realse(){
  for (size_t i = 0; i < active_banks_; i++)
  {
    releaseBankBuffer(i,getReadableBufferIndex(i));
  }
  
}
void DmaBuffer::tick() {
    // 1. 状态机驱动核心逻辑
    switch (trans_state_) {
        case IDLE:
            // 空闲状态，不执行任何操作
            return;

        case CONFIG: {
            // 配置阶段：计算每个bank的初始读取地址
            int ori_ch = (global_cmd_.base_addr % 0x100) / 0x40; // 假设的起始通道计算
            for (int i = 0; i < active_banks_; ++i) {
                addr_t dram_bias_addr = i >= ori_ch
                    ? (i - ori_ch) * addr_stride
                    : (i - ori_ch + active_banks_) * addr_stride;
                bank_rd_addr_[i] = global_cmd_.base_addr + dram_bias_addr;

                // 启动第一个空闲缓冲区（通常是Ping）的填充
                auto& controller = bank_controllers_[i];
                int write_idx = controller.current_write_idx;
                if (controller.buffers[write_idx].state == BufferState::EMPTY) {
                    controller.buffers[write_idx].state = BufferState::FILLING;
                    controller.buffers[write_idx].words_written = 0;
                    controller.buffers[write_idx].data.clear();
                    bank_transfer_active_[i] = true;
                }
            }
            trans_state_ = STREAMING;
            break;
        }

        case STREAMING: {
            // 流式传输阶段
            if (global_cmd_.total_lines_remaining <= 0) {
                // 所有行的请求都已发出，检查是否所有缓冲区都已处理完毕
                 bool all_buffers_empty = true;
                 for(int i=0; i < active_banks_; ++i){
                     if(bank_controllers_[i].buffers[0].state != BufferState::EMPTY ||
                        bank_controllers_[i].buffers[1].state != BufferState::EMPTY) {
                         all_buffers_empty = false;
                         break;
                     }
                 }
                 if(all_buffers_empty) {
                     trans_state_ = IDLE; // 任务完成，返回IDLE
                     D_INFO("DMA", "Streaming finished.");
                 }
                 break;
            }

            // 为每个活动的、未暂停的bank生成DRAM读请求
            for (int i = 0; i < active_banks_; ++i) {
                if (bank_transfer_active_[i] && !bank_controllers_[i].stalled) {
                    // 假设每次读取64字节 (16个32位字)
                    PacketPtr read_pkt = PacketManager::create_read_packet(bank_rd_addr_[i], 16);
                    req_fifos_[i].push_back(read_pkt);
                    
                    // 更新地址以供下个周期使用
                    bank_rd_addr_[i] += addr_stride_;
                    if (bank_rd_addr_[i] >= global_cmd_.final_addr) {
                        bank_transfer_active_[i] = false; // 此bank的地址已全部发出
                    }
                }
            }
            global_cmd_.total_lines_remaining--;
            break;
        }
    }

    // 2. 发送所有队列中的请求到底层内存系统
    for (int i = 0; i < num_ports; ++i) {
        if (!req_fifos_[i].empty()) {
            PacketPtr pkt = req_fifos_[i].front();
            // 尝试发送，如果成功，则从队列中移除
            if (requestPorts[i].sendTimingReq(pkt)) {
                req_fifos_[i].pop_front();
            }
        }
    }

    // 3. 如果DMA仍在工作，则调度下一个时钟周期的tick事件
    if (trans_state_ != IDLE&& !tickEvent.scheduled()) {
        schedule(tickEvent, curTick() + 1);
    }
}

bool DmaBuffer::recvTimingResp(PacketPtr pkt, int port_id) {
    if (pkt->isRead()) {
        auto& controller = bank_controllers_[port_id];
        int write_idx = controller.current_write_idx;
        auto& buffer = controller.buffers[write_idx];

        if (buffer.state == BufferState::FILLING) {
            const auto& data = pkt->getData();
            buffer.data.insert(buffer.data.end(), data.begin(), data.end());
            buffer.words_written += data.size();  //16

            // 检查当前缓冲区是否已填满
            if (buffer.words_written >= row_words_) {
                buffer.state = BufferState::FULL;
                D_INFO("DMA", "Bank %d Buffer %d is FULL.", port_id, write_idx);

                // --- 自动切换核心逻辑 ---
                int next_write_idx = (write_idx + 1) % 2;
                if (controller.buffers[next_write_idx].state == BufferState::EMPTY) {
                    // 下一个缓冲区空闲，立即切换并开始填充
                    controller.current_write_idx = next_write_idx;
                    auto& next_buffer = controller.buffers[next_write_idx];
                    next_buffer.state = BufferState::FILLING;
                    next_buffer.words_written = 0;
                    next_buffer.data.clear();
                    controller.stalled = false;
                    D_INFO("DMA", "Bank %d auto-switched to Buffer %d.", port_id, next_write_idx);
                } else {
                    // 下一个缓冲区仍被下游占用，暂停此bank的数据请求
                    controller.stalled = true;
                    D_WARN("DMA", "Bank %d stalled, waiting for Buffer %d to be released.", port_id, next_write_idx);
                }
            }
        }
    }
    delete pkt;
    return true;
}

void DmaBuffer::releaseBankBuffer(int bank_id, int buffer_idx) {
    assert(bank_id >= 0 && bank_id < active_banks_);
    assert(buffer_idx == 0 || buffer_idx == 1);

    auto& controller = bank_controllers_[bank_id];
    if (controller.buffers[buffer_idx].state == BufferState::FULL) {
        controller.buffers[buffer_idx].state = BufferState::EMPTY;
        D_INFO("DMA", "Bank %d Buffer %d released by consumer.", bank_id, buffer_idx);
        
        // 如果DMA正因为等待此buffer而暂停，现在唤醒它
        if (controller.stalled) {
            int next_write_idx = (controller.current_write_idx + 1) % 2;
            if (next_write_idx == buffer_idx) { // 确认是等待的那个buffer被释放了
                controller.stalled = false;
                controller.current_write_idx = next_write_idx;
                auto& next_buffer = controller.buffers[next_write_idx];
                next_buffer.state = BufferState::FILLING;
                next_buffer.words_written = 0;
                next_buffer.data.clear();
                D_INFO("DMA", "Bank %d unstalled, switched to Buffer %d.", bank_id, next_write_idx);
            }
        }
    }
    schedule_tick_if_needed();
}

int DmaBuffer::getReadableBufferIndex(int bank_id) const {
    assert(bank_id >= 0 && bank_id < active_banks_);
    const auto& controller = bank_controllers_[bank_id];
    if (controller.buffers[0].state == BufferState::FULL) return 0;
    if (controller.buffers[1].state == BufferState::FULL) return 1;
    return -1; // 没有可供读取的已满缓冲区
}

const std::vector<uint32_t>& DmaBuffer::getBankData(int bank_id, int buffer_idx) const {
    assert(bank_id >= 0 && bank_id < active_banks_);
    assert(buffer_idx == 0 || buffer_idx == 1);
    assert(bank_controllers_[bank_id].buffers[buffer_idx].state == BufferState::FULL);
    return bank_controllers_[bank_id].buffers[buffer_idx].data;
}

void DmaBuffer::sendRetryReq(int port_id) {
    schedule_tick_if_needed();
}

void DmaBuffer::schedule_tick_if_needed() {
    if (!tickEvent.scheduled() && trans_state_ != IDLE) {
        schedule(tickEvent, curTick() + 1);
    }
}

Port& DmaBuffer::getPort(const std::string &if_name, int idx) {
    // 端口命名："dma_side<bank>"
    if (if_name.rfind("dma_side", 0) == 0) {
        int bank = std::stoi(if_name.substr(8));
        if (bank >= 0 && bank < num_ports)
            return requestPorts[bank];
    }
    throw std::runtime_error("No such port: " + if_name);
}

} // namespace GNN

