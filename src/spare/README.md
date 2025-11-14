# BitmapBank 继承 DmaBuffer 实现

## 概述

BitmapBank 成功继承了 DmaBuffer，实现了稀疏位图数据的存储和管理功能。

## 继承关系

```
SimObject
    └── DmaBuffer
            └── BitmapBank
```

## 主要特性

### 1. 继承DmaBuffer的所有功能
- **端口管理**: 自动获得 `dma_side0-7` 和 `comp_side0-7` 端口
- **DMA命令处理**: 支持异步DMA数据传输
- **双缓冲机制**: 继承DmaBuffer的双缓冲设计
- **事件驱动**: 集成到全局事件队列系统

### 2. BitmapBank特有功能
- **位图存储**: 管理多个稀疏位图
- **地址映射**: 自动计算位图在SRAM中的地址
- **缓存机制**: 缓存常用行数据
- **稀疏度计算**: 自动计算和更新稀疏度

## 文件结构

```
src/spare/
├── SparseTensor.h      # 稀疏张量和位图数据结构
├── SparseBanks.h       # BitmapBank类声明
├── BitmapBank.cpp      # BitmapBank类实现
├── test_bitmap.cpp     # 测试程序
└── Makefile           # 编译配置
```

## 核心实现

### 构造函数
```cpp
BitmapBank::BitmapBank(const std::string& name, addr_t base_addr, int row_words, int active_banks)
    : DmaBuffer(name, base_addr, row_words, active_banks), bitmap_base_addr_(base_addr) {}
```

### 端口访问
```cpp
Port &BitmapBank::getPort(const std::string &if_name, int idx) {
    return DmaBuffer::getPort(if_name, idx);
}
```

### 位图管理
```cpp
void BitmapBank::loadBitmap(const std::string& name, const SparseBitmap& bitmap) {
    bitmaps_[name] = std::unique_ptr<SparseBitmap>(new SparseBitmap(bitmap));
    addr_t bitmap_addr = bitmap_base_addr_ + bitmap_addresses_.size() * bitmap.getHeight() * bitmap.getWidth();
    bitmap_addresses_[name] = bitmap_addr;
    loadBitmapToSRAM(name, bitmap);
}
```

## 使用方式

### 1. 创建BitmapBank
```cpp
BitmapBank bitmap_bank("test_bank", 0x1000, 8, 4);
bitmap_bank.init();
```

### 2. 加载位图
```cpp
SparseBitmap bitmap(8, 8);
bitmap.generateRandom(0.3);
bitmap_bank.loadBitmap("my_bitmap", bitmap);
```

### 3. 端口绑定
```cpp
// DMA端口绑定
Port& dma_port = bitmap_bank.getPort("dma_side0");
dma_port.bind(external_dma_port);

// 计算端口绑定
Port& comp_port = bitmap_bank.getPort("comp_side0");
comp_port.bind(compute_module_port);
```

### 4. 数据访问
```cpp
// 获取位图
const SparseBitmap* bitmap = bitmap_bank.getBitmap("my_bitmap");

// 获取行数据
auto row_data = bitmap_bank.getRowBitmap("my_bitmap", 0);

// 计算非零元素
size_t non_zeros = bitmap_bank.countNonZeros("my_bitmap", 0);
```

## 端口说明

### DMA端口 (dma_side0-7)
- 用于与外部DMA控制器通信
- 支持数据加载和传输
- 继承自DmaBuffer的RequestPort

### 计算端口 (comp_side0-7)
- 用于与计算模块通信
- 支持数据读取和访问
- 继承自DmaBuffer的ResponsePort

## 优势

1. **代码复用**: 充分利用DmaBuffer的成熟实现
2. **接口一致**: 与DmaBuffer保持相同的端口接口
3. **功能扩展**: 在DmaBuffer基础上添加位图管理功能
4. **维护简单**: 减少重复代码，降低维护成本

## 测试

运行测试程序验证功能：
```bash
cd src/spare/
make run
```

## 注意事项

1. **地址管理**: 使用 `bitmap_base_addr_` 存储基础地址
2. **内存管理**: 使用智能指针管理位图对象
3. **端口命名**: 直接使用DmaBuffer的端口命名约定
4. **异步处理**: DMA命令是异步执行的

## 扩展功能

可以基于这个继承关系扩展：
- 支持更多位图格式
- 添加位图压缩
- 实现位图缓存策略
- 支持动态位图更新
- 添加性能统计
