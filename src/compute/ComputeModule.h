/*
 * 计算模块：从 DmaBuffer 取已满缓冲，做简单处理后释放缓冲。
 */

#ifndef GNN_COMPUTE_MODULE_H_
#define GNN_COMPUTE_MODULE_H_

#include "common/object.h"
#include "event/eventq.h"
#include "dma/DmaBuffer.h"
#include <string>
#include <vector>

namespace GNN {

class ComputeModule : public SimObject {
public:
  ComputeModule(const std::string &name, DmaBuffer *dma, int active_banks,
                int poll_interval = 1)
      : SimObject(name), dma_(dma), active_banks_(active_banks),
        poll_interval_(poll_interval),
        tickEvent([this] { tick(); }, name + ".tickEvent") {
    processed_chunks_per_bank_.assign(active_banks_, 0);
  }

  void init() override {
    if (!tickEvent.scheduled())
      schedule(tickEvent, curTick() + poll_interval_);
  }

private:
  DmaBuffer *dma_;
  int active_banks_;
  int poll_interval_;
  std::vector<long long> processed_chunks_per_bank_;
  EventFunctionWrapper tickEvent;

  void tick() {
    // 轮询各 bank，读取已满缓冲区
    for (int bank = 0; bank < active_banks_; ++bank) {
      int idx = dma_->getReadableBufferIndex(bank);
      if (idx >= 0) {
        const auto &data = dma_->getBankData(bank, idx);
        // 简单“计算”：累加前若干元素（示例）
        long long sum = 0;
        for (size_t i = 0; i < data.size(); ++i) {
          sum += data[i];
        }
        processed_chunks_per_bank_[bank] += 1;
        D_INFO("COMPUTE", "Bank %d consumed buffer %d, words=%zu, sum=%lld, total=%lld",
               bank, idx, data.size(), sum, processed_chunks_per_bank_[bank]);

        // 释放该缓冲区，DMA 可继续写入
        dma_->releaseBankBuffer(bank, idx);
      }
    }

    if (!tickEvent.scheduled())
      schedule(tickEvent, curTick() + poll_interval_);
  }
};

} // namespace GNN

#endif // GNN_COMPUTE_MODULE_H_


