/*
 * File: cancellation_token.h
 * Project: libarc
 * File Created: Wednesday, 19th May 2021 9:16:06 pm
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

#ifndef LIBARC__CORO__UTILS_CANCELLATION_TOKEN_H
#define LIBARC__CORO__UTILS_CANCELLATION_TOKEN_H

#include <arc/coro/eventloop.h>
#include <arc/coro/events/cancellation_event.h>

#include <memory>
#include <tuple>

namespace arc {
namespace coro {

namespace detail {

class CancellationTokenCore {
 public:
  CancellationTokenCore() {}

  ~CancellationTokenCore() {
    // TODO add global event loop group
    // Cancel();
  }

  void SetEventAndLoop(BoundEvent* event, EventLoop* loop) {
    std::lock_guard guard(lock_);
    registered_events_pairs_.push_back({event->GetBountEventID(), event, loop});
    loop->AddBoundEvent(event);
  }

  void Cancel() {
    std::lock_guard guard(lock_);
    TriggerCancel();
    registered_events_pairs_.clear();
  }

 private:
  void TriggerCancel() {
    for (auto [bind_event_id, event, loop] : registered_events_pairs_) {
      loop->TriggerBoundEvent(bind_event_id, event);
    }
  }

  std::mutex lock_;

  // vector of {bound_event_id, trigger_event_pair}
  std::vector<std::tuple<EventID, BoundEvent*, EventLoop*>>
      registered_events_pairs_;
};

}  // namespace detail

class CancellationToken {
 public:
  CancellationToken()
      : core_(std::make_shared<detail::CancellationTokenCore>()) {}

  void SetEventAndLoop(BoundEvent* event, EventLoop* loop) {
    core_->SetEventAndLoop(event, loop);
  }

  void Cancel() { core_->Cancel(); }

 private:
  std::shared_ptr<detail::CancellationTokenCore> core_{nullptr};
};

}  // namespace coro
}  // namespace arc

#endif