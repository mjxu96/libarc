/*
 * File: coro_lock_examle.cc
 * Project: libarc
 * File Created: Friday, 14th May 2021 11:53:29 pm
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

#include <arc/coro/task.h>
#include <arc/coro/eventloop.h>
#include <arc/coro/locks/condition.h>

#include <iostream>
#include <chrono>
#include <thread>

using namespace arc;
using namespace arc::coro;

Condition cond;

Task<void> CoroWait() {
  // std::cout << "before wait" << std::endl;
  co_await cond.Wait();
  // std::cout << "after wait" << std::endl;
}

void StartCoroWait() {
  for (int i = 0; i < 3000; i++) {
    EnsureFuture(CoroWait());
  }
  RunUntilComplelete();
}

Task<void> StartCondition(int thread_num) {
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; i++) {
    threads.emplace_back(StartCoroWait);
  }
  co_await SleepFor(std::chrono::seconds(1));
  std::cout << "before sending one signal" << std::endl;
  cond.NotifyOne();
  std::cout << "after sending one signal" << std::endl;
  co_await SleepFor(std::chrono::seconds(1));
  std::cout << "before sending all signals" << std::endl;
  cond.NotifyAll();
  std::cout << "after sending all signals" << std::endl;
  for (int i = 0; i < thread_num; i++) {
    threads[i].join();
  }
}

int main() {
  StartEventLoop(StartCondition(10));
  return 0;
}
