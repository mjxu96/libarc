/*
 * File: eventloop.h
 * Project: libarc
 * File Created: Monday, 7th December 2020 9:17:34 pm
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

#ifndef LIBARC__CORO__EVENTLOOP_H
#define LIBARC__CORO__EVENTLOOP_H

#include <arc/concept/coro.h>
#include <arc/coro/dispatcher.h>
#include <arc/coro/events/io_event.h>
#include <arc/io/io_base.h>
#include <arc/utils/bits.h>
#include <assert.h>
#include <sys/epoll.h>

#ifdef __linux__
#include <arc/coro/poller/epoll.h>
#endif

#ifdef __clang__
#include <experimental/coroutine>
namespace std {
using experimental::coroutine_handle;
}
#else
#include <coroutine>
#endif
#include <iostream>
#include <list>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace arc {
namespace coro {

enum class EventLoopType {
  PRODUCER = 0U,
  CONSUMER,
  NONE,
};

template <arc::concepts::CopyableMoveableOrVoid T>
class[[nodiscard]] Task;

class EventLoop {
 public:
  EventLoop();
  ~EventLoop() = default;

  bool IsDone();
  void Do();

  inline void AddIOEvent(events::IOEvent* event, bool replace = false) {
    poller_->AddIOEvent(event, replace);
  }

  inline void RemoveAllIOEvents(int fd) { poller_->RemoveAllIOEvents(fd); }

  void AddToCleanUpCoroutine(std::coroutine_handle<> handle);

  void CleanUpFinishedCoroutines();

  void Dispatch(Task<void>&& task);
  void AsProducer();
  void AsConsumer();

 private:
  detail::Poller* poller_{nullptr};

  const static int kMaxEventsSizePerWait_ = 1024;
  const static int kMaxConsumableCoroutineNum_ = 4;

  // poller related
  events::IOEvent* todo_events_[2 * kMaxEventsSizePerWait_] = {nullptr};

  std::vector<std::coroutine_handle<>> to_clean_up_handles_{};
  std::vector<std::coroutine_handle<>> to_dispatched_coroutines_{};
  EventDispatcher<std::coroutine_handle<void>>* global_dispatcher_{nullptr};
  EventLoopType type_{EventLoopType::NONE};
  void ConsumeCoroutine();
  void ProduceCoroutine();
};

EventLoop& GetLocalEventLoop();

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__CORE__EVENTLOOP_H */
