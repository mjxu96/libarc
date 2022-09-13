/*
 * File: condition.h
 * Project: libarc
 * File Created: Friday, 14th May 2021 9:03:58 pm
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

#ifndef LIBARC__CORO__LOCKS__CONDITION_H
#define LIBARC__CORO__LOCKS__CONDITION_H

#include <arc/coro/awaiter/condition_awaiter.h>
#include <arc/coro/task.h>
#include <arc/exception/io.h>

#include "lock.h"

namespace arc {
namespace coro {

class Condition {
 public:
  Condition() { core_ = new arc::coro::detail::ConditionCore(); }

  void NotifyOne() { core_->TriggerOne(); }

  void NotifyAll() { core_->TriggerAll(); }

  Task<void> Wait(Lock& lock) {
    co_await ConditionAwaiter(core_, lock.core_);
    co_await lock.Acquire();
  }

  Task<void> Wait(Lock& lock, const CancellationToken& token) {
    co_await ConditionAwaiter(core_, lock.core_, token);
    co_await lock.Acquire();
  }

  Task<void> WaitFor(Lock& lock,
                     const std::chrono::steady_clock::duration& timeout) {
    auto awaiter = ConditionAwaiter(core_, lock.core_, timeout);
    co_await lock.Acquire();
  }

  ConditionAwaiter Wait() { return ConditionAwaiter(core_, nullptr); }

  Task<void> Wait(const CancellationToken& token) {
    co_await ConditionAwaiter(core_, nullptr, token);
  }

  Task<void> WaitFor(const std::chrono::steady_clock::duration& timeout) {
    co_await ConditionAwaiter(core_, nullptr, timeout);
  }

  ~Condition() { delete core_; }

  // Condition cannot be copied nor moved.
  Condition(const Condition&) = delete;
  Condition& operator=(const Condition&) = delete;
  Condition(Condition&&) = delete;
  Condition& operator=(Condition&&) = delete;

 private:
  arc::coro::detail::ConditionCore* core_{nullptr};
};

}  // namespace coro
}  // namespace arc

#endif
