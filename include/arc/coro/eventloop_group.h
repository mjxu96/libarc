/*
 * File: eventloop_group.h
 * Project: libarc
 * File Created: Saturday, 22nd May 2021 7:35:50 pm
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

#ifndef LIBARC__CORO__EVENTLOOP_GROUP_H
#define LIBARC__CORO__EVENTLOOP_GROUP_H

#include "eventloop.h"

namespace arc {
namespace coro {


class EventLoopGroup {
 public:
  static EventLoopGroup& GetInstance() {
    static EventLoopGroup group;
    return group;
  }

  EventLoopID RegisterEventLoop(EventLoop* loop) {
    std::lock_guard guard(lock_);
    loops_.insert({max_id_, loop});
    max_id_++;
    return max_id_ - 1;
  }

  void DeRegisterEventLoop(EventLoopID id) {
    std::lock_guard guard(lock_);
    loops_.erase(id);
  }

  EventLoop* GetEventLoop(EventLoopID id) {
    std::lock_guard guard(lock_);
    return GetEventLoopNoLock(id);
  }

  EventLoop* GetEventLoopNoLock(EventLoopID id) {
    auto event_loop_itr = loops_.find(id);
    if (event_loop_itr == loops_.end()) {
      return nullptr;
    }
    return event_loop_itr->second;
  }

  std::mutex& EventLoopGroupLock() {
    return lock_;
  }

 private:
  EventLoopGroup() = default;

  std::mutex lock_;
  EventLoopID max_id_{0};
  std::unordered_map<EventLoopID, EventLoop*> loops_;
};

}  // namespace coro
}  // namespace arc

#endif
