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
#include <arc/io/utils.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>

#include <coroutine>
#include <iostream>

namespace arc {
namespace events {
namespace detail {

enum class IOEventType { READ = 0U, WRITE = 1U };

class IOEventBase : public EventBase {
 public:
  IOEventBase(int fd, io::IOType io_type, std::coroutine_handle<void> handle)
      : EventBase(handle), fd_(fd), io_type_(io_type) {
    if (io_type == io::IOType::READ || io_type == io::IOType::ACCEPT) {
      io_event_type_ = detail::IOEventType::READ;
      if (io_type == io::IOType::ACCEPT) {
        should_remove_everytime_ = false;
      }
    } else {
      io_event_type_ = detail::IOEventType::WRITE;
    }
  }

  virtual ~IOEventBase() {}

  inline int GetFd() const noexcept { return fd_; }
  inline detail::IOEventType GetIOEventType() const noexcept {
    return io_event_type_;
  }
  inline io::IOType GetIOType() const noexcept { return io_type_; }
  inline bool ShouldRemoveEveryTime() const noexcept {
    return should_remove_everytime_;
  }

 protected:
  int fd_{-1};
  io::IOType io_type_{};
  detail::IOEventType io_event_type_{};
  bool should_remove_everytime_{true};
};

}  // namespace detail
}  // namespace events
}  // namespace arc

#endif /* LIBARC__CORO__EVENTS__IO_EVENT_BASE_H */
