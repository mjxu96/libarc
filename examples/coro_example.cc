/*
 * File: example.cc
 * Project: libarc
 * File Created: Tuesday, 8th December 2020 10:18:54 pm
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

#include <arc/arc.h>
#include <arc/coro/task.h>

#include <memory>

using namespace arc::coro;

Task<int> InternalTask(int i) {
  // std::cout << "interanal " << i << std::endl;
  if (i < 0) {
    std::string error =
        std::string("i cannot be smaller than 0, now is: ") + std::to_string(i);
    throw std::logic_error(error.c_str());
  } else if (i == 0) {
    std::cout << "sleep for 3 seconds before return" << std::endl;
    co_await SleepFor(std::chrono::seconds(3));
    co_return i;
  }
  co_return co_await InternalTask(i - 1) + i;
  // assert(i > -1);
  // std::cout << i << std::endl;
  // co_return (i >= 0 ? i : i - 1);
  // co_return (i >= 0 ? i : (co_await InternalTask(i - 1)));
}

Task<std::unique_ptr<int>> TestMove(int i) {
  co_return std::make_unique<int>(i);
}

Task<void> TestException() { throw std::logic_error("some error"); }

Task<void> TestFuture(int i) {
  i = co_await InternalTask(i);
  co_return;
}

Task<void> TestEmptyCoro() {
  for (int i = 0; i < 2; i++) {
    EnsureFuture(TestFuture(0));
  }
  int value = 0;
  for (int i = 0; i < 2; i++) {
    int ii = co_await InternalTask(1);
    value += ii;
  }
  std::cout << value << std::endl;
  std::cout << co_await InternalTask(100000) << std::endl;

  std::cout << *(co_await TestMove(123)) << std::endl;

  try {
    co_await InternalTask(-100);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
  }

  try {
    co_await TestException();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
  }

  // throw std::logic_error("som");

  co_return;
}

int test111() {
  int value = 0;
  for (int i = 0; i < 1000000; i++) {
    value += 1;
  }
  return value;
}

Task<void> SleepTime(int seconds) {
  std::cout << "before sleep for " << seconds << std::endl;
  auto now = std::chrono::system_clock::now();
  co_await SleepFor(std::chrono::seconds(seconds));
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - now).count();
  std::cout << "after sleep for " << seconds << " actually slept for " << elapsed << std::endl;
}

Task<void> MultipleTimers(int num) {
  for (int i = 0; i < num; i++) {
    EnsureFuture(SleepTime(num - i));
  }
  co_return;
}



int main(int argc, char** argv) {
  std::cout << "arc version: " << arc::Version() << std::endl;
  try {
    /* code */
    StartEventLoop(TestEmptyCoro());
    // StartEventLoop(MultipleTimers(std::stoi(std::string(argv[1]))));
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
  }

  // test111();
}