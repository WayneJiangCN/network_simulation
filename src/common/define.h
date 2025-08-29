#ifndef DEFINE_H
#define DEFINE_H

#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>

//#define DEBUG
#define ANALYSIS
#define DRAM_MODE
#define FIX_MODE_DLY 32

// #define DRAM_TRACE
// types
typedef uint64_t cycles_t;
typedef uint32_t address_t;
typedef uint32_t reg_t;

// buf types
typedef int int_buf_t[2];
typedef float float_buf_t[2];
typedef bool bool_buf_t[2];

typedef std::vector<int> vec_int_buf_t[2];
typedef std::vector<float> vec_float_buf_t[2];

typedef int *int_array_buf_t[2];

typedef uint64_t cycles_buf_t[2];
typedef address_t address_buf_t[2];

#define CHANNEL_NUM 8


// #define ADD_ROW_BASE 0x0
// #define ADD_COL_BASE 0x10000
// #define FEATURE_ROW_BASE 0x500000
// #define FEATURE_COL_BASE 0x600000
// #define ADD_FEATURE_BASE 0x50000

#define BANK_SIZE 128 // SRAM Depth

// cache
#define NUM_CORES 9 // 8 + 1 axi

#define CACHE_LINE_SIZE 16 // word size
#define CACHE_LINE_GRAN 4  //! TODO: set to 4

#define CACHE_WAY_NUM 32
#define CACHE_LINE_NUM 1024 
#define CACHE_OFFSET 4
#define MSHR_SIZE 32//64 
#define MSHR_ENTRY_SIZE 8//64//32
#define UP_BUF_SIZE 16//32 
#define MAC_NUM 32
#define TOTAL_ROUND 352 //cora:46 86 2  pubmed:80 3090 10 Flickr:352 61380 44
#define TOTAL_ROW_ROUND 44//cora:2 2 pubmed:10 10 Flickr: 44  44
#define TOTAL_SLICE_NUM 8  //cora:23 43 1 pubmed:8 309 1 Flickr: 8 1395 1
#define ROW_NUM 1314520//cora: 2708 pubmed:19717 Flickr:89250
#define COL_NUM 131072//定值
// #define CACHE_WAY_NUM 4
// #define CACHE_LINE_NUM 32
// #define CACHE_OFFSET 4
// #define MSHR_SIZE 64
// #define MSHR_ENTRY_SIZE 32

enum NET_TYPE {
    GCN,
    GIN,
    GRPHASAGE,
    GAT
};

enum request_type {
    READ,
    FILL,
    FILL_IN // filled signal back to cache
};

struct request_pack {
    int core_id;   // which core
    int thread_id; // which thread of the core
    address_t address;
    request_type core_req;

    int sel_way;               // tag
    address_t cache_line_addr; // tag
    int tag;

    int l2_rd[CACHE_LINE_SIZE * CACHE_LINE_GRAN]; // read

    int l2_wr[CACHE_LINE_SIZE * CACHE_LINE_GRAN]; // miss fill
    int fill_degree;
    bool fill_ok;

    int corr[2]; // for test
    int col;     // for test
    // TODO:mask,data
};

struct cache2core_pack {
    int l2rd[CACHE_LINE_SIZE * CACHE_LINE_GRAN];
    int core_id=0;   // which core
    int thread_id; // which thread of the core
    address_t address;
    int corr[2]; // for test
    int col;     // for test
};
struct core_pack {
    // int l2rd[CACHE_LINE_SIZE];
    int core_id;   // which core
    int thread_id; // which thread of the core
    int y;         // y coor
    int col;       // for test
    int vertex_num;
    int adj_value; //! not comp
    int data[CACHE_LINE_SIZE * CACHE_LINE_GRAN];
    address_t address;
};

// acc engine
#define ACC_PSTAGE 3// > 1 , = 1 has bug
#define PE_NUM 16
#define THREAD_NUM 32
struct agg_compute {
    int acc_reg[PE_NUM];
    bool vld2buf;
    int thread_id;
    unsigned int thread_cnt;
};
struct agg2buf {
    int acc_reg[CACHE_LINE_SIZE * CACHE_LINE_GRAN]; //! TODO PE_NUM->CACHE_LINE_NUM
    int thread_id;
};

// comb engine
#define ADJ_R_CHANNEL 0
#define DEGREE_R_CHANNEL 1
#define WEIGHT_R_CHANNEL 2
#define IN_R_CHANNEL 3
#define OUT_R_CHANNEL 4
#define CACHE_ARB_CH 5

#define OUT_W_CHANNEL 6
#define FEATURE_W_CHANEL 7

#define NUM_SYS 8
#define SYS_W_SIZE_MAX 16     // per sys W -> H_f max
#define SYS_BUF_SIZE_MAX 128 // W_w max
struct comb_out {
    int feature_out[CACHE_LINE_SIZE]; // equal to dram gran, the smallest gran for feature length
    address_t addr;
    bool wr_vld;
    bool wr_receive;
    bool rd_vld;
    bool data_vld;
    unsigned int thread_id; // for test
};
struct comb_CA_buf {
    // feature_data
    address_t addr;
    unsigned int core_id;
    unsigned int col; // this is the coor[1]
    bool rd_vld;
    bool data_vld;
    unsigned int data_cnt;
    bool is_reading = false;
};
struct comb2agg {
    // CA
    address_t addr;
    unsigned int core_id;
    unsigned int col;                                   // this is the coor[1]
    int feature_out[CACHE_LINE_SIZE * CACHE_LINE_GRAN]; // equal to dram gran, the smallest gran for feature length
    bool is_send2cache = false;
};

#endif
