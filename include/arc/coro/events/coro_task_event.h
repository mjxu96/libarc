/*
 * File: coro_task_event.h
 * Project: libarc
 * File Created: Monday, 7th December 2020 10:28:33 pm
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

#ifndef LIBARC__CORO__EVENTS__CORO_TASK_EVENT_H
#define LIBARC__CORO__EVENTS__CORO_TASK_EVENT_H

#include <sys/eventfd.h>

#ifdef __clang__
#include <experimental/coroutine>
namespace std {
  using experimental::coroutine_handle;
}
#else
#include <coroutine>
#endif
#include <cstdint>

#include "event_base.h"

namespace arc {
namespace events {

class CoroTaskEvent : public detail::EventBase {
 public:
  CoroTaskEvent() : detail::EventBase(nullptr) {}
  CoroTaskEvent(std::coroutine_handle<> handle) : detail::EventBase(handle) {}
  ~CoroTaskEvent() = default;

  inline void SetCoroId(std::uint64_t id) noexcept { id_ = id; }

  inline std::uint64_t GetCoroId() const noexcept { return id_; }

  void SetCoroutineHandle(std::coroutine_handle<> handle) { handle_ = handle; }

 private:
  std::uint64_t id_{0};
};

}  // namespace events
}  // namespace arc

#endif /* LIBARC__CORO__EVENTS__CORO_TASK_EVENT_H */
