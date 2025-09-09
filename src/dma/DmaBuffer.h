/*
 * 自动控制双缓冲的 DMA 上游模块。
 * 能够自主管理乒乓缓冲区，形成连续的数据流。
 */

 #ifndef GNN_DMABUFFER_H_
 #define GNN_DMABUFFER_H_
 
 #include "common/object.h"
 #include "common/packet.h"
 #include "common/port.h"
 #include "event/eventq.h"
 #include <deque>
 #include <string>
 #include <vector>
 #include <cassert>
 
 namespace GNN {
 
 class DmaBuffer : public SimObject {
 public:
     static constexpr int num_ports = 8;
     static constexpr int addr_stride = 16 * sizeof(int);
 
     DmaBuffer(const std::string &name, addr_t base_addr, int row_words,
               int active_banks = num_ports);
 
     void init() override;
 
     // --- 端口 API ---
     bool recvTimingResp(PacketPtr pkt, int port_id);
     void sendRetryReq(int port_id);
     Port &getPort(const std::string &if_name, int idx = -1) override;
 
     // --- 核心控制 API ---
     // API：启动一次连续的数据流传输
     void startStreaming(addr_t base_addr, int total_lines);
     
     // API：下游模块在消耗完数据后，调用此函数来释放一个缓冲区
     void releaseBankBuffer(int bank_id, int buffer_idx);
 
     // API：查询哪个缓冲区已满可读 (-1 表示没有)
     int getReadableBufferIndex(int bank_id) const;
 
     // API：读取指定 bank 的缓冲区数据
     const std::vector<uint32_t>& getBankData(int bank_id, int buffer_idx) const;
 
 private:
     // --- 缓冲区状态管理 ---
     enum class BufferState {
         EMPTY,   // 空闲，可用于填充
         FILLING, // 正在从DRAM接收数据
         FULL     // 已填满，等待下游读取
     };
 
     struct BankBuffer {
         std::vector<uint32_t> data;
         BufferState state = BufferState::EMPTY;
         int words_written = 0; // 已写入的字数
     };
 
     struct BankController {
         BankBuffer buffers[2]; // 0=Ping, 1=Pong
         int current_write_idx = 0; // DMA当前正在写的缓冲区索引 (0 or 1)
         bool stalled = false;    // 是否因下游处理慢而暂停
     };
     std::vector<BankController> bank_controllers_;
 
     // --- 状态机与传输管理 ---
     enum TransState {
         IDLE,
         CONFIG,
         STREAMING
     };
     TransState trans_state_;
 
     struct GlobalCmd {
         addr_t base_addr;
         addr_t final_addr;
         int total_lines_remaining;
     };
     GlobalCmd global_cmd_;
     
     // 每个 bank 的地址步进状态
     addr_t bank_rd_addr_[num_ports]{};
     bool bank_transfer_active_[num_ports]{}; // 此 bank 当前是否在传输周期内
     int addr_stride_;
 
     // --- 内部资源 ---
     addr_t base_addr_;
     const int row_words_;
     const int active_banks_;
     std::vector<std::deque<PacketPtr>> req_fifos_;
 
     class DmaRequestPort : public RequestPort {
         DmaBuffer &owner;
         int port_id;
     public:
         DmaRequestPort(const std::string &name, DmaBuffer &o, int id)
             : RequestPort(name), owner(o), port_id(id) {}
         bool recvTimingResp(PacketPtr pkt) override {
             return owner.recvTimingResp(pkt, port_id);
         }
         void recvReqRetry() override { owner.sendRetryReq(port_id); }
     };
     std::vector<DmaRequestPort> requestPorts;
 
     EventFunctionWrapper tickEvent;
     EventFunctionWrapper realseEvent;
     // --- 行为 ---
     void tick();
     void realse();
     void schedule_tick_if_needed();
 };
 
 } // namespace GNN
 
 #endif // GNN_DMABUFFER_H_
 
 