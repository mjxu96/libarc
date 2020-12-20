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
class[[nodiscard]] Task;

namespace detail {

class TaskBase;
struct PromiseBase;

enum class CoroReturnValueType {
  UNKNOWN = 0U,
  VALUE = 1U,
  EXCEPTION = 2U,
};

struct CoroReturnPack {
  std::exception_ptr ret_exception_ptr{nullptr};
  CoroReturnValueType return_type{CoroReturnValueType::UNKNOWN};
};

class TaskBase {
 public:
  bool await_ready() { return false; }

  friend struct PromiseBase;

  std::exception_ptr CopyExceptionPtr(std::exception_ptr excp_ptr) {
    if (!excp_ptr) {
      return excp_ptr;
    }
    try {
      std::rethrow_exception(excp_ptr);
    } catch (const std::exception& e) {
      return std::current_exception();
    }
  }

  virtual void CleanUp() {
    if (this->ret_pack_->ret_exception_ptr) {
      this->ret_pack_->ret_exception_ptr.~exception_ptr();
      this->ret_pack_->ret_exception_ptr = nullptr;
    }
    if (ret_pack_) {
      delete ret_pack_;
      ret_pack_ = nullptr;
    }
  }

 public:
  PromiseBase* promise_{nullptr};
  CoroReturnPack* ret_pack_{nullptr};
};

struct PromiseBase {
 public:
  PromiseBase() { ret_pack_ = new CoroReturnPack{}; }

  void CleanUp() {
    if (this->ret_pack_->ret_exception_ptr) {
      this->ret_pack_->ret_exception_ptr.~exception_ptr();
      this->ret_pack_->ret_exception_ptr = nullptr;
    }
    if (ret_pack_) {
      delete ret_pack_;
      ret_pack_ = nullptr;
    }
  }

  void CreateAndAddCoroEvent(std::coroutine_handle<void> handle) {
    coro_event_ = new events::CoroTaskEvent{handle};
    is_this_promise_added_to_coro_tasks_ = true;
    GetLocalEventLoop().AddCoroutine(coro_event_);
  }

  void TriggerCoroEvent() noexcept {
    if (is_this_promise_added_to_coro_tasks_) {
      GetLocalEventLoop().FinishCoroutine(coro_event_->GetCoroId());
    }
  }

  auto initial_suspend() noexcept { return std::suspend_always{}; }
  auto final_suspend() noexcept {
    TriggerCoroEvent();
    return std::suspend_never{};
  }

  void unhandled_exception() {
    if (!is_this_promise_added_to_coro_tasks_) {
      CleanUp();
      std::rethrow_exception(std::current_exception());
    }
    ret_pack_->ret_exception_ptr = std::current_exception();
    ret_pack_->return_type = CoroReturnValueType::EXCEPTION;
  }

  CoroReturnPack* ret_pack_{nullptr};
  bool is_this_promise_added_to_coro_tasks_{false};
  events::CoroTaskEvent* coro_event_{nullptr};
};

}  // namespace detail

template <>
class[[nodiscard]] Task<void> : public detail::TaskBase {
 public:
  struct promise_type : public detail::PromiseBase {
    void return_void() {
      if (ret_pack_->return_type == detail::CoroReturnValueType::EXCEPTION) {
        return;
      }
      if (is_this_promise_added_to_coro_tasks_) {
        ret_pack_->return_type = detail::CoroReturnValueType::VALUE;
      } else {
        CleanUp();
      }
    }
    promise_type* get_return_object() noexcept { return this; }
  };

  Task(promise_type * p) noexcept {
    promise_ = p;
    this->ret_pack_ = promise_->ret_pack_;
  }

  void Result() {
    switch (this->ret_pack_->return_type) {
      [[unlikely]] case detail::CoroReturnValueType::EXCEPTION : {
        auto tmp_exception =
            CopyExceptionPtr(this->ret_pack_->ret_exception_ptr);
        this->ret_pack_->ret_exception_ptr = nullptr;
        CleanUp();
        std::rethrow_exception(tmp_exception);
      }
      break;
      default:
        break;
    }
    CleanUp();
    return;
  }

  // coro related
  [[nodiscard]] std::coroutine_handle<promise_type> await_suspend(
      std::coroutine_handle<void> parent_promise_handler) {
    promise_->CreateAndAddCoroEvent(parent_promise_handler);
    return std::coroutine_handle<promise_type>::from_promise(
        *(static_cast<promise_type*>(promise_)));
  }

  void
  await_resume() {
    Result();
  }

  void Start() {
    auto handle = std::coroutine_handle<promise_type>::from_promise(
        *(static_cast<promise_type*>(promise_)));
    assert(!handle.done());
    handle.resume();
  }
};

template <arc::concepts::CopyableMoveableOrVoid T>
class[[nodiscard]] Task : public detail::TaskBase {
 public:
  struct promise_type : public detail::PromiseBase {
    promise_type() : detail::PromiseBase() {
      ret_ = new T*;
      (*ret_) = nullptr;
    }

    template <std::move_constructible U = T>
    void return_value(U&& value) {
      if (this->ret_pack_->return_type ==
          detail::CoroReturnValueType::EXCEPTION) {
        return;
      }
      *(this->ret_) =
          new std::remove_reference_t<U>(std::move(std::forward<U&&>(value)));
      this->ret_pack_->return_type =
          arc::coro::detail::CoroReturnValueType::VALUE;
    }

    template <std::copy_constructible U = T>
    void return_value(U&& value) {
      if (this->ret_pack_->return_type ==
          detail::CoroReturnValueType::EXCEPTION) {
        return;
      }
      *(this->ret_) = new std::remove_reference_t<U>(std::forward<U&&>(value));
      this->ret_pack_->return_type =
          arc::coro::detail::CoroReturnValueType::VALUE;
    }

    promise_type* get_return_object() noexcept { return this; }

    friend class Task<T>;

   private:
    T** ret_{nullptr};
  };

  Task(promise_type * p) noexcept {
    promise_ = p;
    this->ret_pack_ = promise_->ret_pack_;
    ret_ = p->ret_;
  }

  void CleanUp() override {
    TaskBase::CleanUp();
    if (ret_) {
      if (*ret_) {
        delete (*ret_);
        (*ret_) = nullptr;
      }
      delete ret_;
      ret_ = nullptr;
    }
  }

  T Result() {
    switch (this->ret_pack_->return_type) {
      [[likely]] case detail::CoroReturnValueType::VALUE : {
        T ret_value = std::move(*(*(ret_)));
        CleanUp();
        return std::move(ret_value);
      }
      [[unlikely]] case detail::CoroReturnValueType::EXCEPTION : {
        auto tmp_exception =
            CopyExceptionPtr(this->ret_pack_->ret_exception_ptr);
        this->ret_pack_->ret_exception_ptr = nullptr;
        CleanUp();
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

  T await_resume() {
    return Result();
  }

 private:
  T** ret_{nullptr};
};

void EnsureFuture(Task<void> task) { task.Start(); }

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
