/*
 * File: dispatcher.h
 * Project: libarc
 * File Created: Monday, 19th April 2021 7:36:31 pm
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

#ifndef LIBARC__CORO__DISPATCHER_H
#define LIBARC__CORO__DISPATCHER_H

#include <arc/exception/io.h>
#include <arc/utils/data_structures/concurrentqueue.h>
#include <unistd.h>

#ifdef __clang__
#include <experimental/coroutine>
namespace std {
using experimental::coroutine_handle;
using experimental::noop_coroutine;
using experimental::suspend_always;
using experimental::suspend_never;
}  // namespace std
#else
#include <coroutine>
#endif
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace arc {
namespace coro {

#ifdef __linux__
using EventLoopWakeUpHandle = int;
#endif

class CoroutineConsumerToken {
 public:
  CoroutineConsumerToken(
      moodycamel::ConcurrentQueue<std::coroutine_handle<void>>& queue,
      EventLoopWakeUpHandle consumer_id)
      : token(queue), id(consumer_id) {}
  EventLoopWakeUpHandle id{-1};
  moodycamel::ConsumerToken token;
};

// MPSC queue
class CoroutineQueue {
 public:
  CoroutineQueue(int capacity, int max_explicit_producer_count,
                 int max_implicit_producer_count, EventLoopWakeUpHandle id)
      : queue_(capacity, max_explicit_producer_count,
               max_implicit_producer_count),
        token_(queue_, id) {}

  inline bool Enqueue(std::coroutine_handle<void>&& item) {
    bool ret = queue_.try_enqueue(item);
    if (ret) {
      remained_items_.fetch_add(1, std::memory_order::release);
    }
    return ret;
  }

  template <typename It>
  inline bool EnqueueBulk(It it, int count) {
    bool ret = queue_.try_enqueue_bulk(it, count);
    if (ret) {
      remained_items_.fetch_add(count, std::memory_order::release);
    }
    return ret;
  }

  inline bool Deque(std::coroutine_handle<void>&& item) {
    bool ret = queue_.try_dequeue(token_.token, item);
    if (ret) {
      remained_items_.fetch_add(-1, std::memory_order::release);
    }
    return ret;
  }

  template <typename It>
  inline std::size_t DequeBulk(It it, int count) {
    int ret = queue_.try_dequeue_bulk(token_.token, it, count);
    remained_items_.fetch_add(-ret, std::memory_order::release);
    return ret;
  }

  std::vector<std::coroutine_handle<void>> DequeAll(int count) {
    if (count <= 0) {
      return std::vector<std::coroutine_handle<void>>();
    }
    std::vector<std::coroutine_handle<void>> ret(count, 0);
    std::coroutine_handle<void>* now = &ret[0];
    int remained_size = count;
    while (remained_size > 0) {
      std::size_t this_size =
          queue_.try_dequeue_bulk(token_.token, now, remained_size);
      now += this_size;
      remained_size -= this_size;
    }
    remained_items_.fetch_add(-count, std::memory_order::release);
    return ret;
  }

  inline int GetRemainedItemsCount() {
    return remained_items_.load(std::memory_order::acquire);
  }

  friend class CoroutineConsumerToken;

 private:
  moodycamel::ConcurrentQueue<std::coroutine_handle<void>> queue_;
  CoroutineConsumerToken token_;
  std::atomic<int> remained_items_{0};
};

class CoroutineDispatcher {
 public:
  CoroutineDispatcher()
      : kMaxAllowedProducerCount_(std::thread::hardware_concurrency()) {
    queues_.reserve(kMaxInVecQueueCount_);
    for (int i = 0; i < kMaxInVecQueueCount_; i++) {
      queues_.push_back(nullptr);
    }
  }

  ~CoroutineDispatcher() {
    for (EventLoopWakeUpHandle id : consumer_ids_) {
      auto queue_ptr = GetCoroutineQueue(id);
      assert(queue_ptr != nullptr);
      delete queue_ptr;
    }
  }

  static CoroutineDispatcher& GetInstance();

  std::vector<EventLoopWakeUpHandle> GetAvailableDispatchDestinations() {
    std::lock_guard guard(global_addition_lock_);
    return consumer_ids_;
  }

  bool EnqueueToAny(std::coroutine_handle<void>&& handle) {
    std::lock_guard guard(global_addition_lock_);
    EventLoopWakeUpHandle next_consumer_id = GetNextConsumer();
    auto queue_ptr = GetCoroutineQueue(next_consumer_id);
    bool ret = queue_ptr->Enqueue(std::move(handle));
    if (ret) {
      NotifyEventLoop(next_consumer_id, 1);
    }
    return ret;
  }

  bool EnqueueToSpecific(EventLoopWakeUpHandle consumer_id,
                         std::coroutine_handle<void>&& handle) {
    std::lock_guard guard(global_addition_lock_);
    auto queue_ptr = GetCoroutineQueue(consumer_id);
    bool ret = queue_ptr->Enqueue(std::move(handle));
    if (ret) {
      NotifyEventLoop(consumer_id, 1);
    }
    return ret;
  }

  template <typename It>
  bool EnqueueAllToAny(It it, int count) {
    std::lock_guard guard(global_addition_lock_);
    return EnqueueAllToAnyNoLock(it, count);
  }

  template <typename It>
  bool EnqueueAllToSpecific(EventLoopWakeUpHandle consumer_id, It it,
                            int count) {
    std::lock_guard guard(global_addition_lock_);
    auto queue_ptr = GetCoroutineQueue(consumer_id);
    bool ret = queue_ptr->EnqueueBulk(it, count);
    if (ret) {
      NotifyEventLoop(consumer_id, count);
    }
    return ret;
  }

  CoroutineQueue* Register(EventLoopWakeUpHandle consumer_id) {
    std::lock_guard guard(global_addition_lock_);
    bool existed = consumer_id < kMaxInVecQueueCount_
                       ? queues_[consumer_id] != nullptr
                       : extra_queues_.find(consumer_id) != extra_queues_.end();
    if (existed) {
      throw arc::exception::detail::ExceptionBase(
          "Consumer id " + std::to_string(consumer_id) +
          " has already been registered.");
    }

    auto queue_ptr = new CoroutineQueue(kCoroutineQueueDefaultSize_, 0,
                                        kMaxAllowedProducerCount_, consumer_id);
    if (consumer_id < kMaxInVecQueueCount_) {
      queues_[consumer_id] = queue_ptr;
    } else {
      extra_queues_[consumer_id] = queue_ptr;
    }
    consumer_ids_.push_back(consumer_id);
    consumer_id_itr_ = consumer_ids_.begin();
    return queue_ptr;
  }

  void DeRegister(EventLoopWakeUpHandle consumer_id) {
    std::lock_guard guard(global_addition_lock_);
    bool existed = consumer_id < kMaxInVecQueueCount_
                       ? queues_[consumer_id] != nullptr
                       : extra_queues_.find(consumer_id) != extra_queues_.end();
    if (!existed) {
      throw arc::exception::detail::ExceptionBase(
          "Consumer id " + std::to_string(consumer_id) +
          " has not been registered before.");
    }

    auto queue_ptr = GetCoroutineQueue(consumer_id);
    auto coroutines = queue_ptr->DequeAll(queue_ptr->GetRemainedItemsCount());

    if (consumer_id < kMaxInVecQueueCount_) {
      delete queues_[consumer_id];
      queues_[consumer_id] = nullptr;
    } else {
      delete extra_queues_[consumer_id];
      extra_queues_.erase(consumer_id);
    }

    for (auto itr = consumer_ids_.begin(); itr != consumer_ids_.end(); itr++) {
      if (*itr == consumer_id) {
        consumer_id_itr_ = consumer_ids_.erase(itr);
        if (consumer_id_itr_ == consumer_ids_.end()) {
          consumer_id_itr_ = consumer_ids_.begin();
        }
        break;
      }
    }

    if (!consumer_ids_.empty() && !coroutines.empty()) {
      EnqueueAllToAnyNoLock(coroutines.begin(), coroutines.size());
    }
  }

 private:
  void NotifyEventLoop(EventLoopWakeUpHandle id, std::uint64_t count) {
    int wrote = write(id, &count, sizeof(count));
    if (wrote < 0) {
      throw arc::exception::IOException("Notify EventLoop Error");
    }
  }

  template <typename It>
  bool EnqueueAllToAnyNoLock(It it, int count) {
    EventLoopWakeUpHandle next_consumer_id = GetNextConsumer();
    auto queue_ptr = GetCoroutineQueue(next_consumer_id);
    bool ret = queue_ptr->EnqueueBulk(it, count);
    if (ret) {
      NotifyEventLoop(next_consumer_id, count);
    }
    return ret;
  }

  EventLoopWakeUpHandle GetNextConsumer() {
    if (consumer_ids_.empty()) {
      throw arc::exception::detail::ExceptionBase("No consumer available");
    }
    EventLoopWakeUpHandle next_consumer_id = *consumer_id_itr_;
    consumer_id_itr_++;
    if (consumer_id_itr_ == consumer_ids_.end()) {
      consumer_id_itr_ = consumer_ids_.begin();
    }
    return next_consumer_id;
  }

  CoroutineQueue* GetCoroutineQueue(EventLoopWakeUpHandle id) {
    if (id < kMaxInVecQueueCount_) [[likely]] {
      assert(queues_[id]);
      return queues_[id];
    }
    assert(extra_queues_[id]);
    return extra_queues_[id];
  }

  std::mutex global_addition_lock_;

  const int kMaxAllowedProducerCount_;

  constexpr static int kCoroutineQueueDefaultSize_ = 1024;
  constexpr static int kMaxInVecQueueCount_ = 512;
  std::vector<CoroutineQueue*> queues_;
  std::unordered_map<EventLoopWakeUpHandle, CoroutineQueue*> extra_queues_;

  std::vector<EventLoopWakeUpHandle> consumer_ids_;
  std::vector<EventLoopWakeUpHandle>::iterator consumer_id_itr_;
};

}  // namespace coro
}  // namespace arc

#endif
