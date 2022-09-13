/*
 * File: test_coro_timeout.h
 * Project: libarc
 * File Created: Saturday, 22nd May 2021 2:32:11 pm
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
#ifndef LIBARC__TESTS__TEST_CORO_TIMEOUT_H
#define LIBARC__TESTS__TEST_CORO_TIMEOUT_H

#include <arc/coro/locks/condition.h>
#include <arc/coro/task.h>
#include <gtest/gtest.h>

#include "utils.h"

namespace arc {
namespace test {

class TimeoutCoroTest : public ::testing::Test {
 protected:
  float max_allowed_ref_error_ = 0.05;
  coro::Condition cond_;
  int self_released_time_milliseconds_ = 500;

  virtual void SetUp() override {
    if (IsRunningWithValgrind()) {
      max_allowed_ref_error_ = 10;
    }
  }

  coro::Task<void> ConditionTimeout(bool will_be_self_released) {
    int millisecond = self_released_time_milliseconds_ * 2;
    auto now = std::chrono::steady_clock::now();
    co_await cond_.WaitFor(std::chrono::milliseconds(millisecond));
    auto then = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(then - now)
            .count();
    if (will_be_self_released) {
      EXPECT_NEAR(self_released_time_milliseconds_, elapsed,
                  self_released_time_milliseconds_ * max_allowed_ref_error_);
    } else {
      EXPECT_NEAR(millisecond, elapsed, millisecond * max_allowed_ref_error_);
    }
  }

  coro::Task<void> MutilpleRunConditionTimeout(int thread_id, int num,
                                               bool will_be_self_released) {
    for (int i = 0; i < num; i++) {
      coro::EnsureFuture(ConditionTimeout(will_be_self_released));
    }
    if (will_be_self_released) {
      co_await coro::SleepFor(
          std::chrono::milliseconds(self_released_time_milliseconds_));
      if (thread_id == 0) {
        cond_.NotifyAll();
      }
    }
  }

  void MultiThreadMutilpleRunConditionCancel(int thread_num, int per_thread_num,
                                             bool will_be_self_released) {
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_num; i++) {
      threads.emplace_back([this, i, per_thread_num, will_be_self_released]() {
        coro::StartEventLoop(this->MutilpleRunConditionTimeout(
            i, per_thread_num, will_be_self_released));
      });
    }

    for (int i = 0; i < thread_num; i++) {
      threads[i].join();
    }
  }
};

TEST_F(TimeoutCoroTest, TestConditionTimeoutSelfReleased) {
  MultiThreadMutilpleRunConditionCancel(10, 100, true);
}

TEST_F(TimeoutCoroTest, TestConditionTimeoutTriggered) {
  MultiThreadMutilpleRunConditionCancel(10, 100, false);
}

}  // namespace test
}  // namespace arc

#endif
