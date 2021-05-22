/*
 * File: executor_awaiter.h
 * Project: libarc
 * File Created: Saturday, 22nd May 2021 1:19:12 pm
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

#ifndef LIBARC__CORO__AWAITER__EXECUTOR_AWAITER_H
#define LIBARC__CORO__AWAITER__EXECUTOR_AWAITER_H

#include <arc/concept/coro.h>
#include <arc/coro/eventloop.h>
#include <arc/coro/events/user_event.h>
#include <arc/utils/thread_pool.h>

#include <functional>

namespace arc {
namespace coro {

template <typename Functor, typename... Args>
class ExecutorAwaiter {
 public:
  using RetType = typename std::result_of<Functor(Args...)>::type;
  ExecutorAwaiter(Functor&& functor, Args&&... args)
      : functor_(std::bind(std::forward<Functor>(functor),
                           std::forward<Args>(args)...)) {}

  bool await_ready() { return false; }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    UserEvent* event = new UserEvent(handle);
    EventLoop* loop = &GetLocalEventLoop();
    loop->AddUserEvent(event);
    future_ = utils::ThreadPool::GetInstance().Enqueue(loop, event, functor_);
  }

  RetType await_resume() {
    assert(future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    return future_.get();
  }

 private:
  std::function<RetType()> functor_;
  std::future<RetType> future_;
};

}  // namespace coro
}  // namespace arc

#endif