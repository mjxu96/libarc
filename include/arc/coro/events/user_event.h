/*
 * File: user_event.h
 * Project: libarc
 * File Created: Friday, 14th May 2021 11:08:55 pm
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

#ifndef LIBARC__CORO__EVENTS__USER_EVENT_H
#define LIBARC__CORO__EVENTS__USER_EVENT_H

#include "event_base.h"

namespace arc {
namespace events {

#ifdef __linux__
using EventHandleType = int;
#endif

class UserEvent : public EventBase {
 public:
  UserEvent(EventHandleType event_handle, std::coroutine_handle<void> handle) : EventBase(handle), event_handle_(event_handle) {}
  ~UserEvent() = default;
  virtual bool CanResume() { return true; }
  virtual EventHandleType GetEventHandle() const { return event_handle_; }
 protected:
  EventHandleType event_handle_{-1};
};

} // namespace events
} // namespace arc

#endif