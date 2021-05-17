/*
 * File: test_utils.h
 * Project: libarc
 * File Created: Saturday, 15th May 2021 9:55:49 am
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
#ifndef LIBARC__TESTS__UTILS_H
#define LIBARC__TESTS__UTILS_H

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>

namespace arc {
namespace test {

bool IsRunningWithValgrind() {
#ifdef __linux__
  char* p = std::getenv("LD_PRELOAD");
  if (p == nullptr) {
    return false;
  }
  return (std::strstr(p, "/valgrind/") != nullptr ||
          std::strstr(p, "/vgpreload") != nullptr);
#else
  return false;
#endif
}

std::string GetHomeDir() {
#ifdef __linux__
  return std::getenv("HOME");
#else
  return "";
#endif
}

int GetElapsedTimeMilliseconds(const std::function<void()>& function) {
  auto now = std::chrono::steady_clock::now();
  function();
  auto then = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(then - now).count();
  return elapsed;
}

int& GetThreadLocalCounter() {
  thread_local int counter;
  return counter;
}

}  // namespace test
}  // namespace arc

#endif
