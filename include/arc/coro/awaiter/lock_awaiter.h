/*
 * File: lock_awaiter.h
 * Project: libarc
 * File Created: Saturday, 15th May 2021 11:01:07 am
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
#ifndef LIBARC__CORO__AWAITER__LOCK_AWAITER_H
#define LIBARC__CORO__AWAITER__LOCK_AWAITER_H

#include <arc/coro/eventloop.h>
#include <arc/coro/events/lock_event.h>

namespace arc {
namespace coro {

class [[nodiscard]] LockAwaiter {
 public:
  LockAwaiter(arc::events::detail::LockCore* core)
      : core_(core), event_handle_(GetLocalEventLoop().GetEventHandle()) {}
  bool await_ready() { return core_->CanLock(event_handle_, true); }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    GetLocalEventLoop().AddUserEvent(
        new events::LockEvent(core_, event_handle_, handle));
  }

  void await_resume() {}

 private:
  arc::events::detail::LockCore* core_{nullptr};
  arc::events::EventHandleType event_handle_{-1};
};

}  // namespace coro
}  // namespace arc

#endif
