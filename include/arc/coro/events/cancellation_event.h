/*
 * File: cancellationl_event.h
 * Project: libarc
 * File Created: Wednesday, 19th May 2021 8:11:25 pm
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

#ifndef LIBARC__CORO__EVENTS__CANCELLATION_EVENT_H
#define LIBARC__CORO__EVENTS__CANCELLATION_EVENT_H

#include "io_event.h"
#include "time_event.h"
#include "user_event.h"

namespace arc {
namespace coro {

namespace detail {

enum class CancellationType {
  IO_EVENT = 0U,
  USER_EVENT,
  TIME_EVENT,
};

}  // namespace detail

// This event will never resume
class CancellationEvent : public EventBase {
 public:
  CancellationEvent(UserEvent* event)
      : EventBase(nullptr),
        bind_event_id_(event->GetEventID()),
        bind_event_(event),
        type_(detail::CancellationType::USER_EVENT) {}
  CancellationEvent(IOEvent* event)
      : EventBase(nullptr),
        bind_event_id_(event->GetEventID()),
        bind_helper_(event->GetFd()),
        bind_event_(event),
        type_(detail::CancellationType::IO_EVENT) {}
  CancellationEvent(TimeEvent* event)
      : EventBase(nullptr),
        bind_event_id_(event->GetEventID()),
        bind_helper_(event->GetWakeupTime()),
        bind_event_(event),
        type_(detail::CancellationType::TIME_EVENT) {}

  virtual ~CancellationEvent() = default;

  inline const detail::CancellationType GetType() const { return type_; }

  inline const int GetBindEventID() const { return bind_event_id_; }

  inline const std::int64_t GetBindHelper() const { return bind_helper_; }

  inline EventBase* GetEvent() const { return bind_event_; }

  inline void SetIterator(const std::list<CancellationEvent*>::iterator& itr) { event_itr_ = itr; }
  inline const std::list<CancellationEvent*>::iterator& GetIterator() const { return event_itr_; }

 private:
  int bind_event_id_;
  std::int64_t bind_helper_;
  EventBase* bind_event_;
  detail::CancellationType type_;
  std::list<CancellationEvent*>::iterator event_itr_;
};

}  // namespace coro
}  // namespace arc

#endif