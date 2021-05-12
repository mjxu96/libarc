/*
 * File: test_coro.cc
 * Project: libarc
 * File Created: Monday, 10th May 2021 10:56:06 pm
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
#include <arc/coro/task.h>
#include <gtest/gtest.h>

namespace arc {
namespace test {

class BasicCoroTest : public ::testing::Test {
 protected:
  coro::Task<int> ReturnInt(int i) {
    if (i == 1) {
      co_return 1;
    } else if (i > 1) {
      co_return co_await ReturnInt(i - 1) + i;
    } else {
      throw std::logic_error("Error input");
    }
  }

  coro::Task<void> SimpleTestCoro() {
    int ret = co_await ReturnInt(1);
    EXPECT_EQ(ret, 1);
  }

  coro::Task<void> LoopTestCoro() {
    int ret = 0;
    for (int i = 0; i < 100000; i++) {
      ret += co_await ReturnInt(1);
    }
    EXPECT_EQ(ret, 100000);
  }

  coro::Task<void> RecursiveTestCoro() {
    int ret = co_await ReturnInt(10000);
    EXPECT_EQ(ret, 50005000);
  }

  coro::Task<void> InnerTimerTestCoro(int milliseconds) {
    co_await arc::coro::SleepFor(std::chrono::milliseconds(milliseconds));
  }

  coro::Task<void> TimerTestCoro(int milliseconds) {
    const float kMaxAllowedRefError = 0.05;
    auto now = std::chrono::steady_clock::now();
    co_await arc::coro::SleepFor(std::chrono::milliseconds(milliseconds));
    auto then = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(then - now)
            .count();
    EXPECT_NEAR(milliseconds, elapsed, (milliseconds * kMaxAllowedRefError));

    now = then;
    milliseconds = 1000 - milliseconds;
    co_await InnerTimerTestCoro(milliseconds);
    then = std::chrono::steady_clock::now();
    elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(then - now)
            .count();
    EXPECT_NEAR(milliseconds, elapsed, ((milliseconds) * kMaxAllowedRefError));
    co_return;
  }

  coro::Task<void> TestMultiPleTimer(int count) {
    int min_sleep_time = 200;
    int step = 100;
    for (int i = count; i > 0; i--) {
      arc::coro::EnsureFuture(TimerTestCoro(min_sleep_time + step * i));
    }
    co_return;
  }

  void Fail() { FAIL() << "Expected std::logic_error"; }

  coro::Task<void> ExceptionTestCoro() {
    try {
      int ret = co_await ReturnInt(-1);
    } catch (std::logic_error const& err) {
      EXPECT_EQ(err.what(), std::string("Error input"));
    } catch (...) {
      Fail();
    }
    co_return;
  }
};

TEST_F(BasicCoroTest, SimpleTest) { coro::StartEventLoop(SimpleTestCoro()); }

TEST_F(BasicCoroTest, LoopTest) { coro::StartEventLoop(LoopTestCoro()); }

TEST_F(BasicCoroTest, RecursiveTest) {
  coro::StartEventLoop(RecursiveTestCoro());
}

TEST_F(BasicCoroTest, TimerTest) {
  // coro::StartEventLoop(TimerTestCoro(1000));
  coro::StartEventLoop(TestMultiPleTimer(5));
}

TEST_F(BasicCoroTest, ExceptionTest) {
  coro::StartEventLoop(ExceptionTestCoro());
}

}  // namespace test
}  // namespace arc

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}