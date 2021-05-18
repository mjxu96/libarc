/*
 * File: test_coro_lock.h
 * Project: libarc
 * File Created: Saturday, 15th May 2021 9:45:44 am
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

#ifndef LIBARC__TESTS__TEST_CORO_LOCK_H
#define LIBARC__TESTS__TEST_CORO_LOCK_H

#include <arc/coro/eventloop.h>
#include <arc/coro/locks/condition.h>
#include <arc/coro/locks/lock.h>
#include <gtest/gtest.h>

#include "utils.h"

namespace arc {
namespace test {

class LockCoroTest : public ::testing::Test {
 protected:
  float max_allowed_ref_error_{0.05};
  constexpr static int kWaitTimeMS_ = 50;
  constexpr static int kCondPerWaitTimeMs_ = 1000;
  int lock_value_ = 0;
  int cond_value_ = 0;

  std::atomic<int> released_ = 0;

  arc::coro::Lock lock_;
  arc::coro::Condition cond_;

  void virtual SetUp() override {
    if (IsRunningWithValgrind()) {
      max_allowed_ref_error_ = 0.3;
    }
  }

  arc::coro::Task<void> LockCoro(int num) {
    for (int i = 0; i < num; i++) {
      // std::cerr << i << " try to acquired lock" << std::endl;
      co_await lock_.Acquire();
      // std::cerr << i << " acquired lock" << std::endl;
      lock_value_++;
      co_await arc::coro::SleepFor(std::chrono::milliseconds(kWaitTimeMS_));
      EXPECT_EQ(lock_value_, 1);
      lock_value_--;
      // std::cerr << i << "released lock" << std::endl;
      lock_.Release();
    }
  }

  arc::coro::Task<void> CondCoro() {
    co_await lock_.Acquire();
    co_await cond_.Wait(lock_);
    cond_value_++;
    lock_.Release();
    co_await lock_.Acquire();
    co_await cond_.Wait(lock_);
    cond_value_++;
    lock_.Release();
  }

 public:
  void RunMultipleCondCoros(int num) {
    for (int i = 0; i < num; i++) {
      arc::coro::EnsureFuture(CondCoro());
    }
    arc::coro::RunUntilComplelete();
  }

  arc::coro::Task<void> RunMultipleCondCorosInMultithreads(int thread_num,
                                                           int num) {
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_num; i++) {
      threads.emplace_back(&LockCoroTest::RunMultipleCondCoros, this, num);
    }
    co_await arc::coro::SleepFor(
        std::chrono::milliseconds(kCondPerWaitTimeMs_));
    cond_.NotifyOne();
    co_await arc::coro::SleepFor(
        std::chrono::milliseconds(kCondPerWaitTimeMs_));
    cond_.NotifyAll();
    co_await arc::coro::SleepFor(
        std::chrono::milliseconds(kCondPerWaitTimeMs_));
    cond_.NotifyAll();
    for (int i = 0; i < thread_num; i++) {
      threads[i].join();
    }
    EXPECT_EQ(cond_value_, 2 * thread_num * num);
  }

  void RunMultipleLockCoros(int num, int per_num) {
    for (int i = 0; i < num; i++) {
      arc::coro::EnsureFuture(LockCoro(per_num));
    }
    arc::coro::RunUntilComplelete();
  }

  void RunMultipleLockCorosInMultithreads(int thread_num, int num,
                                          int per_num) {
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_num; i++) {
      threads.emplace_back(&LockCoroTest::RunMultipleLockCoros, this, num,
                           per_num);
    }
    for (int i = 0; i < thread_num; i++) {
      threads[i].join();
    }
  }
};

TEST_F(LockCoroTest, BasicLockTest) {
  int run_times = 20;
  int per_num = 10;
  auto elapsed = GetElapsedTimeMilliseconds(
      std::bind(&LockCoroTest::RunMultipleLockCoros, this, run_times, per_num));
  EXPECT_NEAR(run_times * per_num * kWaitTimeMS_, elapsed,
              (elapsed * max_allowed_ref_error_));
}

TEST_F(LockCoroTest, BasicLockMultiThreadTest) {
  int thread_num = 20;
  int run_times = 10;
  int per_num = 4;
  auto elapsed = GetElapsedTimeMilliseconds(
      std::bind(&LockCoroTest::RunMultipleLockCorosInMultithreads, this,
                thread_num, run_times, per_num));
  EXPECT_NEAR(thread_num * run_times * per_num * kWaitTimeMS_, elapsed,
              (elapsed * max_allowed_ref_error_));
}

TEST_F(LockCoroTest, BasicCondMultiThreadTest) {
  int thread_num = 20;
  int run_times = 20;
  arc::coro::StartEventLoop(
      RunMultipleCondCorosInMultithreads(thread_num, run_times));
}

}  // namespace test
}  // namespace arc

#endif
