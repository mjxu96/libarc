/*
 * File: eventloop.c
 * Project: libarc
 * File Created: Monday, 7th December 2020 9:17:50 pm
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

#include <arc/coro/eventloop.h>
#include <arc/coro/task.h>
#include <arc/exception/io.h>

using namespace arc::coro;

inline EventLoopType operator|(EventLoopType a, EventLoopType b) {
    return static_cast<EventLoopType>(static_cast<int>(a) | static_cast<int>(b));
}

EventLoop::EventLoop() : poller_(&detail::GetLocalPoller()) {}

bool EventLoop::IsDone() {
  return poller_->IsPollerClean() && register_id_ == -1;
}

void EventLoop::InitDo() {
  is_running_ = true;
  Trim();
}

void EventLoop::Do() {
  // Then we will handle all others
  int todo_cnt = poller_->WaitEvents(todo_events_);

  for (int i = 0; i < todo_cnt; i++) {
    todo_events_[i]->Resume();
    delete todo_events_[i];
  }

  Trim();
}

void EventLoop::AddToCleanUpCoroutine(std::coroutine_handle<> handle) {
  to_clean_up_handles_.push_back(handle);
}

void EventLoop::CleanUpFinishedCoroutines() {
  for (auto& to_clean_up_handle : to_clean_up_handles_) {
    to_clean_up_handle.destroy();
  }
  to_clean_up_handles_.clear();
}

void EventLoop::Dispatch(arc::coro::Task<void>&& task) {
  task.SetNeedClean(true);
  to_dispatched_coroutines_.push_back(task.GetCoroutine());
}

void EventLoop::ResigerConsumer() {
  if (is_running_) {
    throw arc::exception::detail::ExceptionBase("Cannot register consumer after starting running");
  }
  event_loop_type_ = EventLoopType::CONSUMER | event_loop_type_;
  global_dispatcher_ = &GetGlobalCoroutineDispatcher();
  register_id_ = poller_->Register();
  global_dispatcher_->RegisterConsumer(register_id_);
  global_dispatcher_->WaitForRegistrationDone();
}

void EventLoop::ResigerProducer() {
  if (is_running_) {
    throw arc::exception::detail::ExceptionBase("Cannot register producer after starting running");
  }
  event_loop_type_ = EventLoopType::PRODUCER | event_loop_type_;
  global_dispatcher_ = &GetGlobalCoroutineDispatcher();
}

void EventLoop::Trim() {

  if ((EventLoopType::CONSUMER | event_loop_type_) == EventLoopType::CONSUMER) {
    ConsumeCoroutine();
  }
  if ((EventLoopType::PRODUCER | event_loop_type_) == EventLoopType::PRODUCER) {
    ProduceCoroutine();
  }

  poller_->TrimIOEvents();
  poller_->TrimTimeEvents();
  poller_->TrimUserEvents();

  CleanUpFinishedCoroutines();
}

void EventLoop::ConsumeCoroutine() {
  auto triggered_count = poller_->GetDispatchedCount();
  if (triggered_count == 0) {
    return;
  }
  auto coroutines = global_dispatcher_->DequeueAll(register_id_, triggered_count);
  for (auto& coro : coroutines) {
    coro.resume();
  }
}

void EventLoop::ProduceCoroutine() {
  if (to_dispatched_coroutines_.empty()) {
    return;
  }
  if (!global_dispatcher_->EnqueueAllToAny(to_dispatched_coroutines_.begin(),
                                       to_dispatched_coroutines_.size())) {
    auto itr = to_dispatched_coroutines_.begin();
    // TODO this might cause infinite loop, find a better way to yield
    while (itr != to_dispatched_coroutines_.end()) {
      if (global_dispatcher_->EnqueueToAny(std::move(*itr))) {
        itr = to_dispatched_coroutines_.erase(itr);
      }
    }
  } else {
    to_dispatched_coroutines_.clear();
  }
}

arc::coro::EventLoop& arc::coro::GetLocalEventLoop() {
  thread_local EventLoop loop{};
  return loop;
}
