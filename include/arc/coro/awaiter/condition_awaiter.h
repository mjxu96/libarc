/*
 * File: condition_awaiter.h
 * Project: libarc
 * File Created: Friday, 14th May 2021 9:12:02 pm
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

#ifndef LIBARC__CORO__AWAITER__CONDITION_AWAITER_H
#define LIBARC__CORO__AWAITER__CONDITION_AWAITER_H

#include <atomic>
#include <mutex>
#include <unordered_map>

#include <arc/coro/eventloop.h>
#include <arc/coro/events/condition_event.h>

namespace arc {
namespace coro {

class [[nodiscard]] ConditionAwaiter {
 public:
  ConditionAwaiter(arc::events::detail::ConditionCore* core): core_(core) {} 

  bool await_ready() {
    return false;
  }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    GetLocalEventLoop().AddUserEvent(new events::ConditionEvent(core_, handle));
  }

  void await_resume() {
  }
 private:
  arc::events::detail::ConditionCore* core_{nullptr};
};

} // namespace coro
} // namespace arc


#endif
