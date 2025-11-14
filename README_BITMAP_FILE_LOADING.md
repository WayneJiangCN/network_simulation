# BitmapBank 文本文件读取功能

## 功能概述

`BitmapBank` 现在支持从文本文件读取位图数据，提供了灵活的文本格式支持和批量加载功能。

## 支持的文件格式

### 1. 基本格式
- **0/1 格式**: `1 0 1 0 1`
- **true/false 格式**: `true false true false`
- **T/F 格式**: `T F T F`
- **True/False 格式**: `True False True False`
- **数字格式**: `1 0 2 0 3` (非零值视为true，零值视为false)

### 2. 特殊规则
- 以 `#` 开头的行被视为注释
- 空行会被忽略
- 所有行的列数必须一致
- 文件扩展名应为 `.txt`

## 示例文件格式

### example_bitmap.txt
```
# Example bitmap data file
# This is a comment line
# Format: space-separated 0/1 values

1 0 1 0 1
0 1 0 1 0
1 1 0 0 1
0 0 1 1 0
1 0 0 1 1
```

### sparse_pattern.txt
```
# Another bitmap example with true/false format
true false true false
false true false true
true true false false
false false true true
```

### numeric_bitmap.txt
```
# Numeric format bitmap
# Non-zero values are treated as true, zero as false
1 0 2 0 3
0 4 0 5 0
6 7 0 0 8
0 0 9 10 0
11 0 0 12 13
```

## API 使用方法

### 1. 加载单个文件
```cpp
BitmapBank bitmap_bank("test_bank", 0x1000, 64, 8);

// 加载单个位图文件
bool success = bitmap_bank.loadBitmapFromFile("bitmap_name", "./data/bitmap.txt");
if (success) {
    std::cout << "位图加载成功" << std::endl;
}
```

### 2. 批量加载目录中的所有文件
```cpp
// 从目录加载所有.txt文件
bool success = bitmap_bank.loadBitmapsFromDirectory("./data/");
if (success) {
    std::cout << "批量加载成功" << std::endl;
}
```

### 3. 获取支持的文件格式信息
```cpp
std::string formats = bitmap_bank.getSupportedFormats();
std::cout << formats << std::endl;
```

### 4. 访问加载的位图数据
```cpp
// 获取位图对象
const SparseBitmap* bitmap = bitmap_bank.getBitmap("bitmap_name");
if (bitmap) {
    std::cout << "尺寸: " << bitmap->getRows() << "x" << bitmap->getCols() << std::endl;
    std::cout << "稀疏度: " << (bitmap->getSparsity() * 100) << "%" << std::endl;
}

// 获取指定行数据
std::vector<bool> row_data = bitmap_bank.getRowBitmap("bitmap_name", 0);

// 计算指定行非零元素数量
size_t non_zeros = bitmap_bank.countNonZeros("bitmap_name", 0);

// 获取所有已加载的位图名称
std::vector<std::string> names = bitmap_bank.getBitmapNames();
```

## 错误处理

### 文件格式验证
- 自动验证文件格式是否正确
- 检查所有行的列数是否一致
- 验证每个元素是否为有效的布尔值

### 错误信息
- 文件不存在或无法打开
- 文件格式不正确
- 数据解析失败
- 列数不一致

## 测试程序

### 运行测试
```bash
# 编译测试程序
g++ -o test_bitmap_file_loading test_bitmap_file_loading.cpp

# 运行测试
./test_bitmap_file_loading
```

### 测试内容
1. 单个文件加载测试
2. 不同格式文件测试
3. 目录批量加载测试
4. 位图数据访问测试
5. 错误处理测试

## 集成到主程序

在主程序 `main.cpp` 中已经集成了文本文件加载功能：

```cpp
// 测试从文本文件加载位图数据
bool success1 = bitmap_bank_.loadBitmapFromFile("example_bitmap", "./data/example_bitmap.txt");
bool success2 = bitmap_bank_.loadBitmapFromFile("sparse_pattern", "./data/sparse_pattern.txt");

// 显示已加载的位图信息
auto bitmap_names = bitmap_bank_.getBitmapNames();
for (const auto& name : bitmap_names) {
    auto bitmap = bitmap_bank_.getBitmap(name);
    if (bitmap) {
        std::cout << name << ": " << bitmap->getRows() << "x" << bitmap->getCols() 
                  << ", 稀疏度 " << (bitmap->getSparsity() * 100) << "%" << std::endl;
    }
}
```

## 注意事项

1. **文件路径**: 确保文件路径正确，相对路径相对于程序运行目录
2. **内存管理**: 加载的位图数据会存储在内存中，注意内存使用
3. **文件格式**: 严格按照支持的格式编写文本文件
4. **错误处理**: 检查返回值以确认加载是否成功
5. **性能**: 大文件加载可能需要较长时间，考虑异步加载

## 扩展功能

未来可以考虑添加的功能：
- 支持更多文件格式（CSV、JSON等）
- 异步文件加载
- 文件压缩支持
- 增量加载
- 文件格式转换工具
