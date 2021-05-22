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
#include <arc/coro/events/timeout_event.h>
#include <arc/coro/utils/cancellation_token.h>
#include <arc/exception/io.h>

#include <functional>
#include <variant>

namespace arc {
namespace coro {

template <typename ReadyFunctor, typename ResumeFunctor>
class [[nodiscard]] IOAwaiter {
 public:
  IOAwaiter(ReadyFunctor&& ready_functor, ResumeFunctor&& resume_functor,
            int fd, io::IOType io_type)
      : ready_functor_(std::forward<ReadyFunctor>(ready_functor)),
        resume_functor_(std::forward<ResumeFunctor>(resume_functor)),
        resume_interrupted_functor_(resume_functor_),
        fd_(fd),
        io_type_(io_type) {}

  IOAwaiter(ReadyFunctor&& ready_functor, ResumeFunctor&& resume_functor,
            ResumeFunctor&& resume_interrucpted_functor, int fd,
            io::IOType io_type, const CancellationToken& token)
      : ready_functor_(std::forward<ReadyFunctor>(ready_functor)),
        resume_interrupted_functor_(
            std::forward<ResumeFunctor>(resume_interrucpted_functor)),
        resume_functor_(std::forward<ResumeFunctor>(resume_functor)),
        fd_(fd),
        io_type_(io_type),
        abort_handle_(std::make_shared<CancellationToken>(token)) {}

  IOAwaiter(ReadyFunctor&& ready_functor, ResumeFunctor&& resume_functor,
            ResumeFunctor&& resume_interrucpted_functor, int fd,
            io::IOType io_type,
            const std::chrono::steady_clock::duration& sleep_time)
      : ready_functor_(std::forward<ReadyFunctor>(ready_functor)),
        resume_interrupted_functor_(
            std::forward<ResumeFunctor>(resume_interrucpted_functor)),
        resume_functor_(std::forward<ResumeFunctor>(resume_functor)),
        fd_(fd),
        io_type_(io_type),
        abort_handle_(std::chrono::duration_cast<std::chrono::milliseconds>(
                          (std::chrono::steady_clock::now() + sleep_time)
                              .time_since_epoch())
                          .count()) {}

  bool await_ready() { return ready_functor_(); }

  typename std::invoke_result_t<ResumeFunctor> await_resume() {
    if (abort_handle_.index() != 0 && io_event_->IsInterrupted()) [[unlikely]] {
      return resume_interrupted_functor_();
    }
    return resume_functor_();
  }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    io_event_ = new coro::IOEvent(fd_, io_type_, handle);
    auto event_loop = &EventLoop::GetLocalInstance();
    event_loop->AddIOEvent(io_event_);
    if (abort_handle_.index() == 1) [[unlikely]] {
      auto cancellation_event = new coro::CancellationEvent(io_event_);
      std::get<1>(abort_handle_)
          ->SetEventAndLoop(cancellation_event, event_loop);
    } else if (abort_handle_.index() == 2) [[unlikely]] {
      auto timeout_event =
          new coro::TimeoutEvent(std::get<2>(abort_handle_), io_event_);
      event_loop->AddBoundEvent(timeout_event);
    }
  }

 private:
  io::IOType io_type_;
  int fd_;

  ResumeFunctor resume_functor_;
  ResumeFunctor resume_interrupted_functor_;
  ReadyFunctor ready_functor_;

  std::variant<std::monostate, std::shared_ptr<CancellationToken>, std::int64_t>
      abort_handle_;

  IOEvent* io_event_{nullptr};
};

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__AWAITER__IO_AWAITER_H */
