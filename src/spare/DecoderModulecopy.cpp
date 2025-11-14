// // src/compute/DecoderModule.cpp

// #include "DecoderModule.h"
// #include "common/common.h"
// #include "common/debug.h"
// #include "common/packet.h"
// #include <cassert>
// #include <cstdint>
// #include <fstream>
// #include <sstream>
// #include <string>

// namespace GNN
// {

//   // =======================================================================
//   // 端口实现
//   // =======================================================================

//   DecoderModule::BankRequestPort::BankRequestPort(const std::string &name,
//                                                   DecoderModule &o, int id,
//                                                   const std::string &type)
//       : RequestPort(name), owner(o), bank_id(id), bank_name(type) {}
//   DecoderModule::CompResponsePort::CompResponsePort(const std::string &name,
//                                                     DecoderModule &o, int id)
//       : ResponsePort(name), owner(o), bank_id(id) {}

//   bool DecoderModule::BankRequestPort::recvTimingResp(PacketPtr pkt)
//   {
//     // 统一调用 DecoderModule 的响应处理函数
//     return owner.recvTimingResp(pkt, bank_id, bank_name);
//   }
//   uint64_t bm_total_cycle[8];
//   uint64_t fw_total_cycle[8];
//   uint64_t wt_total_cycle[8];
//   uint64_t bm_current_cycle[8];
//   uint64_t fw_current_cycle[8];
//   uint64_t wt_current_cycle[8];
//   void DecoderModule::BankRequestPort::recvReqRetry()
//   {
//     // 对端可再次接收请求，标记该 bank 需要重试
//     D_DEBUG("DECODER", "Bank %d : Retry request,bank_name: %s", bank_id,
//             bank_name.c_str());
//     if (bank_name == "bmap")
//     {
//       owner.pending_request_[bank_id] = true;
//       bm_total_cycle[bank_id] += gSim->getCurTick() - bm_current_cycle[bank_id];
//     }
//     if (bank_name == "weight")
//     {
//       owner.pending_weight_request_[bank_id] = true;
//       wt_total_cycle[bank_id] = gSim->getCurTick() - wt_current_cycle[bank_id];
//     }
//     else if (bank_name == "feature")
//     {
//       owner.pending_feature_request_[bank_id] = true;
//       fw_total_cycle[bank_id] += gSim->getCurTick() - fw_current_cycle[bank_id];
//     }
//     owner.scheduleTickIfNeeded(1); // 立即安排下一拍的 Tick 尝试重发
//   }

//   // =======================================================================
//   // 构造函数与初始化
//   // =======================================================================

//   DecoderModule::DecoderModule(const std::string &name, int active_banks,
//                                SimDramStorage *sim_dram_storage)
//       : SimObject(name), active_banks_(active_banks),
//         sim_dram_storage_(sim_dram_storage),
//         tickEvent([this]
//                   { tick(); }, name + ".tickEvent"),
//         retry2CamEvent([this]
//                        { retry2CamTick(); }, name + ".retry2CamEvent"),
//         clearCamEvent([this]
//                       { clearCamtick(); }, name + ".clearCamEvent")
//   {
//     OUT.open("./result/mac_u.txt", std::ios::trunc);
//     if (!OUT)
//     {
//       std::cerr << "Failed to open file." << std::endl;
//       throw std::runtime_error("Failed to open file.");
//     }
//     weight_success.assign(active_banks_, false);
//     feature_success.assign(active_banks_, false);
//     bank_states_.resize(active_banks_);
//     bank_wf_request_info_.resize(active_banks_);
//     pending_request_.assign(active_banks_, false);
//     pending_weight_request_.assign(active_banks_, false);
//     pending_feature_request_.assign(active_banks_, false);
//     in_flight_.assign(active_banks_, false);
//     weight_in_flight_.assign(active_banks_, false);
//     feature_in_flight_.assign(active_banks_, false);
//     // 初始化每个bank的Hash CAM（两个CAM：行0与行1）
//     hash_cam_.resize(active_banks_);

//     // 构造请求端口
//     bitmapRequestPorts.reserve(active_banks_);
//     weightRequestPorts.reserve(active_banks_);
//     featureRequestPorts.reserve(active_banks_);
//     computresponsePort.reserve(active_banks_);
//     Info2Cam_.resize(active_banks_);
//     hash_cam_perf_stats_.resize(active_banks_);
//     file_stall.resize(active_banks);
//     for (int i = 0; i < active_banks_; ++i)
//     {
//       // 端口名称: module.bank_type_sideN
//       computresponsePort.emplace_back(name + "compute_side" + std::to_string(i),
//                                       *this, i);
//       bitmapRequestPorts.emplace_back(name + "bmap_side" + std::to_string(i),
//                                       *this, i, "bmap");
//       weightRequestPorts.emplace_back(name + "w_side" + std::to_string(i), *this,
//                                       i, "weight");
//       featureRequestPorts.emplace_back(name + "f_side" + std::to_string(i), *this,
//                                        i, "feature");
//       // 设置 file_stall 的初始参数
//       file_stall[i].channel_id = i;
//       file_stall[i].file_total_row = FILE_TOTAL_ROW_CFG;
//       file_stall[i].file_total_col = FILE_TOTAL_COL_CFG;
//       file_stall[i].file_slice_row = FILE_SLICE_ROW_CFG;
//       file_stall[i].file_slice_col = FILE_SLICE_COL_CFG;
//       file_stall[i].total_addr_count += file_stall[i].file_slice_row * file_stall[i].file_slice_col / BURST_BITS;
//       file_stall[i].final_addr += ((file_stall[i].file_slice_row * file_stall[i].file_slice_col / BURST_BITS  - 1)* INST_ADDR_STRIDE)+i * CHANNEL_ADDR_DIF;
//       D_INFO("CAM", "bank_id :%d total_addr_count :%d final_addr : %d", i, file_stall[i].total_addr_count, file_stall[i].final_addr);
//       assert(file_stall[i].final_addr > 0 && "addr fault");
//       file_stall[i].current_addr_count = 0;
//       file_stall[i].decoder_stall = false;
//     }
//   }

//   void DecoderModule::init()
//   {
//     // 初始化所有bank状态
//     for (int bank = 0; bank < active_banks_; ++bank)
//     {
//       resetBankState(bank);
//     }

//     // 启动状态机 Tick
//     scheduleTickIfNeeded(1);
//     D_INFO("DECODER",
//            "DecoderModule initialized with %d banks, processing %d elements per "
//            "cycle",
//            active_banks_, 32);
//   }

//   void DecoderModule::resetBankState(int bank_id)
//   {

//     bank_states_[bank_id] = {}; // 重置为默认值
//     bank_states_[bank_id].elements_per_cycle =
//         BITMAP_LINE_SIZE * 2; // 每个cycle处理32个元素
//     pending_request_[bank_id] = true;
//     pending_weight_request_[bank_id] = true;
//     pending_feature_request_[bank_id] = true;
//     in_flight_[bank_id] = false;
//     weight_in_flight_[bank_id] = false;
//     feature_in_flight_[bank_id] = false;
//   }

//   void DecoderModule::scheduleTickIfNeeded(uint32_t delay)
//   {
//     if (!tickEvent.scheduled())
//     {
//       schedule(tickEvent, curTick() + delay);
//     }
//   }
//   void DecoderModule::scheduleRetry2CamIfNeeded(uint32_t delay)
//   {
//     if (!retry2CamEvent.scheduled())
//     {
//       schedule(retry2CamEvent, curTick() + delay);
//     }
//   }
//   void DecoderModule::scheduleClearCamTick(uint32_t delay)
//   {
//     if (!clearCamEvent.scheduled())
//     {
//       schedule(clearCamEvent, curTick() + delay);
//     }
//   }
//   void DecoderModule::tick()
//   {

//     for (int bank = 0; bank < active_banks_; ++bank)
//     {
//       // 如果当前 bank 没有待请求且没有请求在飞，则跳过
//       if (!pending_request_[bank] || in_flight_[bank])
//       {
//         continue;
//       }

//       // 1. IDLE 状态：发送 Bitmap 请求 (启动)
//       if (bank_states_[bank].state == DecoderState::IDLE && !file_stall[bank].decoder_stall)
//       {
//         // 如果bitmap处理完成，重置状态准备获取新的bitmap
//         if (bank_states_[bank].bitmap_processing_complete)
//         {
//           D_DEBUG("DECODER",
//                   "Bank %d: Previous bitmap processing complete, resetting for "
//                   "new bitmap",
//                   bank);
//           resetBankState(bank);
//         }
//         // 向 BitmapBank 发送请求
//         PacketPtr req =
//             PacketManager::create_read_packet(0, BURST_BITS / BITMAP_LINE_SIZE);
//         if (bitmapRequestPorts[bank].sendTimingReq(req))
//         {
//           D_DEBUG("DECODER", "Bank %d: Sent BITMAP request", bank);
//           in_flight_[bank] = true;
//           pending_request_[bank] = false;
//           bank_states_[bank].state = DecoderState::BITMAP_WAIT;
//         }
//         else
//         {
//           bm_current_cycle[bank] = gSim->getCurTick();
//           // D_DEBUG("DECODER", "Bank %d: Bitmap request rejected, retrying
//           // later",
//           //        bank);
//           PacketManager::free_packet(req);
//           pending_request_[bank] = true; // 保持 pending
//         }
//       }

//       // 2. DECODE_START 状态：发送 Weight/Feature 请求 (依赖 Bitmap)
//       else if (bank_states_[bank].state == DecoderState::DECODE_START &&
//                !Info2Cam_[bank].retry2Cam_flag)
//       {
//         // 每个cycle处理两行（两个16-bit word）
//         if (bank_states_[bank].processed_words < bank_states_[bank].total_words)
//         {
//           // 发送 Weight 请求 (拉取当前周期的数据)
//           D_DEBUG("DECODER", "Bank %d: Sending Weight and Feature request", bank);

//           PacketPtr w_req = PacketManager::create_read_packet(
//               0x1000000, BURST_BITS / WORD_SIZE);
//           PacketPtr f_req = PacketManager::create_read_packet(
//               0x2000000, BURST_BITS / WORD_SIZE);
//           if (!bank_states_[bank].weight_received && !weight_in_flight_[bank] &&
//               pending_weight_request_[bank])
//           {

//             if (bank_wf_request_info_[bank].weight_is_empty)
//             {
//               bank_wf_request_info_[bank].weight_is_empty = false;
//               w_req->setWeightBufferIsClear(true); // 设置权重缓冲区是否为空
//             }

//             weight_success[bank] = weightRequestPorts[bank].sendTimingReq(w_req);
//             if (weight_success[bank])
//             {
//               weight_in_flight_[bank] = true;
//               pending_weight_request_[bank] = true;
//             }
//             else
//             {
//               wt_current_cycle[bank] = gSim->getCurTick();
//               pending_weight_request_[bank] = false;
//             }
//             D_DEBUG("DECODER", "Bank %d: Weight request success: %d", bank,
//                     static_cast<int>(weight_success[bank]));
//           }
//           if (!bank_states_[bank].feature_received && !feature_in_flight_[bank] &&
//               pending_feature_request_[bank])
//           {
//             // 发送 Feature 请求
//             if (bank_wf_request_info_[bank].feature_is_clear)
//             {
//               bank_wf_request_info_[bank].feature_is_clear = false;
//               f_req->setFeatureBufferIsClear(true); // 设置特征缓冲区是否为空
//             }
//             feature_success[bank] =
//                 featureRequestPorts[bank].sendTimingReq(f_req);
//             if (feature_success[bank])
//             {
//               feature_in_flight_[bank] = true;
//               pending_feature_request_[bank] = true;
//             }
//             else
//             {
//               fw_current_cycle[bank] = gSim->getCurTick();
//               D_DEBUG("DECODER",
//                       "Bank %d: Feature request failed, retrying later", bank);
//               pending_feature_request_[bank] = false;
//             }
//             D_DEBUG("DECODER", "Bank %d: Feature request success: %d", bank,
//                     static_cast<int>(feature_success[bank]));
//           }

//           if (weight_success[bank] && feature_success[bank])
//           {
//             size_t next_word = bank_states_[bank].processed_words;
//             size_t pairs_done = next_word / 2;
//             size_t total_pairs = (bank_states_[bank].total_words + 1) / 2;

//             D_DEBUG("DECODER", "Bank %d: Sent WF_REQ for row-pair %zu/%zu", bank,
//                     pairs_done + 1, total_pairs);
//             in_flight_[bank] = true; // 标记 WF 请求在飞
//             bank_states_[bank].state = DecoderState::WF_WAIT;
//             // 清除之前的接收标记
//             weight_success[bank] = false;
//             feature_success[bank] = false;
//           }
//           else
//           {
//             // 如果有一个失败，则全部释放
//             if (!weight_success[bank])
//             {
//               if (bank == 6)
//                 D_DEBUG("DECODER",
//                         "Bank %d: Weight request failed, retrying later", bank);
//               PacketManager::free_packet(w_req);
//             }
//             if (!feature_success[bank])
//             {

//               PacketManager::free_packet(f_req);
//             }
//           }
//         }
//         else
//         {
//           // 所有word处理完成

//           bank_states_[bank].bitmap_processing_complete = true;
//           D_DEBUG("DECODER", "Bank %d: All %zu words processed. Total ones %zu.",
//                   bank, bank_states_[bank].total_words,
//                   bank_states_[bank].total_elements);
//           bank_states_[bank].state = DecoderState::IDLE;
//         }
//       }
//     }
//   }

//   bool DecoderModule::recvTimingResp(PacketPtr pkt, uint32_t bank_id,
//                                      const std::string &bank_name)
//   {
//     // 收到响应，清除在飞标记；立刻安排下一次拉取

//     if (bank_name == "bmap")
//     {
//       if (bank_states_[bank_id].state == DecoderState::BITMAP_WAIT)
//         handleBitmapResponse(bank_id, pkt);
//     }
//     else if (bank_name == "weight" || bank_name == "feature")
//     {
//       handleWFResponse(bank_id, pkt, bank_name == "weight");
//     }

//     // 检查该 Bank 的所有依赖是否完成
//     bool block_complete = checkBlockCompletion(bank_id);

//     assert(block_complete && "Block completion check failed");
//     return true;
//   }
//   int addr_num = 0;
//   void DecoderModule::handleBitmapResponse(uint32_t bank_id, PacketPtr pkt)
//   {
//     D_DEBUG("DECODER", "Bank %d: Received BITMAP response. Size: %zu.", bank_id,
//             pkt->getSize());
//     // 1. 更新状态
//     in_flight_[bank_id] = false;

//     // 2. 解码/解压 (模拟功能)
//     // 从模拟DRAM读取bitmap数据到pkt，并统计1的数量
//     D_DEBUG("DECODER",
//             "Bank %d: Reading bitmap data from SIM_DRAM_STORAGE, addr: %d",
//             bank_id, pkt->getAddr());
//     sim_dram_storage_->readPacket(pkt);
//     bank_states_[bank_id].bitmap_pkt = pkt;
//     // 统计1的数量（每个元素代表16位=一行）
//     const auto &bitmap_words = pkt->getData();
//     size_t ones_count = 0;
//     // D_INFO("BUG", "Bank %d: Bitmap data: %zu, addr: %llu, addr_num: %d",
//     // bank_id, bitmap_words.size(),pkt->getAddr(),addr_num++);
//     bank_states_[bank_id].total_rows = pkt->getSize();
//     bank_states_[bank_id].row_ones_counts.assign(bank_states_[bank_id].total_rows,
//                                                  0);
//     if (pkt->getSize() < file_stall[bank_id].final_addr)
//     {
//       file_stall[bank_id].current_addr_count++;
//       // D_INFO("CAM","Bank id %d ADDR count %d",bank_id,file_stall[bank_id].current_addr_count);
//     }
//     else
//       assert(false && "file addr cfg error");
//     for (size_t i = 0; i < pkt->getSize(); ++i)
//     {
//       storage_t w = bitmap_words[i];
//       size_t row_count = __builtin_popcount(static_cast<storage_t>(w));
//       bank_states_[bank_id].row_ones_counts[i] = row_count;
//       ones_count += row_count;
//     }

//     bank_states_[bank_id].total_elements = ones_count; // 所有行1的总和
//     bank_states_[bank_id].total_words =
//         bitmap_words.size(); // 与total_rows一致，此处保留字段复用
//     bank_states_[bank_id].processed_words = 0;
//     bank_states_[bank_id].processed_count = 0; // 重置已处理计数
//     bank_states_[bank_id].bitmap_processing_complete = false;

//     // bank_states_[bank_id].bitmap_data = bitmap_data;
//     D_DEBUG("DECODER", "Bank %d: Bitmap decoded, total  elements: %zu", bank_id,
//             bank_states_[bank_id].total_elements);
//     // 3. 状态推进：收到 Bitmap 后，进入 DECODE_START/WF_REQ 阶段
//     bank_states_[bank_id].state = DecoderState::DECODE_START;
//     pending_request_[bank_id] = true; // 允许 Tick 尝试发送 W/F 请求
//     scheduleTickIfNeeded(0);          // 安排下一拍 Tick 尝试发送后续请求
//   }

//   void DecoderModule::handleWFResponse(uint32_t bank_id, PacketPtr pkt,
//                                        bool is_weight)
//   {
//     if (is_weight)
//     {
//       bank_states_[bank_id].weight_received = true;

//       weight_in_flight_[bank_id] = false;
//       D_DEBUG("DECODER", "Bank %d: Received WEIGHT response.", bank_id);
//     }
//     else
//     {
//       bank_states_[bank_id].feature_received = true;

//       feature_in_flight_[bank_id] = false;
//       D_DEBUG("DECODER", "Bank %d: Received FEATURE response.", bank_id);
//     }

//     // TBD: 更新 next_read_addr_w/f 以准备拉取下一个块的数据
//   }
//   uint64_t cal_cycle;
//   bool DecoderModule::checkBlockCompletion(uint32_t bank_id)
//   {
//     auto &state = bank_states_[bank_id];

//     // 只有在 WF_WAIT 状态，且 W 和 F 数据都收到后，才完成一个周期（一个32-bit
//     // word）的处理。
//     if (state.state == DecoderState::WF_WAIT && state.weight_received &&
//         state.feature_received)
//     {
//       bank_states_[bank_id].weight_received = false;
//       bank_states_[bank_id].feature_received = false;
//       in_flight_[bank_id] = false;
//       // 以当前索引为起点，累加两行（两个16-bit word），不足两行则仅一行
//       size_t start = state.processed_words;
//       size_t add_rows = 0;
//       size_t emitted0 = 0, emitted1 = 0;

//       if (start < state.total_rows)
//       {

//         emitted0 = state.row_ones_counts[start];
//         bank_wf_request_info_[bank_id].read4weight_nums += emitted0;
//         state.processed_count += emitted0;
//         add_rows += 1;
//       }
//       if (start + 1 < state.total_rows)
//       {
//         emitted1 = state.row_ones_counts[start + 1];
//         bank_wf_request_info_[bank_id].read4weight_nums += emitted1;
//         state.processed_count += emitted1;
//         add_rows += 1;
//       }

//       bank_wf_request_info_[bank_id].read4row_nums += add_rows;
//       if (bank_wf_request_info_[bank_id].read4row_nums >= FW_SIZE * BURST_NUM)
//       {

//         bank_wf_request_info_[bank_id].read4row_nums = 0;
//         bank_wf_request_info_[bank_id].feature_is_clear = true;
//       }
//       // 原始16-bit行数据（未解压）
//       uint16_t row0_bits = 0;
//       uint16_t row1_bits = 0;
//       if (start < state.total_rows)
//       {
//         row0_bits =
//             static_cast<uint16_t>(state.bitmap_pkt->getData()[start] & 0xFFFFu);
//       }
//       if (start + 1 < state.total_rows)
//       {
//         row1_bits = static_cast<uint16_t>(state.bitmap_pkt->getData()[start + 1] &
//                                           0xFFFFu);
//       }
//       state.processed_words += add_rows;
//       totall_num_output = totall_num_output + emitted0 + emitted1;
//       totall_num_input = totall_num_input + BITMAP_LINE_SIZE * 2;
//       //-BITMAP_SLICE_WORD_NUM_CFG_PER_ROW * 3 * SPARSITY
//       if (bank_wf_request_info_[bank_id].read4weight_nums >=
//           WT_SIZE * BURST_NUM)
//       {
//         bank_wf_request_info_[bank_id].read4weight_nums = 0;
//         bank_wf_request_info_[bank_id].weight_is_empty = true;
//       }
//       if (bank_id == 0)
//         D_DEBUG("CAM", "Bank %d: Emit two rows (idx %zu,%zu): ones=%zu,%zu",
//                 bank_id, start, start + (add_rows > 1 ? 1 : 0), emitted0,
//                 (add_rows > 1 ? emitted1 : 0));
//       D_DEBUG("DECODER",
//               "Bank %d: WF pair received. Progress rows %zu/%zu, ones %zu/%zu",
//               bank_id, state.processed_words, state.total_rows,
//               state.processed_count, state.total_elements);

//       // 将本cycle的两行(值+原始行bits)送入Hash CAM配对逻辑
//       Info2Cam_[bank_id].paired_success[0] = false;
//       Info2Cam_[bank_id].paired_success[1] = false;
//       Info2Cam_[bank_id].entry[0] = {static_cast<int>(emitted0),
//                                      static_cast<uint16_t>(row0_bits)};
//       Info2Cam_[bank_id].entry[1] = {static_cast<int>(emitted1),
//                                      static_cast<uint16_t>(row1_bits)};

//       // 检查是否还有更多行需要处理
//       if (state.processed_words < state.total_rows)
//       {
//         state.state = DecoderState::DECODE_START;
//         D_DEBUG("DECODER", "Bank %d: Next cycle. %zu rows remaining.", bank_id,
//                 (state.total_rows - state.processed_words) / 2);
//       }
//       else
//       {
//         if (file_stall[bank_id].current_addr_count == file_stall[bank_id].total_addr_count)
//         {
//           file_stall[bank_id].decoder_stall = true;
//           // file_stall[bank_id].current_addr_count = 0;
//         }
//         // 所有行处理完毕
//         state.bitmap_processing_complete = true;
//         // 刷新：输出剩余未配对项，按两行两行输出

//         state.state = DecoderState::IDLE;
//         D_DEBUG("DECODER",
//                 "Bank %d: All %zu rows processed. Bitmap processing complete "
//                 "(ones %zu).",
//                 bank_id, state.total_rows, state.total_elements);
//         // 释放bitmap包，避免内存泄漏
//         if (state.bitmap_pkt)
//         {
//           PacketManager::free_packet(state.bitmap_pkt);
//           state.bitmap_pkt = nullptr;
//         }
//       }

//       bool paired_success = processHashCam(bank_id);
//       if (paired_success)
//       {
//         if (!file_stall[bank_id].decoder_stall)
//           scheduleTickIfNeeded(1);
//         else
//           scheduleClearCamTick(1);
//       }
//       else
//       {
//         Info2Cam_[bank_id].retry2Cam_flag = true;

//         scheduleRetry2CamIfNeeded(1);
//       }
//       pending_request_[bank_id] = true;

//       // return true;
//     }
//     return true;
//   }
//   void DecoderModule::clearCamtick()
//   {

//     for (int bank_id = 0; bank_id < active_banks_; ++bank_id)
//     {
//       auto &Info2Cam = Info2Cam_[bank_id];
//       assert(Info2Cam.paired_success[0] || Info2Cam.paired_success[1]);
//       // 需要清空
//       for (int cam_idx = 0; cam_idx < 2; ++cam_idx)
//       {
//         auto &cam = hash_cam_[bank_id][cam_idx];
//         if (cam.size > 0)
//         {
//           bool evict_success = evictPairsLessThan16(bank_id, cam_idx);
//           D_INFO("CAM", "Bank %d: cam_idx %d CAM SIZE %d", bank_id, cam_idx, cam.size);
//           break;
//         }
//       }
//     }
//     bool break_all = false;
//     for (uint32_t i = 0; i < active_banks_; i++)
//     {
//       for (int cam_idx = 0; cam_idx < 2; ++cam_idx)
//       {
//         auto &cam = hash_cam_[i][cam_idx];
//         if (cam.size > 0)
//         {
//           break_all = true;
//           break;
//         }
//       }
//       if (break_all)
//       {

//         scheduleClearCamTick(1);
//         break;
//       }
//     }
//     if (!break_all)
//     {
//       for (size_t bank_id = 0; bank_id < active_banks_; bank_id++)
//         if (file_stall[bank_id].decoder_stall)
//         {
//           file_stall[bank_id].file_total_row = FILE_TOTAL_ROW_CFG;
//           file_stall[bank_id].file_total_col = FILE_TOTAL_COL_CFG;
//           file_stall[bank_id].file_slice_row = FILE_SLICE_ROW_CFG;
//           file_stall[bank_id].file_slice_col = FILE_SLICE_COL_CFG;
//           file_stall[bank_id].total_addr_count += file_stall[bank_id].file_slice_row * file_stall[bank_id].file_slice_col / BURST_BITS;
//           file_stall[bank_id].final_addr += (file_stall[bank_id].file_slice_row * file_stall[bank_id].file_slice_col / BURST_BITS * INST_ADDR_STRIDE - 1) - bank_id * CHANNEL_ADDR_DIF;
//           D_INFO("CAM", "bank_id :%d total_addr_count :%d final_addr : %d", bank_id, file_stall[bank_id].total_addr_count, file_stall[bank_id].final_addr);
//           file_stall[bank_id].decoder_stall = false;
//         }
//       scheduleTickIfNeeded(1);
//     }
//   }
//   int retry_num;
//   void DecoderModule::retry2CamTick()
//   {
//     for (int bank = 0; bank < active_banks_; ++bank)
//     {
//       if (Info2Cam_[bank].retry2Cam_flag)
//       {
//         if (bank == 0)
//           D_DEBUG("CAM", "Bank %d: Retry CAM. retry_num %d", bank, ++retry_num);
//         Info2Cam_[bank].retry2Cam_flag = false;
//         bool paired_flag = processHashCam(bank);
//         if (paired_flag)
//         {
//           if (!file_stall[bank].decoder_stall)
//             scheduleTickIfNeeded(0);
//           else
//             scheduleClearCamTick(1);
//         }
//         else
//         {
//           assert(false && "paired must success");
//           Info2Cam_[bank].retry2Cam_flag = true;
//           scheduleRetry2CamIfNeeded(1);
//         }
//       }
//     }
//   }
//   bool DecoderModule::processHashCam(uint32_t bank_id)
//   {
//     // 分别处理两行，各自对应一个CAM（不跨行配对）
//     auto &Info2Cam = Info2Cam_[bank_id];
//     auto &perf_stats = hash_cam_perf_stats_[bank_id];

//     if (Info2Cam.entry[0].value < 0 || Info2Cam.entry[0].value > 16)
//       assert(false && "Info2Cam.entry[0].value out of range");
//     if (Info2Cam.entry[1].value < 0 || Info2Cam.entry[1].value > 16)
//       assert(false && "Info2Cam.entry[1].value out of range");

//     bool all_success = true;
//     bool paired[2] = {false, false};
//     // 先尝试在各自CAM中直接配对
//     for (int cam_idx = 0; cam_idx < 2; ++cam_idx)
//     {
//       if (Info2Cam.entry[cam_idx].value <= 0 || Info2Cam.paired_success[cam_idx])
//         continue;
//       paired[cam_idx] =
//           tryInsertAndPair(bank_id, cam_idx, Info2Cam.entry[cam_idx].value,
//                            Info2Cam.entry[cam_idx].row_bits);
//       Info2Cam.paired_success[cam_idx] = paired[cam_idx];
//       if (Info2Cam.paired_success[cam_idx])
//         break;
//     }
//     // 未成功的，视各自cam压力进行逐个驱逐
//     if (!paired[0] && !paired[1])
//       for (int cam_idx = 0; cam_idx < 2; ++cam_idx)
//       {
//         if (!Info2Cam.paired_success[cam_idx])
//         {
//           auto &cam = hash_cam_[bank_id][cam_idx];
//           if (cam.size >= static_cast<size_t>(kAggressiveThreshold))
//           {
//             bool evict_success = evictPairsLessThan16(bank_id, cam_idx);
//             if (evict_success)
//               break;
//           }
//         }
//       }

//     // 再尝试仅插入
//     for (int cam_idx = 0; cam_idx < 2; ++cam_idx)
//     {
//       if (Info2Cam.entry[cam_idx].value > 0 &&
//           !Info2Cam.paired_success[cam_idx])
//       {
//         Info2Cam.paired_success[cam_idx] =
//             tryInsertOnly(bank_id, cam_idx, Info2Cam.entry[cam_idx].value,
//                           Info2Cam.entry[cam_idx].row_bits);
//         if (!Info2Cam.paired_success[cam_idx])
//         {
//           all_success = false;
//           perf_stats.cam_full_cycles++;
//         }
//       }
//     }
//     // 统计逻辑：
//     // - emit_paired_cycles: 有双数输出的cycle（可能同时也有单数输出）
//     // - emit_single_cycles: 只有单数输出（没有双数输出）的cycle

//     if (gSim->getCurTick() % 1000 == 0)
//     {
//       perf_stats.total_cycles = gSim->getCurTick();
//       if (bank_id == 0)
//         OUT << perf_stats.total_cycles << "," << cal_cycle << std::endl;
//       printHashCamPerfStats(bank_id);
//     }
//     return all_success;
//   }

//   bool DecoderModule::tryInsertAndPair(uint32_t bank_id, int cam_idx, int value,
//                                        uint16_t row_bits)
//   {

//     auto &cam = hash_cam_[bank_id][cam_idx];
//     const int need = 16 - value;
//     // 先尝试配对
//     auto it_bucket = cam.buckets.find(need);
//     if (it_bucket != cam.buckets.end() && !it_bucket->second.empty())
//     {
//       CamEntry partner = it_bucket->second.front();
//       it_bucket->second.pop_front();
//       // 如果bucket为空，则删除该bucket
//       if (it_bucket->second.empty())
//         cam.buckets.erase(need);
//       cam.size--;
//       emitPaired(bank_id, value, partner.value, row_bits, partner.row_bits,
//                  cam_idx);
//       return true; // 配对成功，已输出
//     }

//     // 配对不成功，不插入，返回false
//     return false;
//   }

//   bool DecoderModule::tryInsertOnly(uint32_t bank_id, int cam_idx, int value,
//                                     uint16_t row_bits)
//   {
//     auto &cam = hash_cam_[bank_id][cam_idx];
//     // 如果CAM满了，无法插入
//     if (cam.size >= static_cast<size_t>(kHashCamCapacity))
//     {
//       D_DEBUG("CAM", "Bank %d: HashCAM full, cannot insert, size=%zu", bank_id,
//               cam.size);
//       return false;
//     }
//     CamEntry e{value, row_bits};
//     cam.buckets[value].push_back(e); // 插入值
//     cam.size++;                      // 更新CAM大小
//     if (bank_id == 0)
//       D_DEBUG("CAM",
//               "Bank %d: HashCAM Insert value %d  -> emit cam_idx %d cam.size %d ",
//               bank_id, value, cam_idx, cam.size);
//     // std::cout <<"bank id :"<< bank_id<<"idx:  " <<cam_idx <<"  Cam size: " <<
//     // cam.size ; std::cout << "Cam value : " ; for (auto it2 =
//     // cam.buckets.begin(); it2 != cam.buckets.end(); ++it2)
//     // {
//     //   std::cout << it2->first <<"  ";
//     // }
//     // std::cout << " " << std::endl;

//     return true;
//   }

//   bool DecoderModule::evictPairsLessThan16(uint32_t bank_id, int cam_idx)
//   {
//     auto &cam = hash_cam_[bank_id][cam_idx];
//     // 找到第一个非空的值，立即pop出来
//     int val1 = 0;
//     CamEntry e1{0, 0};
//     bool found_first = false;
//     for (auto it = cam.buckets.begin(); it != cam.buckets.end(); ++it)
//     {
//       if (!it->second.empty())
//       {
//         val1 = it->first;
//         e1 = it->second.front();
//         it->second.pop_front();
//         found_first = true;
//         cam.size--;
//         break;
//       }
//     }
//     // 如果没有找到任何值，直接返回
//     if (!found_first)
//     {
//       return false;
//     }

//     // 尝试找配对（两值之和<=16）
//     bool found_pair = false;
//     for (auto it2 = cam.buckets.begin(); it2 != cam.buckets.end() && !found_pair;
//          ++it2)
//     {
//       if (it2->second.empty())
//         continue;
//       int val2 = it2->first;
//       if (val1 + val2 <= 16)
//       {
//         // 找到配对，配对输出
//         CamEntry e2 = it2->second.front();
//         it2->second.pop_front();
//         cam.size--;
//         emitPaired(bank_id, e1.value, e2.value, e1.row_bits, e2.row_bits,
//                    cam_idx);
//         found_pair = true;
//       }
//     }

//     // 如果找不到配对，单独输出第一个值
//     if (!found_pair)
//     {
//       emitSingle(bank_id, e1.value, e1.row_bits, cam_idx);
//     }
//     return true;
//   }
//   int parid_sub;
//   void DecoderModule::emitPaired(uint32_t bank_id, int a, int b,
//                                  uint16_t rowA_bits, uint16_t rowB_bits,
//                                  int cam_idx)
//   {
//     auto &cam = hash_cam_[bank_id][cam_idx];
//     // std::string filename =
//     // "/anlab12/stuhome/zngz39/Desktop/gnn/2025/LLM/simulator_simple/result/CAM.txt";
//     // exportHashCamStats(filename);
//     // 当前实现：仅记录日志；携带原始行bits供下游使用
//     cal_cycle += a + b;
//     hash_cam_perf_stats_[bank_id].emit_paired_cycles++;
//     if (bank_id == 0)
//     {
//       if (a + b < 16)
//         parid_sub++;
//       if (bank_id == 0)
//         D_INFO("CAM",
//                "Bank %d: HashCAM paired (%d + %d) = %d -> emit cam_idx %d "
//                "cam.size %d  parid_sub: %d",
//                bank_id, a, b, a + b, cam_idx, cam.size, parid_sub);
//     }
//     // 标记当前cycle有双数输出
//   }

//   void DecoderModule::emitSingle(uint32_t bank_id, int a, uint16_t rowA_bits,
//                                  int cam_idx)
//   {
//     auto &cam = hash_cam_[bank_id][cam_idx];

//     cal_cycle += a;
//     // 当前实现：仅记录日志；单独输出一个值（找不到配对时）
//     if (bank_id == 0)
//       D_INFO("CAM", "Bank %d: HashCAM single emit (%d)  cam_idx %d cam.size %d",
//              bank_id, a, cam_idx, cam.size);
//     // 标记当前cycle有单数输出
//     hash_cam_perf_stats_[bank_id].emit_single_cycles++;
//   }

//   void DecoderModule::exportHashCamStats(const std::string &filename) const
//   {
//     std::ofstream ofs(filename);
//     ofs << "bank_id,cam_idx,hits,inserts,evictions,still_in_cam\n";
//     for (size_t b = 0; b < hash_cam_.size(); ++b)
//     {
//       for (int cam_idx = 0; cam_idx < 2; ++cam_idx)
//       {
//         const auto &cam = hash_cam_[b][cam_idx];
//       }
//     }
//     ofs.close();
//   }

//   void DecoderModule::printHashCamPerfStats(uint32_t bank_id) const
//   {
//     D_INFO("CAM", "========== Hash CAM Performance Statistics ==========");

//     const auto &stats = hash_cam_perf_stats_[bank_id];
//     if (stats.total_cycles == 0)
//     {
//       D_INFO("CAM", "[Bank %zu] No cycles recorded", bank_id);
//     }
//     double cam_full_ratio =
//         (double)stats.cam_full_cycles / stats.total_cycles * 100.0;
//     double emit_single_ratio =
//         (double)stats.emit_single_cycles / stats.total_cycles * 100.0;
//     double emit_paired_ratio =
//         (double)stats.emit_paired_cycles / stats.total_cycles * 100.0;
//     D_INFO("CAM", "[Bank %zu] storage_addr_max: %llu", bank_id,
//            GNN::storage_addr_max);

//     //  for (int i=0;i<8;i++) {
//     //   D_INFO("CAM", "bm_total_cycle[%d] %d",i, bm_total_cycle[i]);
//     //   D_INFO("CAM", "wt_total_cycle[%d] %d",i, wt_total_cycle[i]);
//     //   D_INFO("CAM", "fw_total_cycle[%d] %d",i, fw_total_cycle[i]);
//     //  }

//     D_INFO("CAM", "[Bank %zu] storage_number: %llu", bank_id,
//            GNN::storage_number);
//     D_INFO("CAM", "[Bank %zu] Total input number: %llu", bank_id,
//            totall_num_input);
//     D_INFO("CAM", "[Bank %zu] Total output number: %llu", bank_id,
//            totall_num_output);
//     D_INFO("CAM", "[Bank %zu] Total proportion : %.2f%%", bank_id,
//            (double)totall_num_output / totall_num_input * 100.0);
//     D_INFO("CAM", "[Bank %zu] Total cycles: %llu", bank_id, stats.total_cycles);
//     D_INFO("CAM", "[Bank %zu] CAM full cycles: %llu (%.2f%%)", bank_id,
//            stats.cam_full_cycles, cam_full_ratio);
//     D_INFO("CAM", "[Bank %zu] Emit single cycles: %llu (%.2f%%)", bank_id,
//            stats.emit_single_cycles, emit_single_ratio);
//     D_INFO("CAM", "[Bank %zu] Emit paired cycles: %llu (%.2f%%)", bank_id,
//            stats.emit_paired_cycles, emit_paired_ratio);
//     D_INFO("CAM", "[Bank %zu] Emit proportion: %.2f%%", bank_id,
//            (double)stats.emit_paired_cycles * 2 /
//                (stats.emit_paired_cycles * 2 + stats.emit_single_cycles) * 100.0);
//     uint64_t dram_bw = dram_burst_num * 2 / 8;
//     uint64_t mac_bw = 8 * 16 * 2;
//     double mac_u = (double)cal_cycle / stats.total_cycles / 8 / 16;
//     double dram_u = (double)dram_bw / stats.total_cycles;
//     D_INFO("CAM", "MAC cycle: %llu  cycle", cal_cycle / 8);
//     D_INFO("CAM", "DRAM Output Bytes Number : %llu  cycle", dram_bw);
//     D_INFO("CAM", "MAC BW :  %.2f%% ", mac_u * 100);
//     D_INFO("CAM", "DRAM BW : %.2f%%", dram_u * 100);
//     // D_INFO("CAM", "Total proportion :
//     // %.2f%%",(double)totall_num_output/totall_num_input * 100.0);
//     D_INFO("CAM", "======================================================");
//   }

//   void DecoderModule::exportHashCamPerfStats(const std::string &filename) const
//   {
//     std::ofstream ofs(filename);
//     ofs << "bank_id,total_cycles,cam_full_cycles,cam_full_ratio,"
//         << "emit_single_cycles,emit_single_ratio,"
//         << "emit_paired_cycles,emit_paired_ratio\n";
//     for (size_t b = 0; b < hash_cam_perf_stats_.size(); ++b)
//     {
//       const auto &stats = hash_cam_perf_stats_[b];
//       double cam_full_ratio =
//           stats.total_cycles > 0
//               ? (double)stats.cam_full_cycles / stats.total_cycles * 100.0
//               : 0.0;
//       double emit_single_ratio =
//           stats.total_cycles > 0
//               ? (double)stats.emit_single_cycles / stats.total_cycles * 100.0
//               : 0.0;
//       double emit_paired_ratio =
//           stats.total_cycles > 0
//               ? (double)stats.emit_paired_cycles / stats.total_cycles * 100.0
//               : 0.0;

//       ofs << b << "," << stats.total_cycles << "," << stats.cam_full_cycles << ","
//           << cam_full_ratio << "," << stats.emit_single_cycles << ","
//           << emit_single_ratio << "," << stats.emit_paired_cycles << ","
//           << emit_paired_ratio << "\n";
//     }
//     ofs.close();
//   }

//   // =======================================================================
//   // Port 接口
//   // =======================================================================

//   Port &DecoderModule::getPort(const std::string &if_name, int idx)
//   {
//     std::stringstream ss(if_name);
//     std::string type;
//     std::string side;

//     // 解析端口名格式: type_sideN (e.g., bmap_side0, w_side1)
//     // D_INFO("DECODER", "if_name: %s, checking against: %s", if_name.c_str(),
//     // bitmapRequestPorts[idx].name().c_str());
//     if (if_name.rfind(bitmapRequestPorts[idx].name(), 0) == 0)
//     {
//       int bank = std::stoi(if_name.substr(if_name.length() - 1));
//       if (bank >= 0 && bank < active_banks_)
//         return bitmapRequestPorts[bank];
//     }
//     if (if_name.rfind(featureRequestPorts[idx].name(), 0) == 0)
//     {
//       int bank = std::stoi(if_name.substr(if_name.length() - 1));
//       if (bank >= 0 && bank < active_banks_)
//         return featureRequestPorts[bank];
//     }
//     if (if_name.rfind(weightRequestPorts[idx].name(), 0) == 0)
//     {
//       int bank = std::stoi(if_name.substr(if_name.length() - 1));
//       if (bank >= 0 && bank < active_banks_)
//         return weightRequestPorts[bank];
//     }
//     if (if_name.rfind(computresponsePort[idx].name(), 0) == 0)
//     {
//       int bank = std::stoi(if_name.substr(if_name.length() - 1));
//       if (bank >= 0 && bank < active_banks_)
//         return computresponsePort[bank];
//     }

//     throw std::runtime_error("No such port: " + if_name);
//   }

//   // CompRequestPort::recvTimingResp 统一调用 recvTimingResp
//   // (这里是 Port 内部类，需要根据 BankName 映射回类型)
//   // 注意：由于在 Port 构造函数中传入了类型，这里简化处理 Port 内部的
//   // recvTimingResp 确保 BankRequestPort 构造时传入了正确的类型字符串。
// } // namespace GNN