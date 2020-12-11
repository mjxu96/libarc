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
#include <arc/coro/eventloop.h>
#include <arc/coro/events/coro_task_event.h>
#include <unistd.h>

#include <coroutine>
#include <exception>
#include <iostream>
#include <string>

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
  bool await_ready() { return false; }

 protected:

  
  PromiseBase* promise_{nullptr};
  bool is_outest_coro_{false};

  std::exception_ptr ret_exception_ptr_{nullptr};
  CoroReturnValueType return_type_{CoroReturnValueType::UNKNOWN};
};

struct PromiseBase {
 public:
  PromiseBase() = default;

  template<typename PromiseType>
  void CreateAndAddCoroEvent(std::coroutine_handle<PromiseType> handle) {
    event_ptr_ = new events::CoroTaskEvent<PromiseType>(handle);
    GetLocalEventLoop().AddEvent(event_ptr_);
  }

  void TriggerCoroEvent() {
    if (event_ptr_) {
      std::uint64_t temp = 1;
      if (write(event_ptr_->GetFd(), (void*)&temp, sizeof(temp)) < 0) {
        // TODO change this
        throw std::logic_error("write is error");
      }
    }
  }

  auto initial_suspend() noexcept { 
    return std::suspend_always{};
  }
  auto final_suspend() noexcept {
    return std::suspend_never{};
  }

  void unhandled_exception() {}

  detail::TaskBase* parent_task_ptr_{nullptr};
  events::EventBase* event_ptr_{nullptr};
};

}  // namespace detail

template <>
class Task<void> : public detail::TaskBase {
 public:
  struct promise_type : public detail::PromiseBase {
    void return_void() {
      TriggerCoroEvent();
      (static_cast<Task<void>*>(parent_task_ptr_))->return_type_ =
          detail::CoroReturnValueType::VALUE;
    }
    promise_type* get_return_object() noexcept { return this; }
  };

  Task(promise_type* p) noexcept {
    promise_ = p;
    p->parent_task_ptr_ = static_cast<detail::TaskBase*>(this);
  }

  void Result() {
    switch (return_type_) {
      [[unlikely]] case detail::CoroReturnValueType::EXCEPTION
          : std::rethrow_exception(ret_exception_ptr_);
      break;
      default:
        break;
    }
    return;
  }

  // coro related
  template<typename PromiseType>
  [[nodiscard]] std::coroutine_handle<promise_type> await_suspend(
      std::coroutine_handle<PromiseType> parent_promise_handler) {
    promise_->CreateAndAddCoroEvent<PromiseType>(parent_promise_handler);
    return std::coroutine_handle<promise_type>::from_promise(
        *(static_cast<promise_type*>(promise_)));
  }

  void await_resume() { Result(); }

  void Start() {
    is_outest_coro_ = true;
    auto handle = std::coroutine_handle<promise_type>::from_promise(
        *(static_cast<promise_type*>(promise_)));
    assert(!handle.done());
    handle.resume();
  }
};

template <arc::concepts::MoveableObjectOrVoid T>
class Task : public detail::TaskBase {
 public:
  struct promise_type : public detail::PromiseBase {
    template <arc::concepts::MoveableObjectOrVoid U = T>
    void return_value(U&& value) {
      TriggerCoroEvent();
      (static_cast<Task<T>*>(parent_task_ptr_))->ret_ =
          new std::remove_reference_t<U>(std::move(std::forward<U&&>(value)));
      (static_cast<Task<T>*>(parent_task_ptr_))->return_type_ =
          arc::coro::detail::CoroReturnValueType::VALUE;
    }
    promise_type* get_return_object() noexcept { return this; }
  };

  Task(promise_type* p) noexcept {
    promise_ = p;
    p->parent_task_ptr_ = static_cast<detail::TaskBase*>(this);
  }

  ~Task() {
    if (ret_) {
      delete ret_;
      ret_ = nullptr;
    }
  }

  T Result() {
    switch (return_type_) {
      [[likely]] case detail::CoroReturnValueType::VALUE
          : return std::move(*((T*)ret_));
      [[unlikely]] case detail::CoroReturnValueType::EXCEPTION
          : std::rethrow_exception(ret_exception_ptr_);
      break;
      default:
        // TODO change this
        throw std::logic_error("cannot be here");
    }
  }

  // coro related
  template<typename PromiseType>
  [[nodiscard]] std::coroutine_handle<promise_type> await_suspend(
      std::coroutine_handle<PromiseType> parent_promise_handler) {
    promise_->CreateAndAddCoroEvent<PromiseType>(parent_promise_handler);
    return std::coroutine_handle<promise_type>::from_promise(
        *(static_cast<promise_type*>(promise_)));
  }

  T await_resume() { return Result(); }

 private:
  T* ret_{nullptr};
};

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__CORE__TASK_H */
