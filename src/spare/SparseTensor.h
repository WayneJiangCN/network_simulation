/*
 * @Author: AI Assistant
 * @Date: 2025-01-27
 * @Description: 稀疏张量数据结构定义
 */

#ifndef GNN_SPARSE_TENSOR_H_
#define GNN_SPARSE_TENSOR_H_

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <iostream>
namespace GNN {

// 前向声明
typedef uint32_t addr_t;

// 稀疏位图类
class SparseBitmap {
private:
    std::vector<std::vector<bool>> data_;
    size_t rows_;
    size_t cols_;
    double sparsity_;

public:
    SparseBitmap(size_t rows, size_t cols) : rows_(rows), cols_(cols) {
        data_.resize(rows, std::vector<bool>(cols, false));
        sparsity_ = 0.0;
    }
    
    SparseBitmap(const SparseBitmap& other) : rows_(other.rows_), cols_(other.cols_), sparsity_(other.sparsity_) {
        data_ = other.data_;
    }
    
    // 设置位图值
    void setBit(size_t row, size_t col, bool value) {
        if (row < rows_ && col < cols_) {
            data_[row][col] = value;
            updateSparsity();
        }
    }
    
    // 获取位图值
    bool getBit(size_t row, size_t col) const {
        if (row < rows_ && col < cols_) {
            return data_[row][col];
        }
        return false;
    }
    
    // 获取指定行
    std::vector<bool> getRow(size_t row) const {
        if (row < rows_) {
            return data_[row];
        }
        return {};
    }
    
    // 获取指定区域
    SparseBitmap getRegion(size_t row_start, size_t col_start, 
                          size_t row_len, size_t col_len) const {
        SparseBitmap result(row_len, col_len);
        for (size_t r = 0; r < row_len && (row_start + r) < rows_; ++r) {
            for (size_t c = 0; c < col_len && (col_start + c) < cols_; ++c) {
                result.setBit(r, c, data_[row_start + r][col_start + c]);
            }
        }
        return result;
    }
    
    // 获取稀疏度
    double getSparsity() const { return sparsity_; }
    
    // 获取尺寸
    size_t getHeight() const { return rows_; }
    size_t getWidth() const { return cols_; }
    size_t getRows() const { return rows_; }
    size_t getCols() const { return cols_; }
    
    // 计算非零元素数量
    size_t countNonZeros() const {
        size_t count = 0;
        for (const auto& row : data_) {
            count += std::count(row.begin(), row.end(), true);
        }
        return count;
    }
    
    // 随机生成稀疏位图
    void generateRandom(double sparsity) {
        srand(time(nullptr));
        for (size_t r = 0; r < rows_; ++r) {
            for (size_t c = 0; c < cols_; ++c) {
                data_[r][c] = (rand() % 100) < (sparsity * 100);
            }
        }
        updateSparsity();
    }
    
    // 打印位图（用于调试）
    void print() const {
        std::cout << "SparseBitmap (" << rows_ << "x" << cols_ << "), sparsity: " 
                  << sparsity_ * 100 << "%" << std::endl;
        for (size_t r = 0; r < std::min(rows_, size_t(10)); ++r) { // 只打印前10行
            for (size_t c = 0; c < std::min(cols_, size_t(20)); ++c) { // 只打印前20列
                std::cout << (data_[r][c] ? "1" : "0") << " ";
            }
            if (cols_ > 20) std::cout << "...";
            std::cout << std::endl;
        }
        if (rows_ > 10) std::cout << "..." << std::endl;
    }

private:
    void updateSparsity() {
        size_t total = rows_ * cols_;
        size_t non_zeros = countNonZeros();
        sparsity_ = total > 0 ? (double)non_zeros / total : 0.0;
    }
};

// 数据突发结构
template<typename T>
struct DataBurst {
    std::vector<T> data;
    addr_t base_addr;
    size_t length;
    
    DataBurst() : base_addr(0), length(0) {}
    DataBurst(const std::vector<T>& d, addr_t addr, size_t len) 
        : data(d), base_addr(addr), length(len) {}
};

// 稀疏张量模板类
template<typename T>
class SparseTensor {
private:
    std::vector<std::vector<T>> data_;
    size_t rows_;
    size_t cols_;
    std::string name_;

public:
    SparseTensor(const std::string& name, size_t rows, size_t cols) 
        : name_(name), rows_(rows), cols_(cols) {
        data_.resize(rows, std::vector<T>(cols, T(0)));
    }
    
    // 设置值
    void setValue(size_t row, size_t col, T value) {
        if (row < rows_ && col < cols_) {
            data_[row][col] = value;
        }
    }
    
    // 获取值
    T getValue(size_t row, size_t col) const {
        if (row < rows_ && col < cols_) {
            return data_[row][col];
        }
        return T(0);
    }
    
    // 根据位图获取数据突发
    DataBurst<T> getBurstByBitmap(const std::vector<bool>& bitmap_row) const {
        std::vector<T> burst_data;
        for (size_t c = 0; c < std::min(bitmap_row.size(), cols_); ++c) {
            if (bitmap_row[c]) {
                burst_data.push_back(data_[0][c]); // 简化：假设只有一行
            }
        }
        return DataBurst<T>(burst_data, 0, burst_data.size());
    }
    
    // 获取尺寸
    size_t getRows() const { return rows_; }
    size_t getCols() const { return cols_; }
    std::string getName() const { return name_; }
    
    // 随机生成数据
    void generateRandom() {
        srand(time(nullptr));
        for (size_t r = 0; r < rows_; ++r) {
            for (size_t c = 0; c < cols_; ++c) {
                data_[r][c] = static_cast<T>(rand() % 100);
            }
        }
    }
};

} // namespace GNN

#endif // GNN_SPARSE_TENSOR_H_
