/*
 * File: timeout_event.h
 * Project: libarc
 * File Created: Thursday, 20th May 2021 7:48:55 pm
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

#ifndef LIBARC__CORO__EVENTS__TIMEOUT_EVENT_H
#define LIBARC__CORO__EVENTS__TIMEOUT_EVENT_H

#include "bound_event.h"

namespace arc {
namespace coro {

class TimeoutEvent : public TimeEvent, public BoundEvent {
 public:
  TimeoutEvent(std::int64_t wakeup_time, UserEvent* event)
      : TimeEvent(wakeup_time, nullptr),
        BoundEvent(event, detail::TriggerType::TIME_EVENT),
        EventBase(nullptr) {
    is_trigger_ = true;
  }
  TimeoutEvent(std::int64_t wakeup_time, IOEvent* event)
      : TimeEvent(wakeup_time, nullptr),
        BoundEvent(event, detail::TriggerType::TIME_EVENT),
        EventBase(nullptr) {
    is_trigger_ = true;
  }
};

}  // namespace coro
}  // namespace arc

#endif