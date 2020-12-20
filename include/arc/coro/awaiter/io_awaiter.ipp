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
// #include <arc/io/socket.h>

namespace arc {
namespace coro {

template <net::Domain AF, net::Protocol P, io::IOType T>
class IOAwaiter {
 public:
  using StorageType = typename std::conditional_t<
      T == io::IOType::READ, int,
      typename std::conditional_t<
          T == io::IOType::WRITE, const std::string*,
          typename std::conditional_t<
              T == io::IOType::CONNECT, const net::Address<AF>*, void*>>>;

  IOAwaiter(io::Socket<AF, P, io::Pattern::ASYNC>* sock, StorageType storage)
      : sock_(sock), storage_(storage) {}

  template <io::IOType UT = T>
  requires(UT == io::IOType::ACCEPT) bool await_ready() {
    return dynamic_cast<io::Acceptor<AF, io::Pattern::ASYNC>*>(sock_)
        ->HasCachedAvailableSocket();
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::CONNECT) bool await_ready() {
    if (sock_->InternalTryConnect(*storage_)) {
      return true;
    } else {
      if (errno == EINPROGRESS) {
        return false;
      }
    }
    arc::utils::ThrowErrnoExceptions();
    return true;
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::WRITE) bool await_ready() {
    written_size_ = sock_->InternalTrySend(*storage_);
    if (written_size_ < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return false;
      }
      arc::utils::ThrowErrnoExceptions();
    }
    return true;
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::READ) bool await_ready() {
    return false;
  }

  template <typename PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    GetLocalEventLoop().AddIOEvent(
        new events::detail::IOEventBase(sock_->GetFd(), T, handle));
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::ACCEPT)
      io::Socket<AF, P, io::Pattern::ASYNC> await_resume() {
    return dynamic_cast<io::Acceptor<AF, io::Pattern::ASYNC>*>(sock_)
        ->GetNextAvailableSocket();
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::CONNECT) void await_resume() {
    return;
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::READ) std::string await_resume() {
    return sock_->InternalRecv(storage_);
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::WRITE) int await_resume() {
    if (written_size_ >= 0) [[likely]] {
      return written_size_;
    }
    return sock_->InternalSend(*storage_);
  }

 private:
  StorageType storage_{nullptr};
  io::Socket<AF, P, io::Pattern::ASYNC>* sock_{nullptr};
  int written_size_{0};
};

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__AWAITER__IO_AWAITER_H */
