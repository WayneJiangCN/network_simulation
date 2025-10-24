/*
 * 基于命令队列的、自动控制双缓冲的 DMA 上游模块。
 * 作为一个忠实的指令执行引擎，处理来自主控的DMA命令。
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
#include <functional> // 用于 std::function
 
 namespace GNN
 {
 #define    CAL_GRAN 16
 #define    WORD_SIZE 16
 #define   BURST_BITS 512
 
   class DmaBuffer : public SimObject
   {
   public:
     static constexpr int num_ports = 8;
     static constexpr int addr_stride = 64;
     
 
     // --- 命令结构体定义 ---
     struct DmaCommand
     {
       addr_t base_addr;
       int total_lines;
       uint64_t cmd_id; // 用于跟踪命令的唯一ID
       // 命令完成回调：当DMA完成数据 *获取* 后调用
       std::function<void(uint64_t cmd_id)> completion_callback;
     };
 
     DmaBuffer(const std::string &name, addr_t base_addr, int row_words,
               int active_banks = num_ports);
 
     void init() override;
 
     // --- 端口 API ---
     bool recvTimingResp(PacketPtr pkt, int port_id);
     void sendRetryReq(int port_id);
     Port &getPort(const std::string &if_name, int idx = -1) override;
 
     // --- 核心控制 API ---
     // API: 主控调用此函数将一个DMA任务入队
     void enqueueCommand(const DmaCommand &cmd);
 
     // API: 下游模块释放一个已消耗的缓冲区
     void releaseBankBuffer(int bank_id, int buffer_idx);
 
     // API: 查询数据
     int getReadableBufferIndex(int bank_id) const;
 
     bool recvTimingReq(PacketPtr pkt, uint32_t bank_id);
 
   private:
     // --- 缓冲区状态管理 ---
     enum class BufferState
     {
       FILLING,
       FULL
     };
     struct BankBuffer
     {
       std::vector<PacketPtr> dma_pkt;
       uint32_t pkt_num;
       BufferState state = BufferState::FILLING;
       int words_written = 0;
     };
   
    struct BankController
    {
      BankBuffer buffers[2]; // 0=Ping, 1=Pong
      addr_t dram_base_addr[2];
      addr_t dram_final_addr[2];
      int current_write_idx = 0;
      bool stalled[2] = {false, false}; // 每个buf独立的stall状态
    };
     std::vector<BankController> bank_controllers_;
 
    // --- 状态机与传输管理 ---
    enum TransState
    {
      IDLE,
      CONFIG,
      STREAMING
    };
    TransState trans_state_;

    std::deque<DmaCommand> cmd_queue_; // 命令队列
    DmaCommand current_cmd_;           // 当前正在执行的命令
    int inst_cnt; 

    addr_t bank_rd_addr_[num_ports]{};
    bool bank_transfer_active_[num_ports]{};
    int lines_fetched_for_cmd_ = 0; // 已为当前命令获取的行数
    int addr_stride_;
    
    // 当前命令使用的buf索引 (0 或 1)
    int current_buf_idx_ = 0;
    // 每个buf对应的命令ID，用于正确识别数据来源
    uint64_t buf_cmd_id_[2] = {0, 0};
    // 每个buf对应的命令回调函数
    std::function<void(uint64_t)> buf_cmd_callback_[2] = {nullptr, nullptr};
 
     // --- 内部资源 ---
     addr_t base_addr_;
     const int row_words_;
     const int active_banks_;
     std::vector<std::deque<PacketPtr>> req_fifos_;
 
     class DmaRequestPort : public RequestPort
     {
       DmaBuffer &owner;
       int port_id;
 
     public:
       DmaRequestPort(const std::string &name, DmaBuffer &o, int id);
       bool recvTimingResp(PacketPtr pkt) override;
       void recvReqRetry() override;
     };
     std::vector<DmaRequestPort> requestPorts;
     EventFunctionWrapper sendRespondEvent;
     // 计算侧端口（响应端），供计算模块作为请求端发起拉取
     class ComputeSidePort : public ResponsePort
     {
       DmaBuffer &owner;
       int bank_id;
 
     public:
       ComputeSidePort(const std::string &name, DmaBuffer &o, int id);
       bool recvTimingReq(PacketPtr pkt) { return owner.recvTimingReq(pkt, bank_id); };
       bool tryTiming(PacketPtr pkt) override;
       void recvRespRetry() override;
     };
     std::vector<ComputeSidePort> computePorts;
     std::vector<std::deque<PacketPtr>> compute_resp_fifos_;
 
     bool response_retryReq[num_ports];  // 记录每个bank是否等待发送请求的重试
     bool response_retryResp[num_ports]; // 记录每个bank是否等待发送响应的重试
 
     bool request_retryResp[num_ports]; // 记录每个bank是否等待发送响应的重试
     EventFunctionWrapper tickEvent;
     // 读出轮转偏好：在两个 FULL 缓冲间轮转选择
     bool next_read_idx_[num_ports];
    // --- 行为 ---
    void tick();
    void sendRespond();
    void schedule_tick_if_needed();
    void check_current_cmd_completion();
    void check_buf_cmd_completion(int buf_idx);
    void maybe_notify_compute_full(int bank_id);
   };
 
 } // namespace GNN
 
 #endif // GNN_DMABUFFER_H_
 