/*
 * File: tls_io_awaiter.h
 * Project: libarc
 * File Created: Wednesday, 14th April 2021 10:58:54 pm
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

#ifndef LIBARC__CORO__AWAITER_TLS_IO_AWAITER_H
#define LIBARC__CORO__AWAITER_TLS_IO_AWAITER_H

#include <arc/coro/eventloop.h>
#include <arc/coro/events/io_event_base.h>
#include <arc/io/utils.h>

namespace arc {
namespace coro {

class [[nodiscard]] TLSIOAwaiter {
 public:
  TLSIOAwaiter(int fd, arc::io::IOType next_op)
      : fd_(fd), next_op_(next_op) {}

  bool await_ready() { return false; }

  template <typename PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    GetEventLoop().AddIOEvent(
        new events::detail::IOEventBase(fd_, next_op_, handle));
  }

  void await_resume() { return; }

 private:
  int fd_{-1};
  arc::io::IOType next_op_{};
};

}  // namespace coro
}  // namespace arc

#endif