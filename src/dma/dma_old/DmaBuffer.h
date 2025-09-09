// /*
//  * 简单的 DMA 上游模块：对外每个 DRAM bank 暴露一个端口。
//  * 每个 bank 持有一行数据，并在初始化时发起一次写操作。
//  */

// #ifndef GNN_DMABUFFER_H_
// #define GNN_DMABUFFER_H_

// #include "common/object.h"
// #include "common/packet.h"
// #include "common/port.h"
// #include "event/eventq.h"
// #include <deque>
// #include <string>
// #include <vector>

// namespace GNN {

// class DmaBuffer : public SimObject {
// public:
//   // 每个 DRAM bank 一个端口（上限）。可配置启用的 bank 数量。
//   static constexpr int num_ports = 8;
//   static constexpr int addr_stride =16* sizeof(int);

//   // row_words：每行包含的 32 位字数；active_banks <= num_ports
//   DmaBuffer(const std::string &name, addr_t base_addr, int row_words,
//             int active_banks = num_ports);

//   void init() override;

//   // 端口 API
//   bool sendTimingReq(PacketPtr pkt, int port_id);
//   bool recvTimingResp(PacketPtr pkt, int port_id);
//   void sendRetryReq(int port_id);

//   Port &getPort(const std::string &if_name, int idx = -1) override {
//     // 端口命名："dma_side<bank>"
//     if (if_name.find("dma_side") == 0) {
//       int bank = std::stoi(if_name.substr(8));
//       if (bank >= 0 && bank < num_ports)
//         return requestPorts[bank];
//     }
//     throw std::runtime_error("No such port");
//   }

// private:
//   // bank 0 的基地址；第 i 个 bank 使用 base + i*64
//   addr_t base_addr_;
//   int row_words_;
//   int active_banks_;

//   // 每个端口的请求队列
//   std::vector<std::deque<PacketPtr>> req_fifos_;

//   // DMA 指令与状态
//   struct DmaCmd {
//     addr_t src;          // 源地址
//     addr_t dst0;         // 乒乓缓冲区 ping 基址
//     addr_t dst1;         // 乒乓缓冲区 pong 基址
//     int length_bytes;    // 传输总字节数
//   };

//   std::deque<DmaCmd> cmd_queue_;
//   // 每个 bank 的乒乓选择：false->dst0，true->dst1
//   bool ping_pong_flag_[num_ports]{};
//   // 每个 bank 当前正在执行指令剩余字节数（若有）
//   int remaining_bytes_[num_ports]{};
//   // 每个 bank 当前在用的目的基地址
//   addr_t cur_dst_base_[num_ports]{};
//   // 状态机：CONFIG0→TRANS_ROW→CONFIG1→TRANS_COL
//   // enum TransState {
//   //   CONFIG0,
//   //   TRANS_ROW,
//   //   CONFIG1,
//   //   TRANS_COL
//   // };

//   enum TransState {
//     INIT,
//     CONFIG,
//     TRANS
//   };
//   TransState trans_state_;
//   // 当前指令信息（全局）
//   struct GlobalCmd {
//     addr_t base_addr;
//     addr_t final_addr;
//     int line_num;
//     int bank_wr_sel;  // 当前写入的 bank 组选择
//   };
//   GlobalCmd global_cmd_;
//   // 每个 bank 的地址步进状态
//   addr_t bank_rd_addr_[num_ports]{};
//   bool bank_rd_vld_[num_ports]{};
//   bool bank_trans_done_[num_ports]{};
//   // 地址步长（通道数 * 粒度）
//   int addr_stride_;
//   // 全局传输标志：只有为 true 时才运行状态机
//   bool transGlobal_flag_;
//   // 读取侧：每个 bank 的命令队列（仅需 src/len，用于从 DRAM 读取）
//   struct ReadCmd {
//     addr_t src;
//     int length_bytes;
//   };
//   std::vector<std::deque<ReadCmd>> bank_read_cmds_; // size = active_banks_
//   // 读取侧：每个 bank 的在途状态
//   int remaining_read_bytes_[num_ports]{};
//   addr_t cur_src_addr_[num_ports]{};
//   // 本地存储：每个 bank 的 ping/pong 缓冲区（保存32位数据）
//   std::vector<std::vector<uint32_t>> bank_ping_storage_;
//   std::vector<std::vector<uint32_t>> bank_pong_storage_;
//   // 当前写入偏移（以word计）
//   int write_word_offset_[num_ports]{};
//   // 当前写入已接收的突发次数（以包计），用于判满并切换乒乓缓冲
//   int write_burst_count_[num_ports]{};

//   // 端口实现
//   class DmaRequestPort : public RequestPort {
//     DmaBuffer &owner;
//     int port_id;

//   public:
//     DmaRequestPort(const std::string &name, DmaBuffer &o, int id)
//         : RequestPort(name), owner(o), port_id(id) {}
//     bool recvTimingResp(PacketPtr pkt) override {
//       return owner.recvTimingResp(pkt, port_id);
//     }
//     void recvReqRetry() override { owner.sendRetryReq(port_id); }
//   };

//   std::vector<DmaRequestPort> requestPorts;

//   // 事件
//   EventFunctionWrapper tickEvent;

//   // 行为
//   void try_send_event();
//   void issue_from_cmd_queue();
//   void issue_global_read_bursts();
//   void update_bank_addresses();
//   void check_trans_done();

// public:
//   // API：上层（主机侧）入队一条 DMA 指令
//   void enqueueCommand(addr_t src, addr_t dst0, addr_t dst1, int length_bytes);
//   // API：按 bank 入队读取命令（仅 DRAM -> 本地存储）
//   void enqueueBankCommand(int bank_id, addr_t src, int length_bytes);
//   // API：入队全局指令（多 bank 并行读取）
//   void enqueueGlobalCommand(addr_t base_addr, int line_num);
//   // API：读取指定 bank 的 ping/pong 缓冲区数据
//   const std::vector<uint32_t>& getBankData(int bank_id, bool use_pong = false) const;
//   // API：获取指定 bank 当前使用的缓冲区状态
//   bool getBankPingPongState(int bank_id) const { return ping_pong_flag_[bank_id]; }
//   // API：获取指定 bank 的缓冲区大小
//   size_t getBankBufferSize(int bank_id, bool use_pong = false) const;

//   // 标志
//   bool retryReq[num_ports]{};
// };

// } // namespace GNN

// #endif // GNN_DMABUFFER_H_


