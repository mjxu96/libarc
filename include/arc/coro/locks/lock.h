/*
 * File: lock.h
 * Project: libarc
 * File Created: Saturday, 15th May 2021 10:36:41 am
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

#ifndef LIBARC__CORO__LOCKS__LOCK_H
#define LIBARC__CORO__LOCKS__LOCK_H

#include <arc/coro/awaiter/lock_awaiter.h>

namespace arc {
namespace coro {

class Lock {
 public:
  Lock() {
    core_ = new arc::events::detail::LockCore();
  }
  ~Lock() {
    delete core_;
  }

  // Lock cannot be copied nor moved.
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;
  Lock(Lock&&) = delete;
  Lock& operator=(Lock&&) = delete;

  LockAwaiter Acquire() {
    return LockAwaiter(core_);
  }

  void Release() {
    core_->Unlock();
  }

  friend class Condition;

 private:
  arc::events::detail::LockCore* core_{nullptr};
};

} // namespace coro
} // namespace arc


#endif