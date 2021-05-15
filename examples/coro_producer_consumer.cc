/*
 * File: coro_producer_consumer.cc
 * Project: libarc
 * File Created: Saturday, 15th May 2021 9:17:58 pm
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

#include <iostream>
#include <arc/coro/locks/condition.h>
#include <arc/coro/task.h>

#include <queue>

using namespace arc::coro;

int total_num = 10000;
int cons_num = 20;
int cons_num_per_thread = 4;

template <typename T>
class CoroQueue {
 public:
  CoroQueue() = default;

  Task<T> Pop() {
    co_await lock_.Acquire();
    // std::cout << "pop acquired lock" << std::endl;
    while (q_.empty()) {
      // std::cout << "pop before wait" << std::endl;
      co_await cond_.Wait(lock_);
      // std::cout << "exit wait in pop" << std::endl;
    }
    auto val = q_.front();
    q_.pop();
    cond_.NotifyOne();
    lock_.Release();
    // std::cout << "pop released lock" << std::endl;
    co_return val;
  }

  Task<void> Push(const T& item) {
    co_await lock_.Acquire();
    // std::cout << "push acquired lock" << std::endl;
    while (q_.size() >= kBufferSize_) {
      // std::cout << "push before wait" << std::endl;
      co_await cond_.Wait(lock_);
      // std::cout << "exit wait in push" << std::endl;
    }
    q_.push(item);
    cond_.NotifyOne();
    lock_.Release();
    // std::cout << "push released lock" << std::endl;
  }

 private:
  Lock lock_;
  Condition cond_;
  std::queue<T> q_;
  constexpr static int kBufferSize_ = 100;
};

Task<void> ProduceCoro(CoroQueue<int>* q) {
  for (int i = 0; i < total_num; i++) {
    // std::cout << " pushing " << i << std::endl;
    co_await q->Push(i);
    // std::cout << " pushed " << i << std::endl;
  }
}

Task<void> ConsumeCoro(CoroQueue<int>* q, int id) {
  for (int i = 0; i < (total_num / cons_num); i++) {
    // std::cout << id << " poping " << std::endl;
    auto item = co_await q->Pop();
    // std::cout << id << " popped " << item << std::endl;
  }
}

void Produce(CoroQueue<int>* q) { StartEventLoop(ProduceCoro(q)); }

void Consume(CoroQueue<int>* q, int id_start) {
  for (int i = id_start; i < id_start + cons_num_per_thread; i++) {
    EnsureFuture(ConsumeCoro(q, i));
  }
  RunUntilComplelete();
}

CoroQueue<int> coro_queue;

int main() {
  std::thread prod(std::bind(Produce, &coro_queue));

  std::vector<std::thread> cons_threads;
  for (int i = 0; i < cons_num; i+=cons_num_per_thread) {
    cons_threads.emplace_back(std::bind(Consume, &coro_queue, i));
  }
  // std::cout << cons_threads.size() << std::endl;

  prod.join();
  for (int i = 0; i < (cons_num / cons_num_per_thread); i++) {
    cons_threads[i].join();
  }
}
