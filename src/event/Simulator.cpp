// /*
//  * Licensed under the Apache License, Version 2.0 (the 'License');
//  * you may not use this file except in compliance with the License.
//  * See the NOTICE file distributed with this work for additional information
//  * regarding copyright ownership. You may obtain a copy of the License at
//  *
//  *  http://www.apache.org/licenses/LICENSE-2.0
//  *
//  * Unless required by applicable law or agreed to in writing, software
//  * distributed under the License is distributed on an "AS IS" BASIS,
//  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  * See the License for the specific language governing permissions and
//  * limitations under the License.
//  */
// #include "event/Simulator.h"
// #include "common/common.h"

// #include <cassert>

// Simulator::Simulator()
//     : printProgress_(1),
//       printInterval_(1),
//       time_(0), epsilon_(0), quit_(false),
//       channelCycleTime_(1),
//       routerCycleTime_(1),
//       interfaceCycleTime_(1),
//       initial_(true), initialized_(false), running_(false)
// {
//     assert(channelCycleTime_ > 0);
//     assert(routerCycleTime_ > 0);
//     assert(interfaceCycleTime_ > 0);
//     assert(printInterval_ > 0);
// }

// Simulator::~Simulator() {}

// void Simulator::initialize()
// {
//     assert(!initialized_);
//     for (auto &comp : Event::events_)
//         comp.second->initialize();
//     initialized_ = true;
// }

// void Simulator::simulate()
// {
//     assert(initialized_);
//     assert(!running_);
//     initial_ = false;
//     running_ = true;
//         gSim->serviceOne();
//     running_ = false;
//     quit_ = false;
// }

