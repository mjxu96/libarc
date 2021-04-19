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

#include <arc/coro/events/io_event_base.h>
#include <arc/io/io_base.h>
#include <arc/utils/bits.h>
#include <assert.h>
#include <sys/epoll.h>
#include <arc/concept/coro.h>
#include <arc/coro/dispatcher.h>

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
class [[nodiscard]] Task;

class EventLoop : public io::detail::IOBase {
 public:
  EventLoop();
  ~EventLoop() = default;

  bool IsDone();
  void Do();
  void AddIOEvent(events::detail::IOEventBase* event, bool replace = false);
  void RemoveIOEvent(events::detail::IOEventBase* event);

  void AddToCleanUpCoroutine(std::coroutine_handle<> handle);

  void CleanUp();
  void CleanUpFinishedCoroutines();

  void Dispatch(Task<void>&& task);
  void AsProducer();
  void AsConsumer();

 private:
  const static int kMaxEventsSizePerWait_ = 1024;
  const static int kMaxFdInArray_ = 1024;
  const static int kEpollWaitTimeout_ = 1;
  const static int kMaxConsumableCoroutineNum_ = 4;

  int total_added_task_num_{0};

  // {fd -> {io_type -> [events]}}
  std::vector<std::vector<std::deque<events::detail::IOEventBase*>>> io_events_{
      kMaxFdInArray_, std::vector<std::deque<events::detail::IOEventBase*>>{
                          2, std::deque<events::detail::IOEventBase*>{}}};

  std::unordered_map<int, std::vector<std::deque<events::detail::IOEventBase*>>>
      extra_io_events_{};

  // epoll related
  epoll_event events[kMaxEventsSizePerWait_];
  events::detail::IOEventBase* todo_events[2 * kMaxEventsSizePerWait_] = {
      nullptr};

  int GetExistIOEvent(int fd);
  events::detail::IOEventBase* GetEvent(int fd,
                                        events::detail::IOEventType event_type);

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
