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
#include <arc/coro/eventloop_group.h>
#include <arc/coro/task.h>
#include <arc/exception/io.h>

#include <iostream>

using namespace arc::coro;

inline EventLoopType operator|(EventLoopType a, EventLoopType b) {
  return static_cast<EventLoopType>(static_cast<int>(a) | static_cast<int>(b));
}

inline EventLoopType operator&(EventLoopType a, EventLoopType b) {
  return static_cast<EventLoopType>(static_cast<int>(a) & static_cast<int>(b));
}

inline EventLoopType operator~(EventLoopType a) {
  return static_cast<EventLoopType>(~static_cast<int>(a));
}

EventLoop::EventLoop() {
  poller_ = new Poller();
  id_ = EventLoopGroup::GetInstance().RegisterEventLoop(this);
}

EventLoop::~EventLoop() {
  DeResigerProducer();
  DeResigerConsumer();
  EventLoopGroup::GetInstance().DeRegisterEventLoop(id_);
  delete poller_;
}

bool EventLoop::IsDone() {
  return poller_->IsPollerDone() &&
         ((event_loop_type_ == EventLoopType::NONE) ||
          (((event_loop_type_ & EventLoopType::PRODUCER) ==
            EventLoopType::PRODUCER) &&
           to_dispatched_coroutines_count_ == 0));
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

EventLoop& EventLoop::GetLocalInstance() {
  thread_local EventLoop loop;
  return loop;
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
  to_randomly_dispatched_coroutines_.push_back(task.GetCoroutine());
  to_dispatched_coroutines_count_++;
}

void EventLoop::DispatchTo(arc::coro::Task<void>&& task,
                           EventLoopWakeUpHandle consumer_id) {
  task.SetNeedClean(true);
  if (to_dispatched_coroutines_with_dests_.find(consumer_id) ==
      to_dispatched_coroutines_with_dests_.end()) {
    to_dispatched_coroutines_with_dests_[consumer_id] =
        std::list<std::coroutine_handle<void>>();
  }
  to_dispatched_coroutines_with_dests_[consumer_id].push_back(
      task.GetCoroutine());
  to_dispatched_coroutines_count_++;
}

void EventLoop::ResigerConsumer() {
  event_loop_type_ = EventLoopType::CONSUMER | event_loop_type_;
  global_dispatcher_ = &CoroutineDispatcher::GetInstance();
  register_id_ = poller_->Register();
  dispatcher_queue_ = global_dispatcher_->Register(register_id_);
}

void EventLoop::DeResigerConsumer() {
  if ((event_loop_type_ & EventLoopType::CONSUMER) == EventLoopType::CONSUMER) {
    event_loop_type_ = (event_loop_type_ & (~EventLoopType::CONSUMER));
    ConsumeCoroutine();
    global_dispatcher_->DeRegister(register_id_);
    poller_->DeRegister();
    dispatcher_queue_ = nullptr;
    register_id_ = -1;
  }
}

void EventLoop::ResigerProducer() {
  event_loop_type_ = EventLoopType::PRODUCER | event_loop_type_;
  global_dispatcher_ = &CoroutineDispatcher::GetInstance();
}

void EventLoop::DeResigerProducer() {
  event_loop_type_ = (event_loop_type_ & (~EventLoopType::PRODUCER));
  while (to_dispatched_coroutines_count_ > 0) {
    ProduceCoroutine();
  }
}

void EventLoop::Trim() {
  if ((EventLoopType::CONSUMER & event_loop_type_) == EventLoopType::CONSUMER) {
    ConsumeCoroutine();
  }
  if ((EventLoopType::PRODUCER & event_loop_type_) == EventLoopType::PRODUCER) {
    ProduceCoroutine();
  }

  poller_->TrimIOEvents();
  poller_->TrimTimeEvents();
  poller_->TrimUserEvents();

  CleanUpFinishedCoroutines();

  if (to_dispatched_coroutines_count_ != 0) [[unlikely]] {
    poller_->SetNextTimeNoWait();
  }
}

void EventLoop::ConsumeCoroutine() {
  auto triggered_count = dispatcher_queue_->GetRemainedItemsCount();
  if (triggered_count == 0) {
    return;
  }
  auto coroutines = dispatcher_queue_->DequeAll(triggered_count);
  for (auto& coro : coroutines) {
    coro.resume();
  }
}

void EventLoop::ProduceCoroutine() {
  if (to_dispatched_coroutines_count_ == 0) {
    return;
  }
  if (!to_randomly_dispatched_coroutines_.empty()) {
    if (!global_dispatcher_->EnqueueAllToAny(
            to_randomly_dispatched_coroutines_.begin(),
            to_randomly_dispatched_coroutines_.size())) {
      auto itr = to_randomly_dispatched_coroutines_.begin();
      while (itr != to_randomly_dispatched_coroutines_.end()) {
        if (global_dispatcher_->EnqueueToAny(std::move(*itr))) {
          itr = to_randomly_dispatched_coroutines_.erase(itr);
          to_dispatched_coroutines_count_--;
        } else {
          break;
        }
      }
    } else {
      to_dispatched_coroutines_count_ -=
          to_randomly_dispatched_coroutines_.size();
      to_randomly_dispatched_coroutines_.clear();
    }
  }
  auto map_itr = to_dispatched_coroutines_with_dests_.begin();
  while (map_itr != to_dispatched_coroutines_with_dests_.end()) {
    assert(!map_itr->second.empty());
    if (!global_dispatcher_->EnqueueAllToSpecific(
            map_itr->first, map_itr->second.begin(), map_itr->second.size())) {
      auto itr = map_itr->second.begin();
      while (itr != map_itr->second.end()) {
        if (global_dispatcher_->EnqueueToAny(std::move(*itr))) {
          itr = map_itr->second.erase(itr);
          to_dispatched_coroutines_count_--;
        } else {
          break;
        }
      }
    } else {
      to_dispatched_coroutines_count_ -= map_itr->second.size();
      map_itr->second.clear();
    }

    if (map_itr->second.empty()) {
      map_itr = to_dispatched_coroutines_with_dests_.erase(map_itr);
    } else {
      map_itr++;
    }
  }

  return;
}
