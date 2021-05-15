/*
 * File: lock_event.h
 * Project: libarc
 * File Created: Saturday, 15th May 2021 10:38:14 am
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
#ifndef LIBARC__CORO__EVENTS__LOCK_EVENT_H
#define LIBARC__CORO__EVENTS__LOCK_EVENT_H

#include <arc/exception/io.h>

#include <mutex>
#include <queue>

#include "user_event.h"

namespace arc {
namespace events {

namespace detail {

class LockCore {
 public:
  LockCore() = default;
  ~LockCore() = default;

  void Unlock() {
    std::lock_guard guard(lock_);
    is_locked_ = false;
    triggered_handle_ = -1;
    is_refilled_ = false;
    if (pending_event_handles_.empty()) {
      return;
    }
    triggered_handle_ = pending_event_handles_.front();
    pending_event_handles_.pop();
    std::uint64_t i = 1;
    if (write(triggered_handle_, &i, sizeof(i)) != sizeof(i)) {
      throw arc::exception::IOException("NotifyOne Error");
    }
  }

  bool CanLock(EventHandleType event_handle, bool is_instantly) {
    std::lock_guard guard(lock_);
    if (is_locked_) {
      assert(triggered_handle_ != -1);
      if (is_instantly) {
        pending_event_handles_.push(event_handle);
      }
      return false;
    } else {
      if (is_instantly) {
        if (triggered_handle_ == -1) {
          triggered_handle_ = event_handle;
          is_locked_ = true;
          return true;
        }
        pending_event_handles_.push(event_handle);
        return false;
      }
      if (triggered_handle_ != event_handle) {
        return false;
      }
      triggered_handle_ = event_handle;
      is_locked_ = true;
      return true;
    }
  }

 private:
  std::mutex lock_;
  std::queue<EventHandleType> pending_event_handles_{};
  EventHandleType triggered_handle_{-1};
  bool is_locked_{false};
  bool is_refilled_{false};
};

}  // namespace detail

class LockEvent : public UserEvent {
 public:
  LockEvent(detail::LockCore* core, EventHandleType event_handle,
            std::coroutine_handle<void> handle)
      : UserEvent(event_handle, handle), core_(core) {}

  virtual bool CanResume() override {
    return core_->CanLock(event_handle_, false);
  }

 private:
  detail::LockCore* core_{nullptr};
};
}  // namespace events
}  // namespace arc

#endif
