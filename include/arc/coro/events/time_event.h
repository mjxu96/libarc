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

#include <chrono>
#include "event_base.h"

namespace arc {
namespace coro {

class TimeEvent : virtual public EventBase {
 public:
  TimeEvent(std::int64_t wakeup_time, std::coroutine_handle<void> handle)
      : EventBase(handle), wakeup_time_(wakeup_time) {}

  virtual ~TimeEvent() {}

  const inline std::int64_t GetWakeupTime() const { return wakeup_time_; }

  const inline bool IsValid() const { return is_valid_; }
  inline void SetValidity(bool is_valid) { is_valid_ = is_valid; }

  const inline bool IsTrigger() const { return is_trigger_; }

  friend class TimeEventComparator;

 protected:
  std::int64_t wakeup_time_ = 0;
  bool is_valid_{true};
  bool is_trigger_{false};
};

class TimeEventComparator {
 public:
  bool operator()(TimeEvent* event1, TimeEvent* event2) {
    return event1->wakeup_time_ > event2->wakeup_time_;
  }
};

}  // namespace coro
}  // namespace arc
#endif
