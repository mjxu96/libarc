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

#include <arc/concept/coro.h>
#include <arc/coro/eventloop.h>
#include <arc/coro/events/io_event.h>
#include <arc/exception/io.h>

#include <functional>

namespace arc {
namespace coro {

template <typename ReadyFunctor, typename ResumeFunctor>
class [[nodiscard]] IOAwaiter {
 public:
  IOAwaiter(ReadyFunctor && ready_functor, ResumeFunctor && resume_functor,
            int fd, io::IOType io_type)
      : ready_functor_(std::forward<ReadyFunctor>(ready_functor)),
        resume_functor_(std::forward<ResumeFunctor>(resume_functor)),
        fd_(fd),
        io_type_(io_type) {}

  bool await_ready() { return ready_functor_(); }

  typename std::invoke_result_t<ResumeFunctor> await_resume() {
    return resume_functor_();
  }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    GetLocalEventLoop().AddIOEvent(new events::IOEvent(fd_, io_type_, handle));
  }

 private:
  io::IOType io_type_;
  int fd_;

  ResumeFunctor resume_functor_;
  ReadyFunctor ready_functor_;
};

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__AWAITER__IO_AWAITER_H */
