/*
 * File: epoll.h
 * Project: libarc
 * File Created: Sunday, 9th May 2021 1:37:11 pm
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

#ifndef LIBARC__CORO__POLLER__EPOLL_H
#define LIBARC__CORO__POLLER__EPOLL_H

#include <arc/io/io_base.h>
#include <sys/epoll.h>
#include <arc/coro/events/io_event_base.h>
#include <vector>
#include <unordered_map>
#include <deque>

namespace arc {
namespace coro {

class Poller : public io::detail::IOBase {
 public:
  Poller();
  ~Poller() = default;

  void AddIOEvent(events::detail::IOEventBase* event, bool replace = false);
  void RemoveIOEvent(events::detail::IOEventBase* event);

  void CleanUp();

 private:
  const static int kMaxEventsSizePerWait_ = 1024;
  const static int kMaxFdInArray_ = 1024;
  const static int kEpollWaitTimeout_ = 1;

  int total_added_task_num_{0};

  // {fd -> {io_type -> [events]}}
  std::vector<std::vector<std::deque<events::detail::IOEventBase*>>> io_events_{
      kMaxFdInArray_, std::vector<std::deque<events::detail::IOEventBase*>>{
                          2, std::deque<events::detail::IOEventBase*>{}}};
  std::unordered_map<int, std::vector<std::deque<events::detail::IOEventBase*>>>
      extra_io_events_{};

  // epoll related
  epoll_event events_[kMaxEventsSizePerWait_];
  events::detail::IOEventBase* todo_events_[2 * kMaxEventsSizePerWait_] = {
      nullptr};

  int GetExistIOEvent(int fd);
  events::detail::IOEventBase* GetEvent(int fd,
                                        events::detail::IOEventType event_type);
};

} // namespace coro
} // namespace arc



#endif