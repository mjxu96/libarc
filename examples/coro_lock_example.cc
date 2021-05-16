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
#include <iostream>

#include <arc/coro/task.h>
#include <arc/coro/eventloop.h>
#include <arc/coro/locks/condition.h>
#include <arc/coro/locks/lock.h>

#include <chrono>
#include <thread>

using namespace arc;
using namespace arc::coro;

Condition cond;
Lock lock;

std::atomic<int> stage_count;


Task<void> CoroWait() {
  // co_await cond.Wait();
  // std::cout << "before wait" << std::endl;
  co_await cond.Wait(lock);
  lock.Release();
  // std::cout << "released lock" << std::endl;
  // std::cout << "after wait" << std::endl;
  // std::cout << "before wait 2" << std::endl;
  // co_await cond.Wait();
  co_await cond.Wait(lock);
  lock.Release();
  // lock.Release();
  // std::cout << "after wait 2" << std::endl;
  // std::cout << "finished" << count.fetch_add(1) << std::endl;
}

Task<void> LockWait() {
  for (int i = 0; i < 10; i++) {
    // std::cout << "before lock" << std::endl;
    co_await lock.Acquire();
    // std::cout << "after lock" << std::endl;

    // std::cout << "before releasing" << std::endl;
    lock.Release();
    // std::cout << "after releasing" << std::endl;
  }
}

void StartCoroWait(int num) {
  for (int i = 0; i < num; i++) {
    EnsureFuture(CoroWait());
  }
  RunUntilComplelete();
}

void StartLockWait(int num) {
  for (int i = 0; i < num; i++) {
    EnsureFuture(LockWait());
  }
  RunUntilComplelete();
}

Task<void> StartCondition(int thread_num) {
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; i++) {
    threads.emplace_back(StartCoroWait, 5);
  }
  // for (int i = 0; i < 10000; i++) {
  //   EnsureFuture(CoroWait());
  // }
  co_await SleepFor(std::chrono::seconds(1));
  std::cout << "before sending one signal" << std::endl;
  cond.NotifyOne();
  std::cout << "after sending one signal" << std::endl;
  co_await SleepFor(std::chrono::seconds(1));
  std::cout << "before sending all signals" << std::endl;
  cond.NotifyAll();
  std::cout << "after sending all signals" << std::endl;
  co_await SleepFor(std::chrono::seconds(1));
  std::cout << "before sending all signals 2" << std::endl;
  cond.NotifyAll();
  std::cout << "after sending all signals 2" << std::endl;
  // std::cout << "before sending all signals 3" << std::endl;
  // int c;
  // std::cin >> c;
  // cond.NotifyAll();
  // std::cout << "after sending all signals 3" << std::endl;
  for (int i = 0; i < thread_num; i++) {
    threads[i].join();
  }
}

Task<void> StartLock(int thread_num) {
  std::cout << "main lock try acquire" << std::endl;
  co_await lock.Acquire();
  std::cout << "main lock acquired" << std::endl;
  for (int i = 0; i < 100; i++) {
    EnsureFuture(LockWait());
  }
  // StartLockWait(100);

  // std::vector<std::thread> threads;
  // for (int i = 0; i < thread_num; i++) {
  //   threads.emplace_back(StartLockWait, 100);
  // }

  co_await SleepFor(std::chrono::seconds(1));
  std::cout << "main lock try releasing" << std::endl;
  lock.Release();
  std::cout << "main lock Released" << std::endl;
  std::cout << "main lock try to lock again" << std::endl;
  co_await lock.Acquire();
  std::cout << "main lock locked again" << std::endl;
  co_await SleepFor(std::chrono::seconds(1));
  lock.Release();
  std::cout << "main lock released again" << std::endl;

  // for (int i = 0; i < thread_num; i++) {
  //   threads[i].join();
  // }
  std::cout << "main try lock locked again again" << std::endl;
  co_await lock.Acquire();
  std::cout << "main lock locked again again" << std::endl;
  co_await SleepFor(std::chrono::seconds(1));
  lock.Release();
}

int main() {
  // StartEventLoop(StartCondition(5));
  // StartLockWait(100);
  StartEventLoop(StartLock(3));
  return 0;
}
