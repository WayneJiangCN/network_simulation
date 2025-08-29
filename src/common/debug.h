#ifndef _DEBUG_H
#define _DEBUG_H

#include <iostream>
#include <string>
#include <set>
#include <cstdarg>
#include <cstdio>
#include "event/eventq.h"
// 日志级别
namespace GNN
{

// 日志级别
enum MiniDebugLevel {
    DBG_ERROR = 0,
    DBG_WARN  = 1,
    DBG_INFO  = 2,
    DBG_DEBUG = 3
};

extern MiniDebugLevel miniDebugLevel;
extern std::set<std::string> miniDebugModules;

// 全局时钟函数（你可以用 curTick() 或 gSim->getCurTick()）
extern uint64_t curTick();

inline bool miniDebugModuleEnabled(const std::string& mod) {
    return miniDebugModules.empty() || miniDebugModules.count(mod);
}

// 格式化辅助函数
inline std::string miniDebugFormat(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}

// 主宏（可变参数）
#define MINI_DEBUG(MODULE, LEVEL, FMT, ...) \
    do { \
        if ((LEVEL) <= miniDebugLevel && miniDebugModuleEnabled(MODULE)) { \
            std::cout << "[cycle " << gSim->getCurTick() << "] " \
                      << #LEVEL << " " << MODULE << " " \
                      << __FILE__ << ":" << __LINE__ << " " \
                      << miniDebugFormat(FMT, ##__VA_ARGS__) << std::endl; \
        } \
    } while(0)

#define D_ERROR(MODULE, FMT, ...) MINI_DEBUG(MODULE, DBG_ERROR, FMT, ##__VA_ARGS__)
#define D_WARN(MODULE, FMT, ...)  MINI_DEBUG(MODULE, DBG_WARN,  FMT, ##__VA_ARGS__)
#define D_INFO(MODULE, FMT, ...)  MINI_DEBUG(MODULE, DBG_INFO,  FMT, ##__VA_ARGS__)
#define D_DEBUG(MODULE, FMT, ...) MINI_DEBUG(MODULE, DBG_DEBUG, FMT, ##__VA_ARGS__)


}
#endif // MINI_DEBUG_HH