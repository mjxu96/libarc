/*
 * File: debug.h
 * Project: libarc
 * File Created: Saturday, 17th April 2021 10:44:26 am
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

// Check pointer address

#ifndef LIBARC__UTILS__DEBUG_H
#define LIBARC__UTILS__DEBUG_H

#ifdef NDEBUG
#define ASSERT_POINTER_IN_HEAP ((void)0)
#define ASSERT_POINTER_IN_STACK ((void)0)
#else

#include <cassert>
#include <cstdint>
#include <iostream>

#define ASSERT_POINTER_IN_HEAP(Pointer, Msg) \
  ASSERT_POINTER_POSITION(Pointer, true, Msg);
#define ASSERT_POINTER_IN_STACK(Pointer, Msg) \
  ASSERT_POINTER_POSITION(Pointer, false, Msg);

#define ASSERT_POINTER_POSITION(Pointer, ShouldInHeap, Msg)                  \
  {                                                                          \
    std::uintptr_t pointer_pos = reinterpret_cast<std::uintptr_t>(Pointer);  \
    std::uintptr_t x = 0;                                                    \
    asm("movq %%rsp, %0;" : "=r"(x));                                        \
    std::cout << Pointer << "  " << reinterpret_cast<void*>(x) << std::endl; \
    if (ShouldInHeap) {                                                      \
      assert((Msg, (pointer_pos < x)));                                      \
    } else {                                                                 \
      assert((Msg, (pointer_pos >= x)));                                     \
    }                                                                        \
  }

#endif
#endif