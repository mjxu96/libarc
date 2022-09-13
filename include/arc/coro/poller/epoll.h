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

#ifdef __linux__

#include <arc/coro/events/cancellation_event.h>
#include <arc/coro/events/condition_event.h>
#include <arc/coro/events/io_event.h>
#include <arc/coro/events/time_event.h>
#include <arc/coro/events/timeout_event.h>
#include <arc/io/io_base.h>
#include <sys/epoll.h>

#include <atomic>
#include <deque>
#include <list>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace arc {
namespace coro {

class Poller : public io::detail::IOBase {
 public:
  Poller();
  ~Poller();

  void AddIOEvent(coro::IOEvent* event);
  void AddTimeEvent(coro::TimeEvent* event);
  void AddUserEvent(coro::UserEvent* event);
  void AddBoundEvent(coro::BoundEvent* event);

  void RemoveAllIOEvents(int target_fd);

  int WaitEvents(coro::EventBase** todo_events);

  void TrimIOEvents();
  void TrimTimeEvents();
  void TrimUserEvents();

  inline void SetNextTimeNoWait() { next_wait_timeout_ = 0; }

  inline bool IsPollerDone() {
    std::lock_guard<std::mutex> guard(poller_lock_);
    auto ret =
        (total_io_events_ + time_events_.size() + pending_user_events_.size() +
             triggered_user_events_.size() + pending_bound_events_.size() +
             triggered_bound_events_.size() ==
         0) &&
        (!is_dispatcher_registered_);
    return ret;
  }

  inline int GetEventHandle() const { return user_event_fd_; }
  bool TriggerUserEvent(EventID event_id);
  void TriggerBoundEvent(EventID bound_event_id, coro::BoundEvent* event);

  int Register();
  void DeRegister();

  const static int kMaxEventsSizePerWait = 1024;

 private:
  const static int kMaxFdInArray_ = 1024;

  int next_wait_timeout_ = -1;

  std::atomic<EventID> max_event_id_{0};

  // io events
  int total_io_events_{0};
  std::unordered_set<int> interesting_fds_{};
  // {fd -> {io_type -> [events]}}
  std::vector<std::vector<std::deque<coro::IOEvent*>>> io_events_{
      kMaxFdInArray_,
      std::vector<std::deque<coro::IOEvent*>>{2, std::deque<coro::IOEvent*>{}}};
  int io_prev_events_[kMaxFdInArray_] = {0};

  std::unordered_map<int, std::vector<std::deque<coro::IOEvent*>>>
      extra_io_events_{};
  std::unordered_map<int, int> extra_io_prev_events_{};

  // time events
  std::priority_queue<coro::TimeEvent*, std::vector<coro::TimeEvent*>,
                      coro::TimeEventComparator>
      time_events_;

  // user events
  int user_event_fd_{-1};
  bool is_event_fd_added_{false};
  std::mutex poller_lock_;
  std::list<coro::UserEvent*> pending_user_events_;
  std::list<coro::UserEvent*> triggered_user_events_;
  std::unordered_map<EventID, coro::UserEvent*> user_events_;

  // cancellation events
  std::list<coro::BoundEvent*> pending_bound_events_;
  std::list<coro::BoundEvent*> triggered_bound_events_;
  std::unordered_map<EventID, std::list<coro::BoundEvent*>::iterator>
      event_pending_bound_token_map_;
  EventID self_triggered_event_ids_[kMaxEventsSizePerWait] = {0};

  // coro dispatcher related
  bool is_dispatcher_registered_{false};

  // epoll related
  epoll_event events_[kMaxEventsSizePerWait];

  int GetExistingIOEvent(int fd);
  coro::IOEvent* PopIOEvent(int fd, io::IOType event_type);
  EventBase* PopBoundEvent(coro::BoundEvent* event);
  void RemoveBoundEvent(int count);
  void TriggerBoundEventInternal(int bound_event_id, coro::BoundEvent* event);
};

}  // namespace coro
}  // namespace arc

#endif  // __linux__
#endif
