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

#include <arc/utils/data_structures/concurrentqueue.h>

namespace arc {
namespace coro {

template <typename T>
class DispatcherBase {
 public:
  DispatcherBase(std::size_t capacity, std::size_t producer_count)
      : queue_(capacity, producer_count, 0), producer_count_(producer_count) {}
  virtual ~DispatcherBase() = default;

  virtual bool Deque(T&& element) { return queue_.try_dequeue(element); }

  virtual bool Enqueue(T&& element) { return queue_.try_enqueue(element); }

 protected:
  moodycamel::ConcurrentQueue<T> queue_;
  const int producer_count_{0};
};

template <typename T>
class EventDispatcher : public DispatcherBase<T> {
 public:
  EventDispatcher(std::size_t capacity)
      : DispatcherBase<T>(capacity, 1), token_(this->queue_) {}
  virtual bool Deque(T&& element) override {
    assert(is_produer_registered_);
    return this->queue_.try_dequeue_from_producer(token_, element);
  }

  virtual bool Enqueue(T&& element) override {
    assert(is_produer_registered_);
    return this->queue_.try_enqueue(token_, element);
  }

  template <typename It>
  bool EnqueueBulk(It it, int count) {
    assert(is_produer_registered_);
    return this->queue_.try_enqueue_bulk(token_, it, count);
  }

  template <typename It>
  std::size_t DequeueBulk(It it, int count) {
    assert(is_produer_registered_);
    return this->queue_.try_dequeue_bulk_from_producer(token_, it, count);
  }

  void RegisterAsProducerEvent() {
    assert(!is_produer_registered_);
    is_produer_registered_ = true;
  }

 private:
  moodycamel::ProducerToken token_;
  bool is_produer_registered_{false};
};

template <typename T>
EventDispatcher<T>& GetGlobalEventDispatcher();

}  // namespace coro
}  // namespace arc

#endif