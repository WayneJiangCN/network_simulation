#include "dma/DmaBuffer.h"
#include "common/packet.h"

namespace GNN
{
  constexpr int DmaBuffer::num_ports;
  constexpr int DmaBuffer::addr_stride;

  DmaBuffer::DmaBuffer(const std::string &name, addr_t base_addr, int row_words, int active_banks)
      : SimObject(name), base_addr_(base_addr), row_words_(row_words),
        active_banks_(std::min(active_banks, num_ports)), trans_state_(IDLE),
        addr_stride_(active_banks * addr_stride), current_buf_idx_(1),
        sendRespondEvent([this]
                         { sendRespond(); }, name + ".sendRespondEvent"),
        tickEvent([this]
                  { tick(); }, name + ".tickEvent")
  {
    inst_cnt = 0;
    requestPorts.reserve(num_ports);
    req_fifos_.resize(num_ports);
    for (int i = 0; i < num_ports; ++i)
    {
      requestPorts.emplace_back(name + ".dma_side" + std::to_string(i), *this, i);
    }
    // 构造计算侧端口与队列
    computePorts.reserve(num_ports);
    compute_resp_fifos_.resize(num_ports);
    for (int i = 0; i < num_ports; ++i)
    {
      computePorts.emplace_back(name + ".comp_side" + std::to_string(i), *this, i);
    }

    bank_controllers_.resize(active_banks_);
    for (int i = 0; i < active_banks_; ++i)
    {
      response_retryResp[i] = false;
      response_retryReq[i] = false;
      request_retryResp[i] = false;
      next_read_idx_[i] = 0;
    }
  }

  // DmaRequestPort inner class implementation
  DmaBuffer::DmaRequestPort::DmaRequestPort(const std::string &name, DmaBuffer &o, int id) : RequestPort(name), owner(o), port_id(id) {}
  bool DmaBuffer::DmaRequestPort::recvTimingResp(PacketPtr pkt) { return owner.recvTimingResp(pkt, port_id); }
  void DmaBuffer::DmaRequestPort::recvReqRetry() { owner.sendRetryReq(port_id); }

  // ComputeSidePort implementation
  DmaBuffer::ComputeSidePort::ComputeSidePort(const std::string &name, DmaBuffer &o, int id)
      : ResponsePort(name), owner(o), bank_id(id) {}

  bool DmaBuffer::ComputeSidePort::tryTiming(PacketPtr pkt)
  {
    // 仅当该bank存在可读FULL缓冲才允许请求
    int idx = owner.getReadableBufferIndex(bank_id);
    return idx >= 0;
  }

  bool DmaBuffer::recvTimingReq(PacketPtr pkt, uint32_t bank_id)
  {

    int idx = getReadableBufferIndex(bank_id);
    if (idx < 0)
    {
      // 没有可读数据，拒绝请求
      D_WARN("DMA", "[DMA] recvTimingReq,没有可读数据，拒绝请求");
      response_retryReq[bank_id] = true;
      return false;
    }

    if (!sendRespondEvent.scheduled())
    {
      schedule(sendRespondEvent, curTick() + 1);
    }
    return true;
  }
  void DmaBuffer::sendRespond()
  {
    // 构造响应包，搬运该FULL缓冲数据
    for (int bank = 0; bank < active_banks_; bank++)
    {
      //   assert(!response_retryResp[bank]);
      if (!response_retryResp[bank])
      {
        int idx = getReadableBufferIndex(bank);
        if (idx < 0)
          continue;
        assert(!bank_controllers_[bank].buffers[idx].dma_pkt.empty());
        auto &dq = bank_controllers_[bank].buffers[idx].dma_pkt;

        // D_INFO("DMA", "[DMA]dq.size()：%d,addr:%d", dq.size(), dq.front()->getAddr());
        // D_INFO("DMA", "[DMA]dq.size()：%d,addr:%d", dq.size(), dq.back()->getAddr());
        int batch = std::min(static_cast<int>(dq.size()), CAL_GRAN);

        // 将前 batch 个包合并成一个包（复用第一个包作为合并后的承载）
        PacketPtr _pkt = dq.front();
        if (batch > 1)
        {
          std::vector<uint32_t> merged = _pkt->getData();
          merged.reserve(merged.size() + 1024); // 预留一些空间，避免多次扩容
          size_t total_size = _pkt->getSize();
          for (int i = 1; i < batch; ++i)
          {
            PacketPtr p = dq[i];
            const auto &d = p->getData();
            if (!d.empty())
              merged.insert(merged.end(), d.begin(), d.end());
            total_size += p->getSize();
          }
          _pkt->setData(merged);
          _pkt->setSize(total_size);
        }

        _pkt->setBankId(bank);
        _pkt->setBufferIdx(idx);
        // 直接尝试发送响应
        bool success = computePorts[bank].sendTimingResp(_pkt);
        if (success)
        {
          // 发送成功，DMA释放该缓冲
          // D_INFO("DMA", "释放缓冲: bank=%d, idx=%d", bank, idx);
          // 弹出并在做合并时释放多余的包
          if (batch <= 1)
          {
            dq.erase(dq.begin());
          }
          else
          {
            // 保留第一个（被复用发送的）不释放，由下游负责释放
            dq.erase(dq.begin());
            for (int i = 1; i < batch; ++i)
            {
              PacketManager::free_packet(dq.front());
              dq.erase(dq.begin());
            }
          }
          if (dq.empty())
            releaseBankBuffer(bank, idx);
          else
          {
            if (!sendRespondEvent.scheduled())
              schedule(sendRespondEvent, curTick() + 1);
          }
        }
        else
        {
          // 对端暂时无法接收，排入响应队列，等待对端resp retry
          D_INFO("DMA", "响应发送失败: bank=%d", bank);
          compute_resp_fifos_[bank].push_back(_pkt);
          response_retryResp[bank] = true;
        }
      }
    }
  }
  void DmaBuffer::ComputeSidePort::recvRespRetry()
  {
    // 对端可以再次接收响应，尝试发送队列中的响应
    auto &q = owner.compute_resp_fifos_[bank_id];
    if (!q.empty())
    {
      PacketPtr pkt = q.front();
      if (sendTimingResp(pkt))
      {
        q.pop_front();
        // 发送成功后释放对应缓冲
        D_INFO("DMA", "释放缓冲: bank=%d, idx=%d", bank_id, pkt->getBufferIdx());
        owner.releaseBankBuffer(bank_id, pkt->getBufferIdx());
        owner.response_retryResp[bank_id] = false;
      }
    }
  }

  void DmaBuffer::init() {}

  void DmaBuffer::enqueueCommand(const DmaCommand &cmd)
  {
    // 检查是否还能接受新指令：当前指令数不能超过2（两个buf）
    if (inst_cnt >= 2)
    {
      D_INFO("DMA", "Command %lu rejected: already have %d instructions (max 2 for dual buffer)", cmd.cmd_id, inst_cnt);
      return;
    }

    cmd_queue_.push_back(cmd);
    D_INFO("DMA", "Command %lu enqueued. Current inst_cnt: %d", cmd.cmd_id, inst_cnt);
    schedule_tick_if_needed();
  }

  void DmaBuffer::tick()
  {
    // 1. 状态机驱动核心逻辑
    if (trans_state_ == IDLE)
    {
      // 只有在指令数小于2且队列不为空时才能处理新指令
      if (inst_cnt < 2 && !cmd_queue_.empty())
      {
        // 从队列中获取新命令
        current_cmd_ = cmd_queue_.front();
        cmd_queue_.pop_front();
        lines_fetched_for_cmd_ = 0;
        trans_state_ = CONFIG;
        D_INFO("DMA", "Starting command %lu: base_addr=%#d, lines=%d, inst_cnt will be %d",
               current_cmd_.cmd_id, current_cmd_.base_addr, current_cmd_.total_lines, inst_cnt + 1);
      }
      else if (inst_cnt >= 2)
      {
        // 指令数已满，等待现有指令完成
        D_INFO("DMA", "Cannot start new command: inst_cnt=%d (max 2), queue_size=%zu",
               inst_cnt, cmd_queue_.size());
        return;
      }
      else
      {
        // 队列为空，保持空闲
        return;
      }
    }

    if (trans_state_ == CONFIG)
    {
      inst_cnt++;
      int ori_ch = (current_cmd_.base_addr % 0x100) / 0x40;

      // 切换buf索引：当前命令使用另一个buf
      current_buf_idx_ = 1 - current_buf_idx_;

      for (int i = 0; i < active_banks_; ++i)
      {
        addr_t dram_bias_addr = i >= ori_ch ? (i - ori_ch) * addr_stride : (i - ori_ch + active_banks_) * addr_stride;
        bank_rd_addr_[i] = current_cmd_.base_addr + dram_bias_addr;

        auto &controller = bank_controllers_[i];
        // 使用当前命令指定的buf索引
        int write_idx = current_buf_idx_;
         if (controller.buffers[write_idx].state == BufferState::FILLING)
         {
           controller.dram_base_addr[write_idx] = bank_rd_addr_[i];
           controller.dram_final_addr[write_idx] = bank_rd_addr_[i] + static_cast<addr_t>(current_cmd_.total_lines) * addr_stride_;
           controller.buffers[write_idx].state = BufferState::FILLING;
           controller.buffers[write_idx].words_written = 0;
           controller.buffers[write_idx].dma_pkt.clear();
           controller.buffers[write_idx].dma_pkt.reserve(row_words_);
           bank_transfer_active_[i] = true;
           // 确保该buf的stall状态为false
           controller.stalled[write_idx] = false;
           D_INFO("DMA", "Starting command %lu: base_addr=%#d, lines=%d, inst_cnt  be %d, dram_final_addr=%#d",
                  current_cmd_.cmd_id, current_cmd_.base_addr, current_cmd_.total_lines, inst_cnt, controller.dram_final_addr[write_idx]);
           // 记录该buf对应的命令ID和回调
           buf_cmd_id_[write_idx] = current_cmd_.cmd_id;
           buf_cmd_callback_[write_idx] = current_cmd_.completion_callback;
         }
      }
      trans_state_ = STREAMING;
    }

    if (trans_state_ == STREAMING)
    {
      // 检查是否所有行的请求都已发出
      if (lines_fetched_for_cmd_ >= current_cmd_.total_lines)
      {
        trans_state_ = IDLE;
        // 请求已全部发出，等待DRAM响应。tick会继续运行直到命令完成。
      }
      else
      {
        // 为每个活动的、未暂停的bank生成DRAM读请求
        for (int i = 0; i < active_banks_; ++i)
        {
          if (bank_transfer_active_[i] && !bank_controllers_[i].stalled[current_buf_idx_])
          {
            PacketPtr read_pkt = PacketManager::create_read_packet(bank_rd_addr_[i], BURST_BITS / WORD_SIZE);
            req_fifos_[i].push_back(read_pkt);
           
            addr_t final_addr_for_cmd = current_cmd_.base_addr + static_cast<addr_t>(current_cmd_.total_lines) * addr_stride_;
             D_INFO("DMA", "bank_rd_addr_[i]=%#d, final_addr_for_cmd=%#d,lines_fetched_for_cmd_=%d", bank_rd_addr_[i], final_addr_for_cmd,lines_fetched_for_cmd_);
            bank_rd_addr_[i] += addr_stride_;

            if (bank_rd_addr_[i] >= final_addr_for_cmd)
            {
              bank_transfer_active_[i] = false;
            }
          }
        }
        lines_fetched_for_cmd_++;
      }
    }

    // 2. 发送所有队列中的请求
    for (int i = 0; i < num_ports; ++i)
    {
      if (!req_fifos_[i].empty())
      {
        PacketPtr pkt = req_fifos_[i].front();
        if (requestPorts[i].sendTimingReq(pkt))
        {
          req_fifos_[i].pop_front();
        }
      }
    }

    // 3. 只要有活动命令或队列不为空，就继续调度
    if (trans_state_ != IDLE || !cmd_queue_.empty())
    {
      if (!tickEvent.scheduled())
      {
        schedule(tickEvent, curTick() + 1);
      }
    }
  }

  bool DmaBuffer::recvTimingResp(PacketPtr pkt, int port_id)
  {
    if (pkt->isRead())
    {
      auto &controller = bank_controllers_[port_id];
      addr_t pkt_addr = pkt->getAddr();

      // 根据地址区间判断数据属于哪个buf
      int target_buf_idx = -1;
      for (int buf_idx = 0; buf_idx < 2; ++buf_idx)
      {
        if (pkt_addr >= controller.dram_base_addr[buf_idx] &&
            pkt_addr < controller.dram_final_addr[buf_idx])
        {
          target_buf_idx = buf_idx;
          break;
        }
      }

      if (target_buf_idx == -1)
      {
        D_INFO("DMA", "无法确定地址 %#x 属于哪个buf，拒绝响应", pkt_addr);
        return false;
      }

      auto &buffer = controller.buffers[target_buf_idx];
      D_INFO("DMA", "Bank %d 地址 %#d 路由到 buf %d, state: %d", port_id, pkt_addr, target_buf_idx, static_cast<int>(buffer.state));

      if (buffer.state == BufferState::FILLING)
      {
        int idx = (pkt_addr - controller.dram_base_addr[target_buf_idx]) / addr_stride_;
        // D_INFO("DMA", "addr %#x idx %d", pkt_addr, idx);
        if (idx >= static_cast<int>(buffer.dma_pkt.size()))
          buffer.dma_pkt.resize(idx + 1);
        buffer.dma_pkt[idx] = pkt;
        if (!buffer.dma_pkt.empty() && buffer.dma_pkt.front() != nullptr)
          // D_INFO("DMA", "[DMA]dq.size()：%d,addr:%#d", buffer.dma_pkt.size(), buffer.dma_pkt.front()->getAddr());
          //  D_INFO("DMA", "[DMA]dq.size()：%d,addr back:%#d", buffer.dma_pkt.size(), buffer.dma_pkt.back()->getAddr());
          buffer.words_written += 1;
           D_INFO("DMA", " buffer.words_written   %d", buffer.words_written);
        if (buffer.words_written >= row_words_)
        {
          buffer.state = BufferState::FULL;
          // 使用该buf对应的命令ID，而不是当前命令ID
          D_INFO("DMA", "Bank %d Buffer %d is FULL for cmd %lu.", port_id, target_buf_idx, buf_cmd_id_[target_buf_idx]);

          // 检查该buf对应的命令是否完成
          check_buf_cmd_completion(target_buf_idx);
          if(target_buf_idx==next_read_idx_[port_id])
          maybe_notify_compute_full(port_id);

          // 不再自动切换到下一个buf，因为buf选择由命令控制
          controller.stalled[target_buf_idx] = true; // 当前buf已满，暂停该bank的该buf
        }
      }
      else if (buffer.state == BufferState::FULL)
      {
        D_INFO("DMA", "目标buf %d 已满，无法再接收DRAM响应，标记重试并拒绝 DMA请求: bank=%d", target_buf_idx, port_id);
        // 目标buf已满，无法再接收DRAM响应，标记重试并拒绝
        request_retryResp[port_id] = true;
        return false;
      }
      else
      {
        D_INFO("DMA", "目标buf %d 状态为 %d，无法接收数据", target_buf_idx, static_cast<int>(buffer.state));
        return false;
      }
    }

    return true;
  }

  void DmaBuffer::check_buf_cmd_completion(int buf_idx)
  {
    // 检查该buf对应的命令是否完成：所有bank的该buf都已填满
    bool all_bufs_full = true;
    for (int i = 0; i < active_banks_; ++i)
    {
      auto &controller = bank_controllers_[i];
      if (controller.buffers[buf_idx].state != BufferState::FULL)
      {
        all_bufs_full = false;
        break;
      }
    }

    if (all_bufs_full)
    {
      // 该buf对应的命令完成
      uint64_t completed_cmd_id = buf_cmd_id_[buf_idx];
      auto callback = buf_cmd_callback_[buf_idx];

      D_INFO("DMA", "Command %lu completed (DMA fetch part). All %d banks buf %d are FULL. Firing callback.",
             completed_cmd_id, active_banks_, buf_idx);

      if (callback)
      {
        callback(completed_cmd_id);
        // 清空回调，防止重复调用
        buf_cmd_callback_[buf_idx] = nullptr;
      }

      // 如果这是当前命令，则返回IDLE状态
      if (buf_idx == current_buf_idx_)
      {
        trans_state_ = IDLE;
      }
    }
  }

  void DmaBuffer::check_current_cmd_completion()
  {
    // 检查是否所有请求都已发出
    if (lines_fetched_for_cmd_ < current_cmd_.total_lines)
    {
      return; // 请求尚未全部发出
    }

    // 检查当前命令是否完成
    check_buf_cmd_completion(current_buf_idx_);
  }

  void DmaBuffer::releaseBankBuffer(int bank_id, int buffer_idx)
  {
    assert(bank_id >= 0 && bank_id < active_banks_ && (buffer_idx == 0 || buffer_idx == 1));
    auto &controller = bank_controllers_[bank_id];

    if (controller.buffers[buffer_idx].state == BufferState::FULL)
    {
      controller.buffers[buffer_idx].state = BufferState::FILLING;
      controller.buffers[buffer_idx].words_written = 0;
      controller.buffers[buffer_idx].dma_pkt.clear();
      controller.buffers[buffer_idx].dma_pkt.reserve(row_words_);

      D_INFO("DMA", "Bank %d Buffer %d released by consumer.", bank_id, buffer_idx);
      next_read_idx_[bank_id] = !next_read_idx_[bank_id];

      // 检查是否所有bank的指定buf都已被释放
      bool all_specified_bufs_released = true;
      for (int i = 0; i < active_banks_; ++i)
      {
        auto &ctrl = bank_controllers_[i];
        if (ctrl.buffers[buffer_idx].state == BufferState::FULL)
        {
          all_specified_bufs_released = false;
          break;
        }
      }

      // 只有当所有bank的指定buf都被释放时，才减少指令计数
      if (all_specified_bufs_released)
      {
        inst_cnt -= 1;
        D_INFO("DMA", "All banks buf %d released, inst_cnt decreased to %d", buffer_idx, inst_cnt);
      }

      // 如果该bank的该buf被暂停，现在可以恢复
      if (controller.stalled[buffer_idx])
      {
        controller.stalled[buffer_idx] = false;
        D_INFO("DMA", "Bank %d buf %d unstalled after release.", bank_id, buffer_idx);
      }

      // 如果之前由于写缓冲满而拒绝过DRAM响应，则在释放后通知上游重试
      if (request_retryResp[bank_id])
      {
        D_INFO("DMA", "Bank %d request retryResp.", bank_id);
        request_retryResp[bank_id] = false;
        requestPorts[bank_id].sendRetryResp();
      }
    }
    schedule_tick_if_needed();
  }

  int DmaBuffer::getReadableBufferIndex(int bank_id) const
  {
    const auto &controller = bank_controllers_[bank_id];
    bool full0 = controller.buffers[0].state == BufferState::FULL;
    bool full1 = controller.buffers[1].state == BufferState::FULL;
    if (full0 || full1)
      // 轮转选择，避免总是取同一侧
      return next_read_idx_[bank_id];

    return -1;
  }
  void DmaBuffer::sendRetryReq(int port_id) { schedule_tick_if_needed(); }

  void DmaBuffer::schedule_tick_if_needed()
  {
    if (!tickEvent.scheduled() && (trans_state_ != IDLE || !cmd_queue_.empty()))
    {
      schedule(tickEvent, curTick() + 1);
    }
  }

  Port &DmaBuffer::getPort(const std::string &if_name, int idx)
  {
    if (if_name.rfind("dma_side", 0) == 0)
    {
      int bank = std::stoi(if_name.substr(8));
      if (bank >= 0 && bank < num_ports)
        return requestPorts[bank];
    }
    if (if_name.rfind("comp_side", 0) == 0)
    {
      int bank = std::stoi(if_name.substr(9));
      if (bank >= 0 && bank < num_ports)
        return computePorts[bank];
    }
    throw std::runtime_error("No such port: " + if_name);
  }

  void DmaBuffer::maybe_notify_compute_full(int bank_id)
  {
    // 如果该bank之前有请求被拒，或当前存在FULL缓冲，尝试唤醒计算侧
    if (response_retryReq[bank_id])
    {
      response_retryReq[bank_id] = false;
      computePorts[bank_id].sendRetryReq();
    }
    else
    {
      // 可选的主动通知：根据需要开启
      // computePorts[bank_id].sendRetryReq();
    }
  }

} // namespace GNN
