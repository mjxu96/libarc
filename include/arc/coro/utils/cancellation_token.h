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

namespace arc {
namespace coro {

namespace detail {

class CancellationTokenCore {
 public:
  CancellationTokenCore() {}

  ~CancellationTokenCore() {
    std::lock_guard guard(lock_);
    if (event_) {
      TriggerCancel();
    }
  }

  void SetEventAndLoop(CancellationEvent* event, EventLoop* loop) {
    std::lock_guard guard(lock_);
    event_ = event;
    bind_event_id_ = event->GetBindEventID();
    loop_ = loop;
    loop_->AddCancellationEvent(event);
  }

  void Cancel() {
    std::lock_guard guard(lock_);
    if (!event_) {
      return;
    }
    if (TriggerCancel() < sizeof(std::uint64_t)) {
      throw arc::exception::IOException("Cancel token write error");
    }
    event_ = nullptr;
  }

 private:
  int TriggerCancel() {
    loop_->TriggerCancellationEvent(bind_event_id_, event_);
    std::uint64_t event_read = 1;
    return write(loop_->GetEventHandle(), &event_read, sizeof(event_read));
  }

  std::mutex lock_;
  CancellationEvent* event_{nullptr};
  int bind_event_id_{-1};
  EventLoop* loop_{nullptr};
};

}  // namespace detail

class CancellationToken {
 public:
  CancellationToken() : core_(std::make_shared<detail::CancellationTokenCore>()) {}

  void SetEventAndLoop(CancellationEvent* event, EventLoop* loop) {
    core_->SetEventAndLoop(event, loop);
  }

  // const bool IsValid() const { return core_ != nullptr; }

  void Cancel() { 
    core_->Cancel(); }

 private:
  std::shared_ptr<detail::CancellationTokenCore> core_{nullptr};
};

}  // namespace coro
}  // namespace arc

#endif