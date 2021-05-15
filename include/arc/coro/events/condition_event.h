/*
 * File: condition_event.h
 * Project: libarc
 * File Created: Friday, 14th May 2021 9:01:27 pm
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

#ifndef LIBARC__CORO__EVENTS__CONDITION_EVENT_H
#define LIBARC__CORO__EVENTS__CONDITION_EVENT_H

#include <arc/exception/io.h>

#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "lock_event.h"

namespace arc {

namespace events {

namespace detail {

class ConditionCore {
 public:
  ConditionCore() = default;
  ~ConditionCore() = default;

  void Register(EventHandleType event_handle) {
    std::lock_guard guard(lock_);
    if (registered_events_.find(event_handle) == registered_events_.end()) {
      registered_events_[event_handle] = 0;
    }
    registered_events_[event_handle]++;
    registered_count_++;
    in_queue_event_handles_.push_back(event_handle);
  }

  bool CanDeregister(EventHandleType event_handle) {
    std::lock_guard guard(lock_);
    if (triggered_count_ > 0 &&
        triggered_events_.find(event_handle) != triggered_events_.end()) {
      triggered_count_--;
      triggered_events_[event_handle]--;
      assert(triggered_events_[event_handle] >= 0);
      if (triggered_events_[event_handle] == 0) {
        triggered_events_.erase(event_handle);
      }
      return true;
    }
    return false;
  }

  void TriggerOne() {
    std::lock_guard guard(lock_);
    if (registered_count_ <= 0) {
      return;
    }
    if (write(TriggerInternal(), &i_, sizeof(i_)) != sizeof(i_)) {
      throw arc::exception::IOException("NotifyOne Error");
    }
  }

  void TriggerAll() {
    std::lock_guard guard(lock_);
    std::unordered_set<int> to_notified_event_ids;
    while (registered_count_ > 0) {
      to_notified_event_ids.insert(TriggerInternal());
    }
    for (int to_notified_event_id : to_notified_event_ids) {
      if (write(to_notified_event_id, &i_, sizeof(i_)) != sizeof(i_)) {
        throw arc::exception::IOException("NotifyOne Error");
      }
    }
  }

 private:
  int TriggerInternal() {
    EventHandleType event_handle = in_queue_event_handles_.front();
    in_queue_event_handles_.pop_front();

    registered_count_--;
    registered_events_[event_handle]--;
    if (registered_events_[event_handle] == 0) {
      registered_events_.erase(event_handle);
    }

    triggered_count_++;
    if (triggered_events_.find(event_handle) == triggered_events_.end()) {
      triggered_events_[event_handle] = 0;
    }
    triggered_events_[event_handle]++;

    return event_handle;
  }

  std::mutex lock_;
  int triggered_count_{0};
  int registered_count_{0};
  // event handle -> event counts in the fd
  std::unordered_map<EventHandleType, int> registered_events_;
  std::unordered_map<EventHandleType, int> triggered_events_;

  std::deque<EventHandleType> in_queue_event_handles_;
  std::uint64_t i_{1};
  static_assert(sizeof(i_) == 8U);
};

}  // namespace detail

class ConditionEvent : public UserEvent {
 public:
  ConditionEvent(detail::ConditionCore* core, EventHandleType event_handle,
                 std::coroutine_handle<void> handle)
      : UserEvent(event_handle, handle), core_(core) {
    core_->Register(event_handle_);
  }

  virtual ~ConditionEvent() = default;

  virtual bool CanResume() override {
    return core_->CanDeregister(event_handle_);
  }

 private:
  detail::ConditionCore* core_{nullptr};
};

}  // namespace events
}  // namespace arc

#endif
