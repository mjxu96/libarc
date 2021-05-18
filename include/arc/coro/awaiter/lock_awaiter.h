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


namespace detail {

class LockCore {
 public:
  LockCore() = default;
  ~LockCore() = default;

  void Unlock() {
    std::lock_guard guard(lock_);
    is_locked_ = false;
    if (pending_event_pairs_.empty()) {
      return;
    }
    auto& next_event_pair = pending_event_pairs_.front();
    std::uint64_t i = 1;
    next_event_pair.second->TriggerUserEvent(next_event_pair.first);
    if (write(next_event_pair.second->GetEventHandle(), &i, sizeof(i)) != sizeof(i)) {
      throw arc::exception::IOException("NotifyOne Error");
    }
    pending_event_pairs_.pop();
    is_locked_ = true;
  }

  void AddPendingEventPair(events::LockEvent* event, EventLoop* event_loop) {
    event_loop->AddUserEvent(event);
    pending_event_pairs_.emplace(event, event_loop);
  }

  inline void CoreLock() {
    lock_.lock();
  }

  inline void CoreUnlock() {
    lock_.unlock();
  }

  bool TryLock() {
    if (!is_locked_) {
      is_locked_ = true;
      return true;
    }
    return false;
  }

 private:
  std::mutex lock_;
  std::queue<std::pair<events::LockEvent*, EventLoop*>> pending_event_pairs_{};
  bool is_locked_{false};
};

}  // namespace detail


class [[nodiscard]] LockAwaiter {
 public:
  LockAwaiter(detail::LockCore * core)
      : core_(core), event_handle_(GetLocalEventLoop().GetEventHandle()) {}
  bool await_ready() {
    core_->CoreLock();
    if (core_->TryLock()) {
      core_->CoreUnlock();
      return true;
    }
    return false;
  }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    core_->AddPendingEventPair(new events::LockEvent(handle), &GetLocalEventLoop());
    core_->CoreUnlock();
  }

  void await_resume() {}

 private:
  detail::LockCore* core_{nullptr};
  arc::events::EventHandleType event_handle_{-1};
};

}  // namespace coro
}  // namespace arc

#endif
