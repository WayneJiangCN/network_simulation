#include "cpu.h"

namespace dramsim3 {

void RandomCPU::ClockTick() {
    // Create random CPU requests at full speed
    // this is useful to exploit the parallelism of a DRAM protocol
    // and is also immune to address mapping and scheduling policies
    memory_system_.ClockTick();
    if (get_next_) {
        last_addr_ = gen();
        last_write_ = (gen() % 3 == 0);

    }

    get_next_ = memory_system_.WillAcceptTransaction(last_addr_, last_write_);
    if (get_next_) {

        memory_system_.AddTransaction(last_addr_, last_write_);

        if(trace_out_file_.is_open()){
             trace_out_file_ <<"addr:"<<last_addr_<<" is write:"<<last_write_<<" clk:"<<clk_<<std::endl; 
        } else {
            std::cerr << "File is not open for writing." << std::endl;
        }
    }
    clk_++;
    return;
}

void StreamCPU::ClockTick() {
    // stream-add, read 2 arrays, add them up to the third array
    // this is a very simple approximate but should be able to produce
    // enough buffer hits

    // moving on to next set of arrays
    memory_system_.ClockTick();
    if (offset_ >= array_size_ || clk_ == 0) {
        addr_a_ = gen();
        addr_b_ = gen();
        addr_c_ = gen();
        addr_d_ = gen();
        addr_e_ = gen();
        addr_f_ = gen();
        offset_ = 0;
    }

    if (!inserted_a_ &&
        memory_system_.WillAcceptTransaction(addr_a_ + offset_, false)) {
        memory_system_.AddTransaction(addr_a_ + offset_, false);
        inserted_a_ = true;
        if(trace_out_file_.is_open()){
             trace_out_file_ <<"addr:"<<addr_a_ + offset_<<" reada:"<<" clk:"<<clk_<<std::endl; 
        } else {
            std::cerr << "File is not open for writing." << std::endl;
        }
 
    }
    if (!inserted_b_ &&
        memory_system_.WillAcceptTransaction(addr_b_ + offset_, false)) {
        memory_system_.AddTransaction(addr_b_ + offset_, false);
        inserted_b_ = true;
        if(trace_out_file_.is_open()){
             trace_out_file_ <<"addr:"<<addr_b_ + offset_<<" readb:"<<" clk:"<<clk_<<std::endl; 
        } else {
            std::cerr << "File is not open for writing." << std::endl;
        }
    }
    if (!inserted_c_ &&
        memory_system_.WillAcceptTransaction(addr_c_ + offset_, true)) {
        memory_system_.AddTransaction(addr_c_ + offset_, true);
        inserted_c_ = true;
        if(trace_out_file_.is_open()){
             trace_out_file_ <<"addr:"<<addr_c_ + offset_<<" writec:"<<" clk:"<<clk_<<std::endl; 
        } else {
            std::cerr << "File is not open for writing." << std::endl;
        }
    }
    if (!inserted_d_ &&
        memory_system_.WillAcceptTransaction(addr_d_ + offset_, false)) {
        memory_system_.AddTransaction(addr_d_ + offset_, false);
        inserted_d_ = true;
        if(trace_out_file_.is_open()){
             trace_out_file_ <<"addr:"<<addr_d_ + offset_<<" readd:"<<" clk:"<<clk_<<std::endl; 
        } else {
            std::cerr << "File is not open for writing." << std::endl;
        }
    }
     if (!inserted_e_ &&
        memory_system_.WillAcceptTransaction(addr_e_ + offset_, false)) {
        memory_system_.AddTransaction(addr_e_ + offset_, false);
        inserted_e_ = true;
        if(trace_out_file_.is_open()){
             trace_out_file_ <<"addr:"<<addr_e_ + offset_<<" reade:"<<" clk:"<<clk_<<std::endl; 
        } else {
            std::cerr << "File is not open for writing." << std::endl;
        }
    }
     if (!inserted_f_ &&
        memory_system_.WillAcceptTransaction(addr_f_ + offset_, false)) {
        memory_system_.AddTransaction(addr_f_ + offset_, false);
        inserted_f_ = true;
        if(trace_out_file_.is_open()){
             trace_out_file_ <<"addr:"<<addr_f_ + offset_<<" readf:"<<" clk:"<<clk_<<std::endl; 
        } else {
            std::cerr << "File is not open for writing." << std::endl;
        }
    }
 
    // moving on to next element
    if (inserted_a_ && inserted_b_ && inserted_c_ && inserted_d_ && inserted_e_ && inserted_f_) {
        offset_ += stride_;
        inserted_a_ = false;
        inserted_b_ = false;
        inserted_c_ = false;
        inserted_d_ = false;
        inserted_e_ = false;
        inserted_f_ = false;
    }
    clk_++;
    return;
}

TraceBasedCPU::TraceBasedCPU(const std::string& config_file,
                             const std::string& output_dir,
                             const std::string& trace_file)
    : CPU(config_file, output_dir,"./trace_out_file.txt") {
    trace_file_.open(trace_file);
    if (trace_file_.fail()) {
        std::cerr << "Trace file does not exist" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
}

void TraceBasedCPU::ClockTick() {
    memory_system_.ClockTick();
    if (!trace_file_.eof()) {
        if (get_next_) {
            get_next_ = false;
            trace_file_ >> trans_;
        }
        if (trans_.added_cycle <= clk_) {
            get_next_ = memory_system_.WillAcceptTransaction(trans_.addr,
                                                             trans_.is_write);
            if (get_next_) {
                memory_system_.AddTransaction(trans_.addr, trans_.is_write);
            }
        }
    }
    clk_++;
    return;
}

}  // namespace dramsim3
