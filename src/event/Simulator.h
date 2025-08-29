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
// #ifndef EVENT_SIMULATOR_H_
// #define EVENT_SIMULATOR_H_
// #include "event/Event.h"
// #include <iostream>
// class Event;
// class Simulator
// {
// public:
//   explicit Simulator();
//   virtual ~Simulator();

//   // this adds an event to the queue
//   virtual void addEvent(uint64_t _time, uint8_t _epsilon, Event *_component,
//                         void *_message, int32_t _type) = 0;

//   // this function must return the current size of the queue
//   virtual uint64_t queueSize() const = 0;
//   // virtual bool cancelEvent(Event *comp, int32_t type);
//   void initialize();
//   void simulate();
//   void stop();
//   bool initial() const;
//   bool running() const;

//   uint64_t time() const;
//   uint8_t epsilon() const;

//   enum class Clock : uint8_t
//   {
//     CHANNEL = 0,
//     ROUTER = 1,
//     INTERFACE = 2
//   };

// protected:
//   // this function must set time_, epsilon_, and quit_ on every call
//   virtual void runNextEvent() = 0;

//   const bool printProgress_;
//   const double printInterval_;

//   uint64_t time_;
//   uint8_t epsilon_;
//   bool quit_;

// private:
//   const uint64_t channelCycleTime_;
//   const uint64_t routerCycleTime_;
//   const uint64_t interfaceCycleTime_;

//   bool initial_;
//   bool initialized_;
//   bool running_;
// };



// #endif // EVENT_SIMULATOR_H_
