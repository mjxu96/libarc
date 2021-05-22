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

#include <unordered_map>
#include <variant>

namespace arc {
namespace coro {

namespace detail {

class ConditionCore {
 public:
  ConditionCore() = default;
  ~ConditionCore() = default;

  void Register(ConditionEvent* event, EventLoop* event_loop) {
    std::lock_guard guard(lock_);
    event_loop->AddUserEvent(event);
    EventIDEventLoopPair pair = {.event_id = event->GetEventID(),
                                 .event_loop = event_loop};
    pending_events_.push(pair);
    if (pending_events_pairs_.find(event_loop) == pending_events_pairs_.end()) {
      pending_events_pairs_[event_loop] = std::unordered_set<EventID>();
    }
    pending_events_pairs_[event_loop].insert(event->GetEventID());
  }

  void DeRegister(ConditionEvent* event, EventLoop* event_loop) {
    EventID event_id = event->GetEventID();
    pending_events_pairs_[event_loop].erase(event_id);
    if (pending_events_pairs_[event_loop].empty()) {
      pending_events_pairs_.erase(event_loop);
    }
  }

  void TriggerOne() {
    std::lock_guard guard(lock_);
    if (pending_events_.empty()) {
      return;
    }
    TriggerInternal();
  }

  void TriggerAll() {
    std::lock_guard guard(lock_);
    while (!pending_events_.empty()) {
      TriggerInternal();
    }
  }

 private:
  void TriggerInternal() {
    while (!pending_events_.empty()) {
      auto pending_event_pair = pending_events_.front();
      pending_events_.pop();
      auto pending_events_pairs_itr =
          pending_events_pairs_.find(pending_event_pair.event_loop);
      if (pending_events_pairs_itr == pending_events_pairs_.end()) {
        continue;
      }
      auto event_id_itr =
          pending_events_pairs_itr->second.find(pending_event_pair.event_id);
      if (event_id_itr == pending_events_pairs_itr->second.end()) {
        continue;
      }
      EventID event_id = pending_event_pair.event_id;
      EventLoop* loop = pending_event_pair.event_loop;
      pending_events_pairs_itr->second.erase(event_id_itr);
      if (pending_events_pairs_itr->second.empty()) {
        pending_events_pairs_.erase(pending_events_pairs_itr);
      }
      if (!loop->TriggerUserEvent(event_id)) {
        continue;
      }
      break;
    }
  }

  std::mutex lock_;
  std::queue<EventIDEventLoopPair> pending_events_;
  // TODO will cause double free if directly use EventIDEventLoopPair
  // investigate it
  std::unordered_map<EventLoop*, std::unordered_set<EventID>>
      pending_events_pairs_;
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
        abort_handle_(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                (std::chrono::steady_clock::now() + timeout).time_since_epoch())
                .count()) {}

  bool await_ready() { return false; }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    condition_event_ = new coro::ConditionEvent(handle);
    event_loop_ = &GetLocalEventLoop();
    core_->Register(condition_event_, event_loop_);
    if (abort_handle_.index() == 1) {
      auto cancellation_event = new coro::CancellationEvent(condition_event_);
      std::get<1>(abort_handle_)
          ->SetEventAndLoop(cancellation_event, event_loop_);
    } else if (abort_handle_.index() == 2) {
      auto timeout_event =
          new coro::TimeoutEvent(std::get<2>(abort_handle_), condition_event_);
      event_loop_->AddBoundEvent(timeout_event);
    }
    if (lock_core_) {
      lock_core_->Unlock();
    }
  }

  void await_resume() {
    if (condition_event_->IsInterrupted()) [[unlikely]] {
      core_->DeRegister(condition_event_, event_loop_);
    }
  }

 private:
  arc::coro::detail::ConditionCore* core_{nullptr};
  arc::coro::detail::LockCore* lock_core_{nullptr};

  coro::ConditionEvent* condition_event_{nullptr};
  EventLoop* event_loop_{nullptr};

  std::variant<std::monostate, std::shared_ptr<CancellationToken>, std::int64_t>
      abort_handle_;
};

}  // namespace coro
}  // namespace arc

#endif
