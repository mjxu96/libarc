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
#include <unistd.h>
#include <arc/utils/data_structures/concurrentqueue.h>

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
#include <mutex>
#include <unordered_map>
#include <vector>
#include <condition_variable>
#include <atomic>

namespace arc {
namespace coro {

#ifdef __linux__
using CoroutineDispatcherRegisterIDType = int;
#endif

class CoroutineDispatcherConsumerToken {
 public:
  CoroutineDispatcherConsumerToken(moodycamel::ConcurrentQueue<std::coroutine_handle<void>>& queue,
        CoroutineDispatcherRegisterIDType consumer_id)
   : token(queue), id(consumer_id) {}
  CoroutineDispatcherRegisterIDType id{-1};
  moodycamel::ConsumerToken token;
};

// MPSC queue
class CoroutineDispatcherQueue {
 public:
  CoroutineDispatcherQueue(int capacity, int max_explicit_producer_count,
                           int max_implicit_producer_count, CoroutineDispatcherRegisterIDType id)
      : queue_(capacity, max_explicit_producer_count,
               max_implicit_producer_count), token_(queue_, id) {}
  
  inline bool Enqueue(std::coroutine_handle<void>&& item) {
    return queue_.try_enqueue(item);
    // if (ret) {
    //   remained_items_.fetch_add(1, std::memory_order::release);
    // }
    // return ret;
  }

  template<typename It>
  inline bool EnqueueBulk(It it, int count) {
    return queue_.try_enqueue_bulk(it, count);
    // if (ret) {
    //   remained_items_.fetch_add(count, std::memory_order::release);
    // }
    // return ret;
  }

  inline bool Deque(std::coroutine_handle<void>&& item) {
    return queue_.try_dequeue(token_.token, item);
    // if (ret) {
    //   remained_items_.fetch_add(-1, std::memory_order::release);
    // }
    // return ret;
  }

  template<typename It>
  inline std::size_t DequeBulk(It it, int count) {
    return queue_.try_dequeue_bulk(token_.token, it, count);
    // remained_items_.fetch_add(-ret, std::memory_order::release);
  }

  // inline int GetRemainedItemsCount() {
  //   return remained_items_.load(std::memory_order::acquire);
  // }

  friend class CoroutineDispatcherConsumerToken;
 private:
  moodycamel::ConcurrentQueue<std::coroutine_handle<void>> queue_;
  CoroutineDispatcherConsumerToken token_;
  // std::atomic<int> remained_items_{0};
};


class CoroutineDispatcher {
 public:
  CoroutineDispatcher() {
    queues_.reserve(kMaxInVecQueueCount_);
    for (int i = 0; i < kMaxInVecQueueCount_; i++) {
      queues_.push_back(nullptr);
    }
  }
  ~CoroutineDispatcher() {
    for (CoroutineDispatcherRegisterIDType id : consumer_ids_) {
      auto queue_ptr = GetCoroutineQueue(id);
      assert(queue_ptr != nullptr);
      delete queue_ptr;
    }
  }

  bool EnqueueToAny(std::coroutine_handle<void>&& handle) {
    CoroutineDispatcherRegisterIDType next_consumer_id = GetNextConsumer();
    auto queue_ptr = GetCoroutineQueue(next_consumer_id);
    bool ret = queue_ptr->Enqueue(std::move(handle));
    if (ret) {
      NotifyEventLoop(next_consumer_id, 1);
    }
    return ret;
  }

  bool EnqueueToSpecific(CoroutineDispatcherRegisterIDType consumer_id,
                         std::coroutine_handle<void>&& handle) {
    auto queue_ptr = GetCoroutineQueue(consumer_id);
    bool ret = queue_ptr->Enqueue(std::move(handle));
    if (ret) {
      NotifyEventLoop(consumer_id, 1);
    }
    return ret;
  }

  template<typename It>
  bool EnqueueAllToAny(It it, int count) {
    CoroutineDispatcherRegisterIDType next_consumer_id = GetNextConsumer();
    auto queue_ptr = GetCoroutineQueue(next_consumer_id);
    bool ret = queue_ptr->EnqueueBulk(it, count);
    if (ret) {
      NotifyEventLoop(next_consumer_id, count);
    }
    return ret;
  }

  template<typename It>
  bool EnqueueAllToSpecific(CoroutineDispatcherRegisterIDType consumer_id, It it, int count) {
    auto queue_ptr = GetCoroutineQueue(consumer_id);
    bool ret = queue_ptr->EnqueueBulk(it, count);
    if (ret) {
      NotifyEventLoop(consumer_id, count);
    }
    return ret;
  }

  std::vector<std::coroutine_handle<void>> DequeueAll(CoroutineDispatcherRegisterIDType consumer_id, int remained_size) {
    std::vector<std::coroutine_handle<void>> ret;
    ret.reserve(remained_size);
    auto queue_ptr = GetCoroutineQueue(consumer_id);

    std::unique_ptr<std::coroutine_handle<void>[]> data(new std::coroutine_handle<void>[remained_size]);
    auto data_ptr = data.get();
    while (remained_size > 0) {
      auto this_size = queue_ptr->DequeBulk(data_ptr, remained_size);
      for (int i = 0; i < this_size; i++) {
        ret.push_back(*data_ptr);
        data_ptr ++;
      }
      remained_size -= this_size;
    }
    return ret;
  }

  void SetRegistrationDone() {
    std::lock_guard guard(global_addition_lock_);
    is_registration_done_ = true;
    consumer_id_itr_ = consumer_ids_.begin();
    for (CoroutineDispatcherRegisterIDType id : consumer_ids_) {
      if (id < kMaxInVecQueueCount_) {
        queues_[id] = new CoroutineDispatcherQueue(kMaxInVecQueueCount_, 0, consumer_ids_.size(), id);
      } else {
        extra_queues_[id] = new CoroutineDispatcherQueue(kMaxInVecQueueCount_, 0, consumer_ids_.size(), id);
      }
    }
    wait_cond_.notify_all();
  }

  void WaitForRegistrationDone() {
    std::unique_lock ulock(wait_lock_);
    wait_cond_.wait(ulock);
  }

  void RegisterConsumer(
      CoroutineDispatcherRegisterIDType consumer_id) {
    std::lock_guard guard(global_addition_lock_);
    if (is_registration_done_) {
      throw arc::exception::detail::ExceptionBase(
          "Consumer registration is done. You cannot register any more "
          "consumers");
    }
    bool existed = consumer_id < kMaxInVecQueueCount_ ? queues_[consumer_id] != nullptr : extra_queues_.find(consumer_id) != extra_queues_.end();
    if (existed) {
      throw arc::exception::detail::ExceptionBase(
          "Consumer id " + std::to_string(consumer_id) +
          " has already been registered.");
    }
    consumer_ids_.push_back(consumer_id);
  }

 private:
  void NotifyEventLoop(CoroutineDispatcherRegisterIDType id, std::uint64_t count) {
    int wrote = write(id, &count, sizeof(count));
    if (wrote < 0) {
      throw arc::exception::IOException("Notify EventLoop Error");
    }
  }

  CoroutineDispatcherRegisterIDType GetNextConsumer() {
    CoroutineDispatcherRegisterIDType next_consumer_id = *consumer_id_itr_;
    consumer_id_itr_++;
    if (consumer_id_itr_ == consumer_ids_.end()) {
      consumer_id_itr_ = consumer_ids_.begin();
    }
    return next_consumer_id;
  }

  CoroutineDispatcherQueue* GetCoroutineQueue(CoroutineDispatcherRegisterIDType id) {
    if (id < kMaxInVecQueueCount_) [[likely]] {
      assert(queues_[id] != nullptr);
      return queues_[id];
    }
    assert(extra_queues_[id] != nullptr);
    return extra_queues_[id];
  }

  std::mutex global_addition_lock_;
  bool is_registration_done_{false};

  constexpr static int kCoroutineQueueDefaultSize_ = 1024;
  constexpr static int kMaxInVecQueueCount_ = 512;
  std::vector<CoroutineDispatcherQueue*> queues_;
  std::unordered_map<CoroutineDispatcherRegisterIDType, CoroutineDispatcherQueue*> extra_queues_;

  std::vector<CoroutineDispatcherRegisterIDType> consumer_ids_;
  std::vector<CoroutineDispatcherRegisterIDType>::iterator consumer_id_itr_;

  std::mutex wait_lock_;
  std::condition_variable wait_cond_;
};

CoroutineDispatcher& GetGlobalCoroutineDispatcher();

}  // namespace coro
}  // namespace arc

#endif
