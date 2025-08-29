// #include <iostream>
// #include <queue>
// #include <vector>
// #include <functional>
// #include <string>
// #include <memory>

// // 定义时间的基本单位 Tick
// using Tick = uint64_t;

// // 为简化起见，使用全局时钟
// Tick g_current_tick = 0;

// // 前向声明，解决交叉引用的问题
// class Event;
// class EventQueue;
// class SimObject;
// class PingSim;
// class PongSim;

// //===================================================================
// // 1. Event: 所有被调度动作的基类
// //===================================================================
// class Event {
// public:
//     virtual ~Event() {}
//     virtual void process() = 0; // 纯虚函数，由子类实现具体动作
//     Tick get_tick() const { return _tick; }
//     void set_tick(Tick t) { _tick = t; }

// private:
//     Tick _tick = 0;
// };

// // 比较器，用于优先队列，使其成为一个基于Tick的最小堆
// struct EventComparator {
//     bool operator()(const Event* a, const Event* b) const {
//         return a->get_tick() > b->get_tick();
//     }
// };

// //===================================================================
// // 2. EventQueue: 核心事件调度器
// //===================================================================
// class EventQueue {
// public:
//     // 析构函数，用于清理队列中所有剩余的事件
//     ~EventQueue() {
//         while(!_queue.empty()) {
//             delete _queue.top();
//             _queue.pop();
//         }
//     }

//     void schedule(Event* event, Tick scheduled_tick) {
//         event->set_tick(scheduled_tick);
//         _queue.push(event);
//         std::cout << "[EventQueue] 已安排一个事件在 tick " << scheduled_tick << " 触发。" << std::endl;
//     }

//     void service_one_event() {
//         if (empty()) return;

//         Event* event = _queue.top();
//         _queue.pop();

//         g_current_tick = event->get_tick();
//         std::cout << "\n------ 时间推进至 Tick: " << g_current_tick << " ------" << std::endl;

//         event->process();
//         delete event; // 处理完事件后释放内存
//     }

//     bool empty() const {
//         return _queue.empty();
//     }
    

// private:
//     std::priority_queue<Event*, std::vector<Event*>, EventComparator> _queue;
// };

// //===================================================================
// // 3. SimObject: 所有被模拟组件的基类
// //===================================================================
// class SimObject {
// public:
//     SimObject(std::string name) : _name(name) {}
//     virtual ~SimObject() {}
//     const std::string& name() const { return _name; }
// protected:
//     std::string _name;
// };

// //===================================================================
// // 4. 具体的事件和模拟对象实现
// //===================================================================

// // 发往 PingSim 对象的事件
// class PingEvent : public Event {
// public:
//     PingEvent(PingSim& target) : _target(target) {}
//     void process() override;
// private:
//     PingSim& _target;
// };

// // 发往 PongSim 对象的事件
// class PongEvent : public Event {
// public:
//     PongEvent(PongSim& target) : _target(target) {}
//     void process() override;
// private:
//     PongSim& _target;
// };

// // PingSim 模块
// class PingSim : public SimObject {
// public:
//     PingSim(const std::string& name, EventQueue& eq) : SimObject(name), _event_queue(eq) {}
    
//     void set_peer(PongSim* peer) { _pong_peer = peer; }
    
//     // 启动函数，发起第一个事件
//     void startup() {
//         std::cout << "[Ping]       启动！正在为我的伙伴安排一个在 tick 100 发生的 PongEvent。" << std::endl;
//         _event_queue.schedule(new PongEvent(*_pong_peer), g_current_tick + 100);
//     }

//     // 当被Pong模块“回敬”时，这个函数被调用
//     void handle_ping_event() {
//         std::cout << "[Ping]       收到了一个 PingEvent！模拟现在将结束。" << std::endl;
//     }

// private:
//     EventQueue& _event_queue;
//     PongSim* _pong_peer = nullptr;
// };

// // PongSim 模块
// class PongSim : public SimObject {
// public:
//     PongSim(const std::string& name, EventQueue& eq) : SimObject(name), _event_queue(eq) {}

//     void set_peer(PingSim* peer) { _ping_peer = peer; }

//     // 当被Ping模块的事件触发时，这个函数被调用
//     void handle_pong_event() {
//         std::cout << "[Pong]       收到了一个 PongEvent！正在为我的伙伴安排一个在 tick " << g_current_tick + 50 << " 发生的 PingEvent。" << std::endl;
//         _event_queue.schedule(new PingEvent(*_ping_peer), g_current_tick + 50);
//     }
    
// private:
//     EventQueue& _event_queue;
//     PingSim* _ping_peer = nullptr;
// };

// // 在所有类都完全定义后，再实现 process 函数
// void PingEvent::process() {
//     std::cout << "[PingEvent]  正在处理... 即将调用目标的 handle_ping_event()。" << std::endl;
//     _target.handle_ping_event();
// }

// void PongEvent::process() {
//     std::cout << "[PongEvent]  正在处理... 即将调用目标的 handle_pong_event()。" << std::endl;
//     _target.handle_pong_event();
// }

// //===================================================================
// // 5. 模拟主程序
// //===================================================================
// int main1() {
//     std::cout << "===== 迷你gem5事件驱动模拟器 启动 =====\n" << std::endl;

//     // 1. 创建事件队列
//     EventQueue event_queue;

//     // 2. 创建被模拟的硬件模块
//     PingSim ping("PingModule", event_queue);
//     PongSim pong("PongModule", event_queue);
    
//     // 3. 将模块互相连接起来
//     ping.set_peer(&pong);
//     pong.set_peer(&ping);
    
//     // 4. “开机”，通过调度第一个事件来启动模拟
//     ping.startup();

//     // 5. 运行主事件循环，直到队列为空
//     while (!event_queue.empty()) {
//         event_queue.service_one_event();
//     }

//     std::cout << "\n===== 模拟结束于 Tick: " << g_current_tick << " =====\n" << std::endl;
    
//     return 0;
// }
