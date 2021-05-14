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

#include <mutex>
#include <deque>
#include <unordered_map>

#include "user_event.h"

namespace arc {

namespace events {

namespace detail {

struct ConditionCore {
  std::mutex lock;
  int triggered_count{0};
  int registered_count{0};
  // eventfd -> event counts in the fd
  std::unordered_map<int, int> registered_events;
  std::unordered_map<int, int> triggered_events;

  std::deque<int> in_queue_event_ids;
};

}  // namespace detail

class ConditionEvent : public UserEvent {
 public:
  ConditionEvent(detail::ConditionCore* core,
                 std::coroutine_handle<void> handle)
      : UserEvent(handle),
        core_(core),
        event_id_(-1) {
    std::lock_guard guard(core_->lock);
  }

  virtual ~ConditionEvent() = default;

  virtual bool CanResume() override {
    std::lock_guard guard(core_->lock);
    if (core_->triggered_count > 0 && core_->triggered_events.find(event_id_) !=
                                          core_->triggered_events.end()) {
      core_->triggered_count--;
      core_->triggered_events[event_id_]--;
      assert(core_->triggered_events[event_id_] >= 0);
      if (core_->triggered_events[event_id_] == 0) {
        core_->triggered_events.erase(event_id_);
      }
      return true;
    }
    return false;
  }

  virtual void ArmEvent(int event_id) override {
    event_id_ = event_id;
    std::lock_guard guard(core_->lock);
    if (core_->registered_events.find(event_id_) ==
        core_->registered_events.end()) {
      core_->registered_events[event_id_] = 0;
    }
    core_->registered_events[event_id_]++;
    core_->registered_count++;
    core_->in_queue_event_ids.push_back(event_id_);
  }

 private:
  detail::ConditionCore* core_{nullptr};
  int event_id_{-1};
};

}  // namespace events
}  // namespace arc

#endif