/*
 * File: io_awaiter.h
 * Project: libarc
 * File Created: Sunday, 13th December 2020 3:58:54 pm
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

#ifndef LIBARC__CORO__AWAITER__IO_AWAITER_H
#define LIBARC__CORO__AWAITER__IO_AWAITER_H

#include <arc/coro/eventloop.h>
#include <arc/coro/events/io_event_base.h>
#include <arc/io/socket.h>

namespace arc {
namespace coro {

template <net::Domain AF, net::Protocol P, detail::IOType T>
class IOAwaiter {
 public:
  IOAwaiter(io::Socket<AF, P, io::Pattern::ASYNC>* sock, void* storage)
      : sock_(sock), storage_(storage) {}

  bool await_ready() { return false; }

  template <typename PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    events::detail::IOEventType type =
        ((T == detail::IOType::ACCEPT || T == detail::IOType::READ)
             ? events::detail::IOEventType::READ
             : events::detail::IOEventType::WRITE);
    GetLocalEventLoop().AddIOEvent(
        new events::detail::IOEventBase(sock_->GetFd(), type, handle));
  }

  template <detail::IOType UT = T>
  requires(UT == detail::IOType::ACCEPT)
      io::Socket<AF, P, io::Pattern::ASYNC> await_resume() {
    return sock_->InternalAccept();
  }

  template <detail::IOType UT = T>
  requires(UT == detail::IOType::CONNECT)
  void await_resume() {
    return sock_->InternalConnect(*(static_cast<net::Address<AF>*>(storage_)));
  }

  template <detail::IOType UT = T>
  requires(UT == detail::IOType::READ)
  std::string await_resume() {
    return sock_->InternalRecv(*(static_cast<int*>(storage_)));
  }

  template <detail::IOType UT = T>
  requires(UT == detail::IOType::WRITE)
  int await_resume() {
    return sock_->InternalSend(*(static_cast<std::string*>(storage_)));
  }

 private:
  void* storage_{nullptr};
  io::Socket<AF, P, io::Pattern::ASYNC>* sock_{nullptr};
};

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__AWAITER__IO_AWAITER_H */
