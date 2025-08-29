#ifndef __CPU_H
#define __CPU_H

#include <fstream>
#include <functional>
#include <random>
#include <string>
#include "memory_system.h"

namespace dramsim3 {

class CPU {
   public:
    CPU(const std::string& config_file, const std::string& output_dir, const std::string& trace_out_file)
        : memory_system_(
              config_file, output_dir,
              std::bind(&CPU::ReadCallBack, this, std::placeholders::_1),
              std::bind(&CPU::WriteCallBack, this, std::placeholders::_1)),
          clk_(0) {trace_out_file_.open(trace_out_file);}
    ~CPU() {trace_out_file_.close();}
    virtual void ClockTick() = 0;
    void ReadCallBack(uint64_t addr) { 
        if(trace_out_file_.is_open()){
             trace_out_file_ <<"read_callback"<<" addr:"<<addr<<" clk:"<<clk_<<std::endl;
        } else {
            std::cerr << "File is not open for writing." << std::endl;
        }
 
        return; }
    void WriteCallBack(uint64_t addr) { 
        if(trace_out_file_.is_open()){
             trace_out_file_ <<"write_callback"<<" addr:"<<addr<<" clk:"<<clk_<<std::endl;
        } else {
            std::cerr << "File is not open for writing." << std::endl;
        } 
    }
    void PrintStats() { memory_system_.PrintStats(); }

   protected:
    MemorySystem memory_system_;
    uint64_t clk_;
    std::ofstream trace_out_file_; 
};

class RandomCPU : public CPU {
   public:
    using CPU::CPU;
    void ClockTick() override;

   private:
    uint64_t last_addr_;
    bool last_write_ = false;
    std::mt19937_64 gen;
    bool get_next_ = true;
};

class StreamCPU : public CPU {
   public:
    using CPU::CPU;
    void ClockTick() override;

   private:
    uint64_t addr_a_, addr_b_, addr_c_, addr_d_, addr_e_, addr_f_,offset_ = 0;
    std::mt19937_64 gen;
    bool inserted_a_ = false;
    bool inserted_b_ = false;
    bool inserted_c_ = false;
    bool inserted_d_ = false;
    bool inserted_e_ = false;
    bool inserted_f_ = false;
    const uint64_t array_size_ = 2 << 20;  // elements in array
    const int stride_ = 64;                // stride in bytes
};

class TraceBasedCPU : public CPU {
   public:
    TraceBasedCPU(const std::string& config_file, const std::string& output_dir,
                  const std::string& trace_file);
    ~TraceBasedCPU() { trace_file_.close(); }
    void ClockTick() override;

   private:
    std::ifstream trace_file_;
    Transaction trans_;
    bool get_next_ = true;
};

}  // namespace dramsim3
#endif
