/*
 * File: time_event.h
 * Project: libarc
 * File Created: Saturday, 17th April 2021 8:15:06 pm
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

#ifndef LIBARC__CORO__EVENTS__TIME_EVENT_H
#define LIBARC__CORO__EVENTS__TIME_EVENT_H

#include <arc/coro/eventloop.h>
#include <arc/exception/io.h>
#include <arc/io/io_base.h>
#include <fcntl.h>
#include <sys/timerfd.h>

#include <chrono>
#include <cstring>
#include <optional>
#include <queue>
#include <unordered_map>

#include "io_event_base.h"

namespace arc {
namespace events {
namespace detail {

class TimerEvent : public IOEventBase {
 public:
  TimerEvent(int fd, std::coroutine_handle<void> handle)
      : IOEventBase(fd, io::IOType::READ, handle) {}
  virtual ~TimerEvent() {}
};

class AsyncTimerController : public io::detail::IOBase {
 public:
  AsyncTimerController() : io::detail::IOBase() {
    fd_ = timerfd_create(CLOCK_MONOTONIC, 0);
    if (fd_ < 0) {
      throw arc::exception::IOException("Creating Local Timer Error");
    }
    int flags = fcntl(fd_, F_GETFL);
    if (flags < 0) {
      throw arc::exception::IOException("Creating Local Timer Error");
    }
    flags = fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    if (flags < 0) {
      throw arc::exception::IOException("Creating Local Timer Error");
    }
  }
  virtual ~AsyncTimerController() {}
  AsyncTimerController(const AsyncTimerController&) = delete;
  AsyncTimerController(AsyncTimerController&&) = delete;
  AsyncTimerController& operator=(const AsyncTimerController&) = delete;
  AsyncTimerController& operator=(AsyncTimerController&&) = delete;

  void AddWakeupTimePoint(int64_t next_wakeup_time,
                          std::coroutine_handle<void> handle) {
    q_.push(next_wakeup_time);
    assert(handles_.find(next_wakeup_time) == handles_.end());
    handles_[next_wakeup_time] = handle;
    if (q_.size() == 1) {
      FireNextAvailableTimePoint();
    }
  }

  void FireNextAvailableTimePoint() {
    if (q_.empty()) {
      return;
    }
    int64_t next_wakeup_time = q_.top();
    std::coroutine_handle<void> handle = handles_[next_wakeup_time];
    itimerspec next_wakeup_ti;
    std::memset(&next_wakeup_ti, 0, sizeof(itimerspec));
    next_wakeup_ti.it_value.tv_sec = next_wakeup_time / 1000000000;
    next_wakeup_ti.it_value.tv_nsec = next_wakeup_time % 1000000000;
    int ret = timerfd_settime(fd_, TFD_TIMER_ABSTIME, &next_wakeup_ti, nullptr);
    if (ret < 0) {
      throw arc::exception::IOException("Set Timer Error");
    }
    coro::GetLocalEventLoop().AddIOEvent(new TimerEvent(fd_, handle));
  }

  void PopFirstTimePoint() {
    char buf[8] = {0};
    if (read(fd_, buf, 8) < 0) {
      throw arc::exception::IOException("Pop Timer Error");
    }
    int64_t wakeup_time = q_.top();
    handles_.erase(wakeup_time);
    q_.pop();
  }

 private:
  std::priority_queue<int64_t, std::vector<int64_t>, std::greater<int64_t>> q_;
  std::unordered_map<int64_t, std::coroutine_handle<void>> handles_;
};

AsyncTimerController& GetLocalAsyncTimerController();

}  // namespace detail
}  // namespace events
}  // namespace arc
#endif
