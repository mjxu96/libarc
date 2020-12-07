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

// TODO add support for clang
#include <arc/concept/coro.h>

#include <coroutine>
#include <exception>
#include <string>
#include <iostream>

#include <sys/eventfd.h>

namespace arc {
namespace coro {

template <arc::concepts::MoveableObjectOrVoid T>
class Task;

namespace detail {

class TaskBase;
struct PromiseBase;

enum class CoroReturnValueType {
  UNKNOWN = 0U,
  VALUE = 1U,
  EXCEPTION = 2U,
};

class TaskBase {
 public:
  void* ret_{nullptr};
  std::exception_ptr ret_exception_ptr_{nullptr};
  CoroReturnValueType return_type_{CoroReturnValueType::UNKNOWN};

  ~TaskBase() noexcept {
    if (ret_) {
      delete ret_;
      ret_ = nullptr;
    }
    return_type_ = CoroReturnValueType::UNKNOWN;
  }

  bool await_ready() {
    return false;
  }

 protected:
  PromiseBase* promise_{nullptr};
  int event_fd_{-1};
};

struct PromiseBase {
 public:
  PromiseBase() = default;
  auto initial_suspend() noexcept { return std::suspend_never{}; }
  auto final_suspend() noexcept { return std::suspend_never{}; }

  void unhandled_exception() {}
  PromiseBase* get_return_object() noexcept { return this; }

  void SetParentTask(TaskBase* task) { parent_task_ = task; }

 protected:
  TaskBase* parent_task_{nullptr};
};

}  // namespace detail

template <>
class Task<void> : public detail::TaskBase {
 public:
  struct promise_type : public detail::PromiseBase {
    void return_void() {
      parent_task_->return_type_ = detail::CoroReturnValueType::VALUE;
    }
  };

  Task(promise_type* p) noexcept {
    promise_ = p;
    promise_->SetParentTask(this);
  }

  void Result() {
    switch (return_type_) {
      [[unlikely]] case detail::CoroReturnValueType::EXCEPTION
          : std::rethrow_exception(ret_exception_ptr_);
        break;
      default:
        break;
    }
  }

  // coro related
  template<typename PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> parent_promise_handler) {

  }

  void await_resume() {
    Result();
  }

 private:
  // promise_type* promise_{nullptr};
};

template <arc::concepts::MoveableObjectOrVoid T>
class Task : public detail::TaskBase {
 public:
  struct promise_type : public detail::PromiseBase {
    void return_value(T&& value) {
      parent_task_->ret_ = static_cast<void*> new T(std::move(value));
      parent_task_->return_type_ =
          arc::coro::detail::CoroReturnValueType::VALUE;
    }
  };

  Task(promise_type* p) noexcept {
    promise_ = p;
    promise_->SetParentTask(this);
  }

  T Result() {
    switch (return_type_) {
      [[likely]] case detail::CoroReturnValueType::VALUE
          : return std::move(*ret_);
      [[unlikely]] case detail::CoroReturnValueType::EXCEPTION
          : std::rethrow_exception(ret_exception_ptr_);
        break;
      default:
        break;
    }
  }

  // coro related
  template<typename PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> parent_promise_handler) {
    event_fd_ = eventfd(0, EFD_NONBLOCK);
    if (event_fd_ < 0) {
      // ERROR
      // TODO handle it
      std::cerr << "cannot create event fd " << errno << std::endl;
      std::terminate();
    }
  }

  T await_resume() {
    return Result();
  }

 private:
  // promise_type* promise_{nullptr};
};

}  // namespace coro
}  // namespace arc

int main() { arc::coro::Task<void> t(nullptr); }

#endif /* LIBARC__CORO__CORE__TASK_H */
