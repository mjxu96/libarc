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

#include <arc/exception/base.h>
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

namespace arc {
namespace coro {

// template <typename T>
// class DispatcherBase {
//  public:
//   DispatcherBase(std::size_t capacity, std::size_t
//   max_explicit_producer_count, std::size_t max_implicit_producer_count)
//       : queue_(capacity, max_explicit_producer_count,
//       max_implicit_producer_count) {}
//   virtual ~DispatcherBase() = default;

//   virtual bool Deque(T&& element) { return queue_.try_dequeue(element); }

//   virtual bool Enqueue(T&& element) { return queue_.try_enqueue(element); }

//  protected:
//   moodycamel::ConcurrentQueue<T> queue_;
// };

class CoroutineDispatcherQueue {
 public:
  CoroutineDispatcherQueue(int capacity, int max_explicit_producer_count,
                           int max_implicit_producer_count)
      : queue_(capacity, max_explicit_producer_count,
               max_implicit_producer_count) {}

  bool Enqueue(std::coroutine_handle<void>&& item) {
    return queue_.try_enqueue(item);
  }

 private:
  moodycamel::ConcurrentQueue<std::coroutine_handle<void>> queue_;
};

using CoroutineDispatcherConsumerIDType = int;

class CoroutineDispatcher {
 public:
  CoroutineDispatcher() = default;
  void EnqueueToAny(std::coroutine_handle<void>&& handle) {
    CoroutineDispatcherConsumerIDType next_consumer_id = GetNextConsumer();
    std::lock_guard guard(*per_queue_locks_[next_consumer_id]);
    queues_[next_consumer_id].push_back(std::move(handle));
  }

  void EnqueueToSpecific(CoroutineDispatcherConsumerIDType consumer_id,
                         std::coroutine_handle<void>&& handle) {
    std::lock_guard guard(*per_queue_locks_[consumer_id]);
    queues_[consumer_id].push_back(std::move(handle));
  }

  void EnqueueAllToAny(
      const std::vector<std::coroutine_handle<void>>& handles) {
    CoroutineDispatcherConsumerIDType next_consumer_id = GetNextConsumer();
    std::lock_guard guard(*per_queue_locks_[next_consumer_id]);
    queues_[next_consumer_id].insert(queues_[next_consumer_id].end(),
                                     handles.begin(), handles.end());
  }

  void EnqueueAllToSpecific(
      CoroutineDispatcherConsumerIDType consumer_id,
      const std::vector<std::coroutine_handle<void>>& handle);

  void SetRegistrationDone() {
    std::lock_guard guard(global_addition_lock_);
    is_register_done_ = true;
    consumer_id_itr_ = consumer_ids_.begin();
  }

  std::vector<std::coroutine_handle<void>>& RegisterConsumer(
      CoroutineDispatcherConsumerIDType consumer_id) {
    std::lock_guard guard(global_addition_lock_);
    if (is_register_done_) {
      throw arc::exception::detail::ExceptionBase(
          "Consumer registration is done. You cannot register any more "
          "consumers");
    }
    if (queues_.find(consumer_id) != queues_.end()) {
      throw arc::exception::detail::ExceptionBase(
          "Consumer id " + std::to_string(consumer_id) +
          " has already been registered.");
    }
    per_queue_locks_[consumer_id] = new std::mutex();
    queues_[consumer_id] = std::vector<std::coroutine_handle<void>>();
    queues_[consumer_id].reserve(kCoroutineQueueDefaultSize);
    consumer_ids_.push_back(consumer_id);
  }

  inline std::mutex& GetConsumerQueueLock(
      CoroutineDispatcherConsumerIDType consumer_id) {
    std::lock_guard guard(global_addition_lock_);
    return *per_queue_locks_[consumer_id];
  }

  constexpr static int kCoroutineQueueDefaultSize = 1024;

 private:
  CoroutineDispatcherConsumerIDType GetNextConsumer() {
    CoroutineDispatcherConsumerIDType next_consumer_id = *consumer_id_itr_;
    consumer_id_itr_++;
    if (consumer_id_itr_ == consumer_ids_.end()) {
      consumer_id_itr_ = consumer_ids_.begin();
    }
    return next_consumer_id;
  }

  std::mutex global_addition_lock_;
  bool is_register_done_{false};
  std::unordered_map<CoroutineDispatcherConsumerIDType, std::mutex*>
      per_queue_locks_;
  std::unordered_map<CoroutineDispatcherConsumerIDType,
                     std::vector<std::coroutine_handle<void>>>
      queues_;
  std::vector<CoroutineDispatcherConsumerIDType> consumer_ids_;
  std::vector<CoroutineDispatcherConsumerIDType>::iterator consumer_id_itr_;
};

// class CoroutineDispatcherConsumerToken {
//  public:
//   CoroutineDispatcherConsumerToken(CoroutineDispatcher& dispatcher,
//   CoroutineDispatcherConsumerIDType consumer_id);

//   friend class CoroutineDispatcher;
//  private:
//   moodycamel::ConsumerToken token_;
//   CoroutineDispatcherConsumerIDType consumer_id_{-1};
// };

// class CoroutineDispatcher {
//  public:
//   CoroutineDispatcher(std::size_t capacity, std::size_t
//   max_explicit_producer,
//                       std::size_t max_implicit_producer)
//       : queue_(capacity, max_explicit_producer, max_implicit_producer) {}

//   ~CoroutineDispatcher() = default;

//   bool Deque(std::coroutine_handle<void>& element) {
//     return queue_.try_dequeue(element);
//   }

//   bool Deque(std::coroutine_handle<void>& element,
//              CoroutineDispatcherConsumerToken& token) {
//     return queue_.try_dequeue(token.token_, element);
//   }

//   bool Enqueue(std::coroutine_handle<void>& element) {
//     return queue_.try_enqueue(element);
//   }

//   template <typename It>
//   bool EnqueueBulk(It it, int count) {
//     return this->queue_.try_enqueue_bulk(it, count);
//   }

//   template <typename It>
//   std::size_t DequeueBulk(It it, int count) {
//     return this->queue_.try_dequeue_bulk(it, count);
//   }

//   template <typename It>
//   std::size_t DequeueBulk(It it, int count, CoroutineDispatcherConsumerToken&
//   token) {
//     return this->queue_.try_dequeue_bulk(token.token_, it, count);
//   }

//   void RegisterAsConsumer(const CoroutineDispatcherConsumerToken& token) {
//     std::lock_guard guard(lock_);
//     if (specific_queues_.find(token.consumer_id_) != specific_queues_.end())
//     {
//       throw arc::exception::detail::ExceptionBase("Cannot register a consumer
//       twice");
//     }
//     specific_queues_[token.consumer_id_] = 0;
//   }

//   friend class CoroutineDispatcherConsumerToken;

//  private:
//   moodycamel::ConcurrentQueue<std::coroutine_handle<void>> queue_;
//   int producer_count_{0};

//   std::mutex lock_;
//   std::unordered_map<CoroutineDispatcherConsumerIDType, int>
//   specific_queues_;
// };

// template <typename T>
// class EventDispatcher : public DispatcherBase<T> {
//  public:
//   EventDispatcher(std::size_t capacity)
//       : DispatcherBase<T>(capacity, 1), token_(this->queue_) {}
//   virtual bool Deque(T&& element) override {
//     assert(is_produer_registered_);
//     return this->queue_.try_dequeue_from_producer(token_, element);
//   }

//   virtual bool Enqueue(T&& element) override {
//     assert(is_produer_registered_);
//     return this->queue_.try_enqueue(token_, element);
//   }

//   template <typename It>
//   bool EnqueueBulk(It it, int count) {
//     assert(is_produer_registered_);
//     return this->queue_.try_enqueue_bulk(token_, it, count);
//   }

//   template <typename It>
//   std::size_t DequeueBulk(It it, int count) {
//     assert(is_produer_registered_);
//     return this->queue_.try_dequeue_bulk_from_producer(token_, it, count);
//   }

//   void RegisterAsProducerEvent() {
//     assert(!is_produer_registered_);
//     is_produer_registered_ = true;
//   }

//  private:
//   moodycamel::ProducerToken token_;
//   bool is_produer_registered_{false};
// };

CoroutineDispatcher& GetGlobalCoroutineDispatcher();

}  // namespace coro
}  // namespace arc

#endif
