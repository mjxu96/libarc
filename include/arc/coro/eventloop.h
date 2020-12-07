/*
 * File: eventloop.h
 * Project: libarc
 * File Created: Monday, 7th December 2020 9:17:34 pm
 * Author: Minjun Xu (mjxu96@outlook.com)
 * -----
 * MIT License
 * Copyright (c) 2020 Minjun Xu
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LIBARC__CORO__EVENTLOOP_H
#define LIBARC__CORO__EVENTLOOP_H

#include <queue>
#include <unordered_map>
#include <coroutine>
#include <sys/epoll.h>

#include <arc/coro/events/event_base.h>
#include <arc/io/io_base.h>

namespace arc {
namespace coro {

class EventLoop : public io::IOBase {
 public:
  EventLoop();

  bool IsDone();
  void Do();
  void AddEvent(events::EventBase*);

 private:
  int total_added_task_num_{0};

  void Open() override;
  void Close() override;

  std::unordered_map<int, std::queue<events::EventBase*>> events_;
};

EventLoop& GetLocalEventLoop();
  
} // namespace coro
} // namespace arc


#endif /* LIBARC__CORO__CORE__EVENTLOOP_H */
