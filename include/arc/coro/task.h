/*
 * File: task.h
 * Project: libarc
 * File Created: Monday, 7th December 2020 8:16:02 pm
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

#ifndef LIBARC__CORO__TASK_H
#define LIBARC__CORO__TASK_H

#include <arc/concept/coro.h>
#include <arc/coro/awaiter/time_awaiter.h>
#include <arc/coro/eventloop.h>
#include <unistd.h>

#ifdef __clang__
#include <experimental/coroutine>
namespace std {
using experimental::coroutine_handle;
using experimental::noop_coroutine;
using experimental::suspend_always;
using experimental::suspend_never;
}  // namespace std
#else
#include <coroutine>
#endif
#include <exception>
#include <string>

namespace arc {
namespace coro {

enum class ReturnType {
  NONE = 0U,
  VALUE,
  EXCEPTION,
};

class PromiseBase {
 public:
  PromiseBase() = default;
  auto initial_suspend() { return std::suspend_always{}; }

  auto final_suspend() noexcept { return FinalAwaiter{}; }

  void SetContinuation(std::coroutine_handle<> continuation) {
    continuation_coro_ = continuation;
  }

  void SetNeedClean(bool need_clean = true) { need_manual_clean_ = need_clean; }

 protected:
  friend struct FinalAwaiter;
  struct FinalAwaiter {
    FinalAwaiter() noexcept = default;

    bool await_ready() noexcept { return false; }

    template <arc::concepts::PromiseT PromiseType>
    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<PromiseType> coro) noexcept {
      return coro.promise().continuation_coro_;
    }

    void await_resume() noexcept {}
  };

  std::coroutine_handle<> continuation_coro_{std::noop_coroutine()};
  ReturnType return_type_{ReturnType::NONE};
  bool need_manual_clean_{false};
};

template <typename T>
class TaskPromise : public PromiseBase {
 public:
  TaskPromise() noexcept {}
  ~TaskPromise() {
    switch (return_type_) {
      case ReturnType::VALUE:
        value_.~T();
        break;

      case ReturnType::EXCEPTION:
        exception_ptr_.~exception_ptr();
        break;

      default:
        break;
    }
  }

  TaskPromise* get_return_object() { return this; }

  void unhandled_exception() {
    if (need_manual_clean_) {
      EventLoop::GetLocalInstance().AddToCleanUpCoroutine(
          std::coroutine_handle<TaskPromise<T>>::from_promise(*this));
    }
    return_type_ = ReturnType::EXCEPTION;
    // ::new (static_cast<void*>(std::addressof(exception_ptr_)))
    //     std::exception_ptr(std::current_exception());
    exception_ptr_ = std::current_exception();
    if (continuation_coro_ == std::noop_coroutine()) {
      std::rethrow_exception(exception_ptr_);
    }
  }

  template <typename U>
  void return_value(U&& value) {
    if (return_type_ == ReturnType::EXCEPTION) {
      return;
    }
    if (need_manual_clean_) {
      EventLoop::GetLocalInstance().AddToCleanUpCoroutine(
          std::coroutine_handle<TaskPromise<T>>::from_promise(*this));
    }
    // new placement to initialize the value_ in union
    ::new (static_cast<void*>(std::addressof(value_)))
        T(std::forward<U>(value));
    return_type_ = ReturnType::VALUE;
  }

  template <typename U = T>
  requires(!arc::concepts::Movable<U>) U& Result() {
    if (this->return_type_ == ReturnType::EXCEPTION) {
      std::rethrow_exception(this->exception_ptr_);
    }
    assert(this->return_type_ == ReturnType::VALUE);
    return this->value_;
  }

  template <typename U = T>
  requires(arc::concepts::Movable<U>) U&& Result() {
    if (this->return_type_ == ReturnType::EXCEPTION) {
      std::rethrow_exception(this->exception_ptr_);
    }
    assert(this->return_type_ == ReturnType::VALUE);
    return std::move(this->value_);
  }

 private:
  union {
    T value_;
    std::exception_ptr exception_ptr_{nullptr};
  };
};

template <>
class TaskPromise<void> : public PromiseBase {
 public:
  TaskPromise() = default;
  ~TaskPromise() {
    if (return_type_ == ReturnType::EXCEPTION) {
      exception_ptr_.~exception_ptr();
    }
  }

  void unhandled_exception() {
    if (need_manual_clean_) {
      EventLoop::GetLocalInstance().AddToCleanUpCoroutine(
          std::coroutine_handle<TaskPromise<void>>::from_promise(*this));
    }
    return_type_ = ReturnType::EXCEPTION;
    exception_ptr_ = std::current_exception();
    if (continuation_coro_ == std::noop_coroutine()) {
      std::rethrow_exception(exception_ptr_);
    }
  }

  TaskPromise* get_return_object() { return this; }

  void return_void() {
    if (need_manual_clean_) {
      EventLoop::GetLocalInstance().AddToCleanUpCoroutine(
          std::coroutine_handle<TaskPromise<void>>::from_promise(*this));
    }
    if (return_type_ == ReturnType::EXCEPTION) {
      return;
    }
    return_type_ = ReturnType::VALUE;
  }

  void Result() {
    if (return_type_ == ReturnType::EXCEPTION) {
      std::rethrow_exception(exception_ptr_);
    }
    return;
  }

 private:
  std::exception_ptr exception_ptr_{nullptr};
};

template <arc::concepts::CopyableMoveableOrVoid T>
class [[nodiscard]] Task {
 public:
  using promise_type = TaskPromise<T>;

  Task(promise_type * promise)
      : coroutine_(
            std::coroutine_handle<promise_type>::from_promise(*promise)) {}

  Task(Task && other) : coroutine_(other.coroutine_) {
    other.coroutine_ = nullptr;
  }

  Task(const Task&) = delete;

  ~Task() {
    if (coroutine_) {
      if (coroutine_.done()) {
        coroutine_.destroy();
      } else {
        coroutine_.promise().SetNeedClean(need_clean_);
      }
    }
  }

  Task& operator=(Task&& other) {
    if (std::addressof(other) != this) {
      if (coroutine_) {
        coroutine_.destroy();
      }
      coroutine_ = other.coroutine_;
      other.coroutine_ = nullptr;
    }
    return *this;
  }

  Task& operator=(const Task& other) = delete;

  void Start(bool need_clean = false) {
    SetNeedClean(need_clean);
    coroutine_.resume();
  }

  std::coroutine_handle<void> GetCoroutine() const { return coroutine_; }

  void SetNeedClean(bool need_clean = false) { need_clean_ = need_clean; }

  bool await_ready() { return (!coroutine_ || coroutine_.done()); }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) {
    coroutine_.promise().SetContinuation(continuation);
    return coroutine_;
  }
  T await_resume() { return coroutine_.promise().Result(); }

 private:
  std::coroutine_handle<promise_type> coroutine_{nullptr};
  bool need_clean_{false};
};

void EnsureFuture(Task<void>&& task);

void RunUntilComplete();

void StartEventLoop(Task<void>&& task);

TimeAwaiter SleepFor(const std::chrono::steady_clock::duration& duration);

TimeAwaiter Yield();

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__CORE__TASK_H */
