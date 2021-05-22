/*
 * File: coro_executor.cc
 * Project: libarc
 * File Created: Saturday, 22nd May 2021 1:47:32 pm
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

#include <arc/coro/utils/executor.h>
#include <arc/coro/task.h>
using namespace arc::coro;

void BlockingRunner(int milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

Task<void> RunOneBlockingTask(int milliseconds) {
  Executor executor;
  auto now = std::chrono::steady_clock::now();
  co_await executor.Execute(&BlockingRunner, milliseconds);
  auto then = std::chrono::steady_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(then - now).count() << std::endl;
}

Task<void> RunMultipleBlockingTask(int num, int milliseconds) {
  for (int i = 0; i < num; i++) {
    EnsureFuture(RunOneBlockingTask(milliseconds));
  }
  co_await SleepFor(std::chrono::milliseconds(milliseconds / 2));
}

void RunMultiThreadMultipleBlockingTask(int thread_num, int per_thread_num, int milliseconds) {
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; i++) {
    threads.emplace_back([=]() {
      StartEventLoop(RunMultipleBlockingTask(per_thread_num, milliseconds));
    });
  }

  for (int i = 0; i < thread_num; i++) {
    threads[i].join();
  }
}

int main() {
  // StartEventLoop(RunMultipleBlockingTask(20, 1000));
  RunMultiThreadMultipleBlockingTask(10, 10, 1000);
}
