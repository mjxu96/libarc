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

EventLoop::EventLoop() : poller_(&detail::GetLocalPoller()) {}

bool EventLoop::IsDone() {
  return (poller_->IsPollerClean()) &&
         (to_dispatched_coroutines_.empty() ||
          type_ != EventLoopType::PRODUCER);
}

void EventLoop::InitDo() {
  poller_->TrimIOEvents();
  poller_->TrimTimeEvents();
  poller_->TrimUserEvents();
}

void EventLoop::Do() {
  if (type_ == EventLoopType::CONSUMER)
    [[likely]] {
      ConsumeCoroutine();
    } else if (type_ == EventLoopType::PRODUCER)[[unlikely]] {
      ProduceCoroutine();
    }

  CleanUpFinishedCoroutines();

  if ((poller_->IsPollerClean())) {
    assert(to_clean_up_handles_.empty());
    while (!to_dispatched_coroutines_.empty() &&
           type_ != EventLoopType::PRODUCER) {
      ProduceCoroutine();
    }
    return;
  }

  // Then we will handle all others
  int todo_cnt = poller_->WaitEvents(todo_events_);

  for (int i = 0; i < todo_cnt; i++) {
    todo_events_[i]->Resume();
    delete todo_events_[i];
  }

  poller_->TrimIOEvents();
  poller_->TrimTimeEvents();
  poller_->TrimUserEvents();
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
  assert(type_ == EventLoopType::PRODUCER);
  task.SetNeedClean(true);
  to_dispatched_coroutines_.push_back(task.GetCoroutine());
}

void EventLoop::AsProducer() {
  assert(type_ == EventLoopType::NONE);
  type_ = EventLoopType::PRODUCER;
  global_dispatcher_ = &GetGlobalEventDispatcher<std::coroutine_handle<void>>();
  global_dispatcher_->RegisterAsProducerEvent();
}

void EventLoop::AsConsumer() {
  assert(type_ == EventLoopType::NONE);
  type_ = EventLoopType::CONSUMER;
  global_dispatcher_ = &GetGlobalEventDispatcher<std::coroutine_handle<void>>();
}

void EventLoop::ConsumeCoroutine() {
  std::coroutine_handle<void> coroutines[kMaxConsumableCoroutineNum_] = {0};
  int size =
      global_dispatcher_->DequeueBulk(coroutines, kMaxConsumableCoroutineNum_);
  for (int i = 0; i < size; i++) {
    coroutines[i].resume();
  }
}

void EventLoop::ProduceCoroutine() {
  if (to_dispatched_coroutines_.empty()) {
    return;
  }
  if (!global_dispatcher_->EnqueueBulk(to_dispatched_coroutines_.begin(),
                                       to_dispatched_coroutines_.size())) {
    auto itr = to_dispatched_coroutines_.begin();
    while (itr != to_dispatched_coroutines_.end()) {
      if (!global_dispatcher_->Enqueue(std::move(*itr))) {
        break;
      } else {
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
