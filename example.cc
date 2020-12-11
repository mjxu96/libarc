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

#include <arc/coro/task.h>

using namespace arc::coro;

Task<int> InternalTask(int i) {
  if (i <= 0) {
    co_return 1;
  } else {
    co_return co_await InternalTask(i - 1) + i;
  }
  // assert(i > -1);
  // std::cout << i << std::endl;
  // co_return (i >= 0 ? i : i - 1);
  // co_return (i >= 0 ? i : (co_await InternalTask(i - 1)));
}

Task<void> TestEmptyCoro() {
  int value = 0;
  for (int i = 0; i < 1000000; i++) {
    int ii = co_await InternalTask(0);
    value += ii;
    // std::cout << ii << std::endl;
  }
  // std::cout << co_await InternalTask(10) << std::endl;
  std::cout << value << std::endl;
  co_return;
}


int test111() {
  int value = 0;
  for (int i = 0; i < 1000000; i++) {
    value += 1;
  }
  return value;
}

void StartEventLoop(Task<void> task) {
  task.Start();
  auto& event_loop = GetLocalEventLoop();
  while (!event_loop.IsDone()) {
    event_loop.Do();
  }
  std::cout << "finished" << std::endl;
}

int main() {
  StartEventLoop(TestEmptyCoro());
  // test111();
}