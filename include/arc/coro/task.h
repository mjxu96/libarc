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

template <arc::concepts::CopyableMoveableOrVoid T>
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

  friend struct PromiseBase;

  std::exception_ptr CopyExceptionPtr() {
    if (!ret_exception_ptr_) {
      return ret_exception_ptr_;
    }
    try {
      std::rethrow_exception(ret_exception_ptr_);
    } catch (const std::exception& e) {
      return std::current_exception();
    }
  }

 protected:
  PromiseBase* promise_{nullptr};

  std::exception_ptr ret_exception_ptr_{nullptr};
  CoroReturnValueType return_type_{CoroReturnValueType::UNKNOWN};
  events::CoroTaskEvent coro_event_{};
};

struct PromiseBase {
 public:
  PromiseBase() = default;

  void CreateAndAddCoroEvent(std::coroutine_handle<void> handle) {
    parent_task_ptr_->coro_event_.SetCoroutineHandle(handle);
    is_this_promise_added_to_coro_tasks_ = true;
    GetLocalEventLoop().AddCoroutine(&parent_task_ptr_->coro_event_);
  }

  void TriggerCoroEvent() noexcept {
    if (is_this_promise_added_to_coro_tasks_) {
      GetLocalEventLoop().FinishCoroutine(
          parent_task_ptr_->coro_event_.GetCoroId());
    }
  }

  auto initial_suspend() noexcept { return std::suspend_always{}; }
  auto final_suspend() noexcept {
    TriggerCoroEvent();
    return std::suspend_never{};
  }

  void unhandled_exception() {
    parent_task_ptr_->ret_exception_ptr_ = std::current_exception();
    parent_task_ptr_->return_type_ = CoroReturnValueType::EXCEPTION;
  }

  detail::TaskBase* parent_task_ptr_{nullptr};
  bool is_this_promise_added_to_coro_tasks_{false};
};

}  // namespace detail

template <>
class Task<void> : public detail::TaskBase {
 public:
  struct promise_type : public detail::PromiseBase {
    void return_void() {
      if ((static_cast<Task<void>*>(parent_task_ptr_))->return_type_ ==
          detail::CoroReturnValueType::EXCEPTION) {
        return;
      }
      (static_cast<Task<void>*>(parent_task_ptr_))->return_type_ =
          detail::CoroReturnValueType::VALUE;
    }
    promise_type* get_return_object() noexcept { return this; }
  };

  Task(promise_type* p) noexcept {
    promise_ = p;
    p->parent_task_ptr_ = static_cast<detail::TaskBase*>(this);
  }

  ~Task() {
    if (ret_exception_ptr_) {
      ret_exception_ptr_.~exception_ptr();
      ret_exception_ptr_ = nullptr;
    }
  }

  void Result() {
    switch (return_type_) {
      [[unlikely]] case detail::CoroReturnValueType::EXCEPTION : {
        auto tmp_exception = CopyExceptionPtr();
        ret_exception_ptr_ = nullptr;
        std::rethrow_exception(tmp_exception);
      }
      break;
      default:
        break;
    }
    return;
  }

  // coro related
  [[nodiscard]] std::coroutine_handle<promise_type> await_suspend(
      std::coroutine_handle<void> parent_promise_handler) {
    promise_->CreateAndAddCoroEvent(parent_promise_handler);
    return std::coroutine_handle<promise_type>::from_promise(
        *(static_cast<promise_type*>(promise_)));
  }

  void await_resume() { Result(); }

  void Start() {
    auto handle = std::coroutine_handle<promise_type>::from_promise(
        *(static_cast<promise_type*>(promise_)));
    assert(!handle.done());
    handle.resume();
  }
};

template <arc::concepts::CopyableMoveableOrVoid T>
class Task : public detail::TaskBase {
 public:
  struct promise_type : public detail::PromiseBase {
    template <std::move_constructible U = T>
    void return_value(U&& value) {
      if ((static_cast<Task<T>*>(parent_task_ptr_))->return_type_ ==
          detail::CoroReturnValueType::EXCEPTION) {
        return;
      }
      (static_cast<Task<T>*>(parent_task_ptr_))->ret_ =
          new std::remove_reference_t<U>(std::move(std::forward<U&&>(value)));
      (static_cast<Task<T>*>(parent_task_ptr_))->return_type_ =
          arc::coro::detail::CoroReturnValueType::VALUE;
    }

    template <std::copy_constructible U = T>
    void return_value(U&& value) {
      if ((static_cast<Task<T>*>(parent_task_ptr_))->return_type_ ==
          detail::CoroReturnValueType::EXCEPTION) {
        return;
      }
      (static_cast<Task<T>*>(parent_task_ptr_))->ret_ =
          new std::remove_reference_t<U>(std::forward<U&&>(value));
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
    if (ret_exception_ptr_) {
      ret_exception_ptr_.~exception_ptr();
      ret_exception_ptr_ = nullptr;
    }
    if (ret_) {
      delete ret_;
      ret_ = nullptr;
    }
  }

  T Result() {
    switch (return_type_) {
      [[likely]] case detail::CoroReturnValueType::VALUE
          : return std::move(*((T*)ret_));
      [[unlikely]] case detail::CoroReturnValueType::EXCEPTION : {
        auto tmp_exception = CopyExceptionPtr();
        ret_exception_ptr_ = nullptr;
        std::rethrow_exception(tmp_exception);
      }
      break;
      default:
        // TODO change this
        throw std::logic_error("cannot be here");
    }
  }

  // coro related
  [[nodiscard]] std::coroutine_handle<promise_type> await_suspend(
      std::coroutine_handle<void> parent_promise_handler) {
    promise_->CreateAndAddCoroEvent(parent_promise_handler);
    return std::coroutine_handle<promise_type>::from_promise(
        *(static_cast<promise_type*>(promise_)));
  }

  T await_resume() { return Result(); }

 private:
  T* ret_{nullptr};
};

void EnsureFuture(Task<void> task) {
  task.Start();
}

void RunUntilComplelete() {
  auto& event_loop = GetLocalEventLoop();
  while (!event_loop.IsDone()) {
    event_loop.Do();
  }
}

void StartEventLoop(Task<void> task) {
  task.Start();
  RunUntilComplelete();
}

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__CORE__TASK_H */
