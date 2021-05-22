/*
 * File: test_coro_executor.h
 * Project: libarc
 * File Created: Saturday, 22nd May 2021 2:12:38 pm
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

#ifndef LIBARC__TESTS__TEST_CORO_EXECUTOR_H
#define LIBARC__TESTS__TEST_CORO_EXECUTOR_H

#include <arc/coro/task.h>
#include <arc/coro/utils/executor.h>
#include <gtest/gtest.h>

#include "utils.h"

namespace arc {
namespace test {

class ExecutorCoroTest : public ::testing::Test {
 protected:
  float max_allowed_ref_error_ = 0.05;

  virtual void SetUp() override {
    if (IsRunningWithValgrind()) {
      max_allowed_ref_error_ = 0.2;
    }
  }

  void BlockingRunner(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
  }

  coro::Task<void> RunOneBlockingTask(int milliseconds) {
    coro::Executor executor;
    co_await executor.Execute(&ExecutorCoroTest::BlockingRunner, this,
                              milliseconds);
  }

  coro::Task<void> RunMultipleBlockingTask(int num, int milliseconds) {
    for (int i = 0; i < num; i++) {
      EnsureFuture(RunOneBlockingTask(milliseconds));
    }
    co_await coro::SleepFor(std::chrono::milliseconds(milliseconds / 2));
  }

 public:
  void RunMultiThreadMultipleBlockingTask(int thread_num, int per_thread_num,
                                          int milliseconds) {
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_num; i++) {
      threads.emplace_back([this, per_thread_num, milliseconds]() {
        coro::StartEventLoop(
            this->RunMultipleBlockingTask(per_thread_num, milliseconds));
      });
    }

    for (int i = 0; i < thread_num; i++) {
      threads[i].join();
    }
  }
};

TEST_F(ExecutorCoroTest, TestTotalTime) {
  int thread_num = 10;
  int per_thread_num = 10;
  int sleep_time = 500;
  int used_time = GetElapsedTimeMilliseconds(
      std::bind(&ExecutorCoroTest::RunMultiThreadMultipleBlockingTask, this,
                thread_num, per_thread_num, sleep_time));
  EXPECT_NEAR(used_time,
              sleep_time * (thread_num * per_thread_num /
                                std::thread::hardware_concurrency() +
                            1),
              used_time * max_allowed_ref_error_);
}

}  // namespace test
}  // namespace arc

#endif