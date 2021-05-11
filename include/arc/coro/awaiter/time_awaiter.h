/*
 * File: time_awaiter.h
 * Project: libarc
 * File Created: Saturday, 17th April 2021 8:08:43 pm
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

#ifndef LIBARC__CORO__AWAITER__TIME_AWAITER_H
#define LIBARC__CORO__AWAITER__TIME_AWAITER_H

#include <arc/concept/coro.h>
#include <arc/coro/eventloop.h>
#include <arc/coro/events/time_event.h>

#include <chrono>

namespace arc {
namespace coro {

class [[nodiscard]] TimeAwaiter {
 public:
  TimeAwaiter(const std::chrono::steady_clock::duration& sleep_time)
      : next_wakeup_time_(std::chrono::duration_cast<std::chrono::milliseconds>(
                              (std::chrono::steady_clock::now() + sleep_time)
                                  .time_since_epoch())
                              .count()) {}

  bool await_ready() { return false; }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    GetLocalEventLoop().AddTimeEvent(new events::TimeEvent(next_wakeup_time_, handle));
  }

  void await_resume() {
  }

 private:
  std::int64_t next_wakeup_time_;
};

}  // namespace coro
}  // namespace arc

#endif