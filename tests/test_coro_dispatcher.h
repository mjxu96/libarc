/*
 * File: test_coro_dispatcher.h
 * Project: libarc
 * File Created: Monday, 17th May 2021 6:55:09 pm
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

#ifndef LIBARC__TESTS__TEST_CORO_DISPATCHER_H
#define LIBARC__TESTS__TEST_CORO_DISPATCHER_H

#include <arc/coro/dispatcher.h>
#include <arc/coro/locks/condition.h>
#include <arc/coro/task.h>
#include "utils.h"

#include <gtest/gtest.h>

namespace arc {
namespace test {

class DispatcherCoroTest : public ::testing::Test {
 protected:
  coro::Lock lock_;
  coro::Condition cond_;
  coro::Condition consumer_prepare_cond_;
  int finished_produce_count_{0};
  int prepared_consumer_count_{0};

 public:
  coro::Task<void> DispatchedTask() {
    co_await coro::SleepFor(std::chrono::milliseconds(1));
    int& counter = GetThreadLocalCounter();
    counter++;
  }

  coro::Task<void> LongRunTask(int producer_count) {
    co_await lock_.Acquire();
    coro::GetLocalEventLoop().ResigerConsumer();
    prepared_consumer_count_++;
    consumer_prepare_cond_.NotifyAll();
    lock_.Release();

    co_await lock_.Acquire();
    while (finished_produce_count_ < producer_count) {
      co_await cond_.Wait(lock_);
    }
    coro::GetLocalEventLoop().DeResigerConsumer();
    lock_.Release();
  }

  coro::Task<void> ProduceTask(int produce_count, int consumer_count,
                               coro::EventLoopWakeUpHandle target) {
    coro::GetLocalEventLoop().ResigerProducer();
    co_await lock_.Acquire();
    while (prepared_consumer_count_ < consumer_count) {
      co_await consumer_prepare_cond_.Wait(lock_);
    }
    lock_.Release();
    for (int i = 0; i < produce_count; i++) {
      if (target == -1) {
        coro::GetLocalEventLoop().Dispatch(DispatchedTask());
        // yield temporarily to avoid dispatch all tasks to one
        co_await coro::Yield();
      } else {
        coro::GetLocalEventLoop().DispatchTo(DispatchedTask(), target);
      }
    }
    coro::GetLocalEventLoop().DeResigerProducer();
    co_await lock_.Acquire();
    finished_produce_count_++;
    cond_.NotifyAll();
    lock_.Release();
  }

  void StartLongRunTask(int expected_count, int produce_count) {
    coro::StartEventLoop(LongRunTask(produce_count));
    EXPECT_EQ(expected_count, GetThreadLocalCounter());
  }

  void StartProducerTask(int produce_count, int consumer_count,
                         coro::EventLoopWakeUpHandle target) {
    coro::StartEventLoop(ProduceTask(produce_count, consumer_count, target));
  }
};

TEST_F(DispatcherCoroTest, EvenlyDispatchTest) {
  finished_produce_count_ = 0;
  prepared_consumer_count_ = 0;
  int expected_value = 10;
  int consumer_count = 20;
  int producer_count = 10;
  std::vector<std::thread> consumer_threads;
  std::vector<std::thread> producer_threads;

  EXPECT_EQ((expected_value * consumer_count) % producer_count, 0);
  int per_producer_count = (expected_value * consumer_count) / producer_count;

  for (int i = 0; i < consumer_count; i++) {
    consumer_threads.emplace_back(&DispatcherCoroTest::StartLongRunTask, this,
                                  expected_value, producer_count);
  }
  for (int i = 0; i < producer_count; i++) {
    producer_threads.emplace_back(&DispatcherCoroTest::StartProducerTask, this,
                                  per_producer_count, consumer_count, -1);
  }

  for (int i = 0; i < consumer_count; i++) {
    consumer_threads[i].join();
  }
  for (int i = 0; i < producer_count; i++) {
    producer_threads[i].join();
  }
}

TEST_F(DispatcherCoroTest, BiasedDispatchTest) {
  finished_produce_count_ = 0;
  prepared_consumer_count_ = 0;
  int per_producer_count = 10;
  int consumer_count = 4;
  int producer_count = 10;
  std::vector<std::thread> consumer_threads;
  std::vector<std::thread> producer_threads;

  std::thread biased_consumer_thread(&DispatcherCoroTest::StartLongRunTask,
                                     this, per_producer_count * producer_count,
                                     producer_count);
  std::this_thread::sleep_for(
      std::chrono::milliseconds(100));  // make sure we register above consumer
  coro::EventLoopWakeUpHandle target =
      coro::CoroutineDispatcher::GetInstance()
          .GetAvailableDispatchDestinations()[0];

  for (int i = 0; i < consumer_count; i++) {
    consumer_threads.emplace_back(&DispatcherCoroTest::StartLongRunTask, this,
                                  0, producer_count);
  }
  for (int i = 0; i < producer_count; i++) {
    producer_threads.emplace_back(&DispatcherCoroTest::StartProducerTask, this,
                                  per_producer_count, consumer_count + 1,
                                  target);
  }
  for (int i = 0; i < producer_count; i++) {
    producer_threads[i].join();
  }

  biased_consumer_thread.join();
  for (int i = 0; i < consumer_count; i++) {
    consumer_threads[i].join();
  }
}

}  // namespace test
}  // namespace arc

#endif