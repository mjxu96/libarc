/*
 * File: condition_awaiter.h
 * Project: libarc
 * File Created: Friday, 14th May 2021 9:12:02 pm
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

#ifndef LIBARC__CORO__AWAITER__CONDITION_AWAITER_H
#define LIBARC__CORO__AWAITER__CONDITION_AWAITER_H

#include <arc/coro/awaiter/lock_awaiter.h>
#include <arc/coro/eventloop.h>
#include <arc/coro/events/condition_event.h>
#include <arc/coro/utils/cancellation_token.h>

#include <variant>

namespace arc {
namespace coro {

namespace detail {

class ConditionCore {
 public:
  ConditionCore() = default;
  ~ConditionCore() = default;

  void Register(coro::ConditionEvent* event, EventLoop* event_loop) {
    std::lock_guard guard(lock_);
    pending_events_pairs_.emplace(event, event_loop);
    event_loop->AddUserEvent(event);
  }

  void TriggerOne() {
    std::lock_guard guard(lock_);
    if (pending_events_pairs_.empty()) {
      return;
    }
    if (write(TriggerInternal(), &i_, sizeof(i_)) != sizeof(i_)) {
      throw arc::exception::IOException("NotifyOne Error");
    }
  }

  void TriggerAll() {
    std::lock_guard guard(lock_);
    std::unordered_set<int> to_notified_event_ids;
    while (!pending_events_pairs_.empty()) {
      to_notified_event_ids.insert(TriggerInternal());
    }
    for (int to_notified_event_id : to_notified_event_ids) {
      if (write(to_notified_event_id, &i_, sizeof(i_)) != sizeof(i_)) {
        throw arc::exception::IOException("NotifyOne Error");
      }
    }
  }

 private:
  coro::EventHandleType TriggerInternal() {
    auto& pending_event_pair = pending_events_pairs_.front();
    coro::EventHandleType event_handle =
        pending_event_pair.second->GetEventHandle();
    pending_event_pair.second->TriggerUserEvent(pending_event_pair.first);
    pending_events_pairs_.pop();
    return event_handle;
  }

  std::mutex lock_;
  std::queue<std::pair<coro::ConditionEvent*, EventLoop*>>
      pending_events_pairs_;
  std::uint64_t i_{1};
  static_assert(sizeof(i_) == 8U);
};

}  // namespace detail

class [[nodiscard]] ConditionAwaiter {
 public:
  ConditionAwaiter(arc::coro::detail::ConditionCore* core,
                   arc::coro::detail::LockCore* lock_core)
      : core_(core), lock_core_(lock_core) {}

  ConditionAwaiter(arc::coro::detail::ConditionCore* core,
                   arc::coro::detail::LockCore* lock_core,
                   const CancellationToken& token)
      : core_(core),
        lock_core_(lock_core),
        abort_handle_(std::make_shared<CancellationToken>(token)) {}

  ConditionAwaiter(arc::coro::detail::ConditionCore* core,
                   arc::coro::detail::LockCore* lock_core,
                   const std::chrono::steady_clock::duration& timeout)
      : core_(core),
        lock_core_(lock_core),
        abort_handle_(std::chrono::duration_cast<std::chrono::milliseconds>(
                          (std::chrono::steady_clock::now() + timeout)
                              .time_since_epoch())
                          .count()) {}

  bool await_ready() { return false; }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    auto event = new coro::ConditionEvent(handle);
    EventLoop* event_loop = &GetLocalEventLoop();
    core_->Register(event, event_loop);
    if (abort_handle_.index() == 1) {
      auto cancellation_event = new coro::CancellationEvent(event);
      std::get<1>(abort_handle_)
          ->SetEventAndLoop(cancellation_event, event_loop);
    } else if (abort_handle_.index() == 2) {
      auto timeout_event =
          new coro::TimeoutEvent(std::get<2>(abort_handle_), event);
      event_loop->AddBoundEvent(timeout_event);
    }
    if (lock_core_) {
      lock_core_->Unlock();
    }
  }

  void await_resume() {}

 private:
  arc::coro::detail::ConditionCore* core_{nullptr};
  arc::coro::detail::LockCore* lock_core_{nullptr};

  std::variant<std::monostate, std::shared_ptr<CancellationToken>, std::int64_t>
      abort_handle_;
};

}  // namespace coro
}  // namespace arc

#endif
