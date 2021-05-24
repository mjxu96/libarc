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
#include <list>

namespace arc {
namespace coro {

class UserEvent : virtual public EventBase {
 public:
  UserEvent(std::coroutine_handle<void> handle)
      : EventBase(handle) {}
  virtual ~UserEvent() = default;

  void SetIterator(const std::list<UserEvent*>::iterator& itr) { event_itr_ = itr; }
  const std::list<UserEvent*>::iterator& GetIterator() const { return event_itr_; }

 protected:
  std::list<UserEvent*>::iterator event_itr_;
};

}  // namespace coro
}  // namespace arc

#endif
