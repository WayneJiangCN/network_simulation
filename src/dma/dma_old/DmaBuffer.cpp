// #include "dma/DmaBuffer.h"

// namespace GNN {
//    constexpr int DmaBuffer::num_ports ;
//    constexpr int DmaBuffer::addr_stride ;
// DmaBuffer::DmaBuffer(const std::string &name, addr_t base_addr, int row_words,
//                      int active_banks)
//     : SimObject(name), base_addr_(base_addr), row_words_(row_words),
//       active_banks_(std::min(active_banks, num_ports)), trans_state_(INIT),
//       addr_stride_(active_banks_ * addr_stride), // 假设4字节粒度
//       tickEvent([this] { try_send_event(); }, name + ".tickEvent") {
//   requestPorts.reserve(num_ports);
//   req_fifos_.reserve(num_ports);
//   for (int i = 0; i < num_ports; ++i) {
//     retryReq[i] = false;
//     requestPorts.emplace_back(name + ".dma_side" + std::to_string(i), *this, i);
//     req_fifos_.emplace_back();
//     ping_pong_flag_[i] = false;
//     remaining_bytes_[i] = 0;
//     cur_dst_base_[i] = 0;
//     remaining_read_bytes_[i] = 0;
//     cur_src_addr_[i] = 0;
//     write_word_offset_[i] = 0;
//     write_burst_count_[i] = 0;
//     bank_rd_addr_[i] = 0;
//     bank_rd_vld_[i] = false;
//     bank_trans_done_[i] = false;
//   }
//   transGlobal_flag_ = false;
//   bank_read_cmds_.resize(active_banks_);
//   bank_ping_storage_.resize(active_banks_);
//   bank_pong_storage_.resize(active_banks_);
//   for (int b = 0; b < active_banks_; ++b) {
//     bank_ping_storage_[b].reserve(static_cast<size_t>(row_words_));
//     bank_pong_storage_[b].reserve(static_cast<size_t>(row_words_));
//   }
//   // 初始化全局指令
//   global_cmd_.base_addr = 0;
//   global_cmd_.final_addr = 0;
//   global_cmd_.line_num = 0;
//   global_cmd_.bank_wr_sel = 0;
// }

// void DmaBuffer::init() {
//   // 初始化：为每个启用的 bank 预装一行数据并发起写请求
//   for (int bank = 0; bank < active_banks_; ++bank) {
//     std::vector<uint32_t> row;
//     row.reserve(row_words_);
//     for (int w = 0; w < row_words_; ++w) {
//       row.push_back(static_cast<uint32_t>(base_addr_ + bank * 64 + w));
//     }
//     PacketPtr pkt =
//         PacketManager::create_write_packet(base_addr_ + bank * 64, row);
//     req_fifos_[bank].push_back(pkt);
//   }
//   if (!tickEvent.scheduled())
//     schedule(tickEvent, curTick() + 1);
// }

// void DmaBuffer::try_send_event() {
//   // 1. 处理全局状态机（多 bank 并行读取）
//   issue_global_read_bursts();

//   // 2. 处理原有的单 bank 指令队列
//   // issue_from_cmd_queue();

//   // 3. 发送所有请求
//   for (int bank = 0; bank < num_ports; ++bank) {
//     if (!req_fifos_[bank].empty()) {
//       PacketPtr pkt = req_fifos_[bank].front();
//       if (requestPorts[bank].sendTimingReq(pkt)) {
//         req_fifos_[bank].pop_front();
//       }
//     }
//   }
//   if (!tickEvent.scheduled())
//     schedule(tickEvent, curTick() + 1);
// }

// void DmaBuffer::issue_from_cmd_queue() {
//   // 从指令队列为各 bank 生成 64B 对齐的写突发。
//   // 简单轮询：若某个 bank 当前无在途数据，则拉取一条指令并开始执行。
//   for (int bank = 0; bank < active_banks_; ++bank) {
//     if (remaining_bytes_[bank] <= 0 && !cmd_queue_.empty()) {
//       DmaCmd cmd = cmd_queue_.front();
//       cmd_queue_.pop_front();
//       // 选择乒乓缓冲区（ping 或 pong）
//       addr_t dst_base = ping_pong_flag_[bank] ? cmd.dst1 : cmd.dst0;
//       ping_pong_flag_[bank] = !ping_pong_flag_[bank];
//       cur_dst_base_[bank] = dst_base;
//       remaining_bytes_[bank] = cmd.length_bytes;
//     }

//     // 每个 tick、每个 bank 至多发出一个 64B 突发
//     if (remaining_bytes_[bank] > 0) {
//       int burst_bytes = std::min(remaining_bytes_[bank], 64);
//       int words = burst_bytes / 4;
//       std::vector<uint32_t> data;
//       data.reserve(words);
//       addr_t dst_addr = cur_dst_base_[bank] + (cmd_queue_.empty() ? 0 : 0);
//       // 示例数据：使用 dst_addr + word 索引填充
//       for (int w = 0; w < words; ++w) {
//         data.push_back(static_cast<uint32_t>(dst_addr + w));
//       }
//       PacketPtr pkt = PacketManager::create_write_packet(dst_addr, data);
//       req_fifos_[bank].push_back(pkt);
//       cur_dst_base_[bank] += burst_bytes;
//       remaining_bytes_[bank] -= burst_bytes;
//     }
//   }
// }

// bool DmaBuffer::sendTimingReq(PacketPtr pkt, int port_id) {
//   bool ok = false;
//   if (port_id >= 0 && port_id < num_ports)
//     ok = requestPorts[port_id].sendTimingReq(pkt);
//   return ok;
// }

// bool DmaBuffer::recvTimingResp(PacketPtr pkt, int port_id) {
//   // 将 DRAM 返回的数据写入本地存储（按 bank 的乒乓缓冲区）
//   if (pkt->isRead()) {
//     const std::vector<uint32_t> &data = pkt->getData();
    
//     // 根据当前乒乓标志选择缓冲区
//     auto &buf = ping_pong_flag_[port_id] ? bank_pong_storage_[port_id]
//                                          : bank_ping_storage_[port_id];
    
//     // 追加写入数据
//     buf.insert(buf.end(), data.begin(), data.end());
    
//     // 更新写入偏移
//     write_word_offset_[port_id] += data.size();
//     write_burst_count_[port_id] += 1;
    
//     D_INFO("DMA", "Bank %d 存储数据到 %s 缓冲区: addr=%d, words=%zu, 总偏移=%d", 
//            port_id, ping_pong_flag_[port_id] ? "PONG" : "PING", 
//            pkt->getAddr(), data.size(), write_word_offset_[port_id]);
    
//     // 打印数据内容（调试用）
//     std::cout << "Bank " << port_id << " " << (ping_pong_flag_[port_id] ? "PONG" : "PING") 
//               << " Data (addr: " << pkt->getAddr() << "): ";
//     for (size_t j = 0; j < std::min(data.size(), (size_t)16); j++) {
//       std::cout << data[j] << " ";
//     }
//     std::cout << std::endl;
    
//     // 若该 bank 当前缓冲区达到预期大小，则自动切换到另一缓冲
//     // 计算当前指令单 bank 预期突发次数（按 16B 或 64B 自行匹配，此处以包计）
//     // 这里约定我们每次 read_packet 长度固定为 16B -> data.size()==4 words
//     int expected_bursts_per_bank = global_cmd_.line_num; // 与发起时一致
//     if (write_burst_count_[port_id] >= expected_bursts_per_bank) {
//       // 切换 ping/pong，并复位计数
//       ping_pong_flag_[port_id] = !ping_pong_flag_[port_id];
//       write_word_offset_[port_id] = 0;
//       write_burst_count_[port_id] = 0;
//       D_INFO("DMA", "Bank %d 缓冲写满，自动切换到 %s 缓冲区", port_id,
//              ping_pong_flag_[port_id] ? "PONG" : "PING");
//     }
//   }
//   delete pkt;
//   return true;
// }

// void DmaBuffer::sendRetryReq(int port_id) {
//   if (!tickEvent.scheduled())
//     schedule(tickEvent, curTick() + 1);
// }

// void DmaBuffer::enqueueCommand(addr_t src, addr_t dst0, addr_t dst1,
//                                int length_bytes) {
//   cmd_queue_.push_back(DmaCmd{src, dst0, dst1, length_bytes});
//   if (!tickEvent.scheduled())
//     schedule(tickEvent, curTick() + 1);
// }

// void DmaBuffer::enqueueBankCommand(int bank_id, addr_t src, int length_bytes) {
//   assert(bank_id >= 0 && bank_id < active_banks_);
//   bank_read_cmds_[bank_id].push_back(ReadCmd{src, length_bytes});
//   if (!tickEvent.scheduled())
//     schedule(tickEvent, curTick() + 1);
// }

// void DmaBuffer::enqueueGlobalCommand(addr_t base_addr, int line_num) {
//   global_cmd_.base_addr = base_addr;
//   global_cmd_.final_addr = base_addr + line_num * addr_stride; // 一个burst
//   global_cmd_.line_num = line_num;
//   global_cmd_.bank_wr_sel = (global_cmd_.bank_wr_sel + 1) % 2; // 切换 bank 组

//   // 重置所有 bank 状态（不在此处切换乒乓；改为按 bank 判满时切换）
//   for (int i = 0; i < active_banks_; ++i) {
//     bank_trans_done_[i] = false;
//     bank_rd_vld_[i] = false;
//     write_word_offset_[i] = 0;
//     write_burst_count_[i] = 0;
//   }
//   transGlobal_flag_ = true;
//   if (!tickEvent.scheduled())
//     schedule(tickEvent, curTick() + 1);
// }

// void DmaBuffer::issue_global_read_bursts() {
//   switch (trans_state_) {
//   case INIT: {
//     if(transGlobal_flag_)
//       trans_state_ = CONFIG;
//     break;
//   }
//   case CONFIG: {
//     // 配置阶段：为所有 bank 设置初始地址
//     int ori_ch = (global_cmd_.base_addr % 0x100) / 0x40; //
//     for (int i = 0; i < active_banks_; ++i) {
//       addr_t dram_bias_addr = i >= ori_ch
//                                   ? (i - ori_ch) * addr_stride
//                                   : (i - ori_ch + active_banks_) * addr_stride;
//       bank_rd_addr_[i] = global_cmd_.base_addr + dram_bias_addr;
//       bank_rd_vld_[i] = true;
//       bank_trans_done_[i] = false;
//     }
//     trans_state_ = TRANS;
//     break;
//   }
//   case TRANS: {
//     // 传输阶段：更新各 bank 地址
//     update_bank_addresses();
//     check_trans_done();
//     break;
//   }
//   }
// }

// void DmaBuffer::update_bank_addresses() {
//   for (int i = 0; i < active_banks_; ++i) {
//     if (bank_rd_vld_[i] && !bank_trans_done_[i]) {
//       // 生成读请求
//       PacketPtr read_pkt =
//           PacketManager::create_read_packet(bank_rd_addr_[i], 16);
//       req_fifos_[i].push_back(read_pkt);

//       // 更新地址
//       if (bank_rd_addr_[i] < global_cmd_.final_addr - addr_stride_) {
//         bank_rd_addr_[i] += addr_stride_;
//       } else {
//         bank_rd_vld_[i] = false;
//         bank_trans_done_[i] = true;
//       }
//     }
//   }
// }

// void DmaBuffer::check_trans_done() {
//   bool all_done = true;
//   for (int i = 0; i < active_banks_; ++i) {
//     if (!bank_trans_done_[i]) {
//       all_done = false;
//       break;
//     }
//   }
//   if (all_done){
//     trans_state_ = INIT;
//     transGlobal_flag_ = false;
//   }
// }

// const std::vector<uint32_t>& DmaBuffer::getBankData(int bank_id, bool use_pong) const {
//   assert(bank_id >= 0 && bank_id < active_banks_);
//   return use_pong ? bank_pong_storage_[bank_id] : bank_ping_storage_[bank_id];
// }

// size_t DmaBuffer::getBankBufferSize(int bank_id, bool use_pong) const {
//   assert(bank_id >= 0 && bank_id < active_banks_);
//   return use_pong ? bank_pong_storage_[bank_id].size() : bank_ping_storage_[bank_id].size();
// }

// } // namespace GNN
