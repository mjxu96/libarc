/*
 * File: dispatcher.cc
 * Project: libarc
 * File Created: Monday, 19th April 2021 8:16:53 pm
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

#include <arc/coro/dispatcher.h>
#ifdef __clang__
#include <experimental/coroutine>
namespace std {
using experimental::coroutine_handle;
}
#else
#include <coroutine>
#endif

#define DISPATCHER_MAX_ELEMENT_NUM 1024

using namespace arc::coro;

// CoroutineDispatcherConsumerToken::CoroutineDispatcherConsumerToken(
//     CoroutineDispatcher& dispatcher,
//     CoroutineDispatcherConsumerIDType consumer_id)
//     : token_(dispatcher.queue_), consumer_id_(consumer_id) {}

arc::coro::CoroutineDispatcher& arc::coro::GetGlobalCoroutineDispatcher() {
  static arc::coro::CoroutineDispatcher dispatcher;
  return dispatcher;
}
