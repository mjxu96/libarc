/*
 * File: event_base.h
 * Project: libarc
 * File Created: Monday, 7th December 2020 10:26:45 pm
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

#ifndef LIBARC__CORO__EVENTS__EVENT_BASE_H
#define LIBARC__CORO__EVENTS__EVENT_BASE_H

#include <unistd.h>
#include <cassert>
#ifdef __clang__
#include <experimental/coroutine>
namespace std {
using experimental::coroutine_handle;
}
#else
#include <coroutine>
#endif

namespace arc {
namespace coro {

using EventID = int;

class EventBase {
 public:
  EventBase(std::coroutine_handle<void> handle) : handle_(handle) {}
  virtual ~EventBase() {}

  virtual void Resume() {
    assert(!handle_.done());
    handle_.resume();
  }

  inline void SetEventID(EventID event_id) { event_id_ = event_id; }

  inline void SetInterrupted(bool is_interrupted) {
    is_interrupted_ = is_interrupted;
  }

  inline const EventID GetEventID() const { return event_id_; }

  inline const bool IsInterrupted() const { return is_interrupted_; }

 protected:
  std::coroutine_handle<void> handle_{nullptr};
  EventID event_id_{-1};
  bool is_interrupted_{false};
};

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__EVENTS__EVENT_BASE_H */
