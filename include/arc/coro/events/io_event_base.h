/*
 * File: io_event_base.h
 * Project: libarc
 * File Created: Saturday, 12th December 2020 11:05:14 am
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

#ifndef LIBARC__CORO__EVENTS__IO_EVENT_BASE_H
#define LIBARC__CORO__EVENTS__IO_EVENT_BASE_H

#include <arc/coro/events/event_base.h>
#include <assert.h>
#include <unistd.h>

#include <coroutine>
#include <iostream>

namespace arc {
namespace events {
namespace detail {

enum class IOEventType {
  NONE = 0U,
  READ = 1U,
  WRITE = 2U,
};

class IOEventBase : public EventBase {
 public:
  IOEventBase(int fd, IOEventType event_type,
              std::coroutine_handle<void> handle)
      : EventBase(handle), fd_(fd), io_event_type_(event_type) {}
  virtual ~IOEventBase() {
    if (fd_ > 0) {
      if (close(fd_) < 0) {
        std::cerr << "close " << fd_ << " error: " << errno << std::endl;
      }
      fd_ = -1;
    }
  }

  inline int GetFd() const noexcept { return fd_; }
  inline IOEventType GetIOEventType() const noexcept { return io_event_type_; }

 protected:
  int fd_{-1};
  IOEventType io_event_type_{IOEventType::NONE};
};

}  // namespace detail
}  // namespace events
}  // namespace arc

#endif /* LIBARC__CORO__EVENTS__IO_EVENT_BASE_H */
