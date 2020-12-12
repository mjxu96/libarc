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

#include <assert.h>
#include <sys/epoll.h>
#include <coroutine>
#include <iostream>
#include <list>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <arc/coro/events/coro_task_event.h>
#include <arc/coro/events/io_event_base.h>
#include <arc/io/io_base.h>

namespace arc {
namespace coro {

class EventLoop : public io::detail::IOBase {
 public:
  EventLoop();
  ~EventLoop() = default;

  bool IsDone();
  void Do();
  void AddIOEvent(events::detail::IOEventBase*);

  void AddCoroutine(events::CoroTaskEvent*);
  void FinishCoroutine(std::uint64_t coro_id);

 private:
  int total_added_task_num_{0};

  std::unordered_map<int, std::queue<events::detail::IOEventBase*>>
      read_events_;
  std::unordered_map<int, std::queue<events::detail::IOEventBase*>>
      write_events_;

  std::unordered_map<std::uint64_t, events::CoroTaskEvent*> coro_events_;
  std::list<events::CoroTaskEvent*> finished_coro_events_;

  const int kMaxEventsSizePerWait = 64;
  std::uint64_t current_coro_id_{0};
};

EventLoop& GetLocalEventLoop();

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__CORE__EVENTLOOP_H */
