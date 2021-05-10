#include <gtest/gtest.h>
#include <arc/coro/task.h>
#include <arc/coro/eventloop.h>

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

  coro::Task<void> TimerTestCoro(int milliseconds) {
    const float kMaxAllowedRefError = 0.05;
    auto now = std::chrono::steady_clock::now();
    co_await arc::coro::SleepFor(std::chrono::milliseconds(milliseconds));
    auto then = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(then - now).count();
    EXPECT_NEAR(milliseconds, elapsed, (milliseconds * kMaxAllowedRefError));
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

  void Fail() {
    FAIL() << "Expected std::logic_error";
  }

  coro::Task<void> ExceptionTestCoro() {
    try {
      int ret = co_await ReturnInt(-1);
    } catch(std::logic_error const & err) {
      EXPECT_EQ(err.what(),std::string("Error input"));
    } catch(...) {
      Fail();
    }
    co_return;
  }
};

TEST_F(BasicCoroTest, SimpleTest) {
  coro::StartEventLoop(SimpleTestCoro());
}

TEST_F(BasicCoroTest, LoopTest) {
  coro::StartEventLoop(LoopTestCoro());
}

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
 
} // namespace testing
} // namespace arc


int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}