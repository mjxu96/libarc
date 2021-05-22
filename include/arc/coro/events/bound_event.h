/*
 * File: bound_event.h
 * Project: libarc
 * File Created: Thursday, 20th May 2021 7:13:35 pm
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

#ifndef LIBARC__CORO__EVENTS__BOUND_EVENT_H
#define LIBARC__CORO__EVENTS__BOUND_EVENT_H

#include "io_event.h"
#include "time_event.h"
#include "user_event.h"

namespace arc {
namespace coro {

namespace detail {

enum class TriggerType {
  TIME_EVENT = 0U,
  USER_EVENT,
};

enum class BoundType {
  IO_EVENT = 0U,
  USER_EVENT,
  TIME_EVENT,
};

}  // namespace detail

// This event will never resume
class BoundEvent : virtual public EventBase {
 public:
  BoundEvent(UserEvent* event, detail::TriggerType trigger_type)
      : EventBase(nullptr),
        bound_event_id_(event->GetEventID()),
        bound_event_(event),
        trigger_type_(trigger_type),
        bound_event_type_(detail::BoundType::USER_EVENT) {}
  BoundEvent(IOEvent* event, detail::TriggerType trigger_type)
      : EventBase(nullptr),
        bound_event_id_(event->GetEventID()),
        bound_helper_(event->GetFd()),
        bound_event_(event),
        trigger_type_(trigger_type),
        bound_event_type_(detail::BoundType::IO_EVENT) {}
  BoundEvent(TimeEvent* event, detail::TriggerType trigger_type)
      : EventBase(nullptr),
        bound_event_id_(event->GetEventID()),
        bound_helper_(event->GetWakeupTime()),
        bound_event_(event),
        trigger_type_(trigger_type),
        bound_event_type_(detail::BoundType::TIME_EVENT) {}

  virtual ~BoundEvent() = default;

  inline const detail::BoundType GetBountEventType() const { return bound_event_type_; }
  
  inline const detail::TriggerType GetTriggerType() const { return trigger_type_; }

  inline const EventID GetBountEventID() const { return bound_event_id_; }

  inline const std::int64_t GetBoundHelper() const { return bound_helper_; }

  inline EventBase* GetBoundEvent() const { return bound_event_; }

  inline void SetIterator(const std::list<BoundEvent*>::iterator& itr) { event_itr_ = itr; }
  inline const std::list<BoundEvent*>::iterator& GetIterator() const { return event_itr_; }

 private:
  EventID bound_event_id_;
  std::int64_t bound_helper_;
  EventBase* bound_event_;
  detail::TriggerType trigger_type_;
  detail::BoundType bound_event_type_;
  std::list<BoundEvent*>::iterator event_itr_;
};

}  // namespace coro
}  // namespace arc

#endif