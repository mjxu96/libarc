/*
 * File: condition.h
 * Project: libarc
 * File Created: Friday, 14th May 2021 9:03:58 pm
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

#ifndef LIBARC__CORO__LOCKS__CONDITION_H
#define LIBARC__CORO__LOCKS__CONDITION_H

#include <arc/coro/awaiter/condition_awaiter.h>
#include <arc/exception/io.h>
#include <unordered_set>

namespace arc {
namespace coro {

class Condition {
 public:
  Condition() {
    static_assert(sizeof(i) == 8);
    core_ = new arc::events::detail::ConditionCore();
  }

  void NotifyOne() {
    std::lock_guard guard(core_->lock);
    if (core_->registered_count <= 0) {
      return;
    }
    if (write(ModifyConditionCore(), &i, sizeof(i)) != sizeof(i)) {
      std::cout << errno << std::endl;
      throw arc::exception::IOException("NotifyOne Error");
    }
  }

  void NotifyAll() {
    std::lock_guard guard(core_->lock);
    std::unordered_set<int> to_notified_event_ids;
    while (core_->registered_count > 0) {
      to_notified_event_ids.insert(ModifyConditionCore());
    }
    for (int to_notified_event_id : to_notified_event_ids) {
      if (write(to_notified_event_id, &i, sizeof(i)) != sizeof(i)) {
        throw arc::exception::IOException("NotifyOne Error");
      }
    }
  }

  ConditionAwaiter Wait() {
    return ConditionAwaiter(core_);
  }

  ~Condition() {
    delete core_;
  }

  // Condition cannot be copied nor moved.
  Condition(const Condition&) = delete;
  Condition& operator=(const Condition&) = delete;
  Condition(Condition&&) = delete;
  Condition& operator=(Condition&&) = delete;

 private:
  arc::events::detail::ConditionCore* core_{nullptr};
  std::uint64_t i{1};

  int ModifyConditionCore() {
    int event_id = core_->in_queue_event_ids.front();
    core_->in_queue_event_ids.pop_front();

    core_->registered_count--;
    core_->registered_events[event_id]--;
    if (core_->registered_events[event_id] == 0) {
      core_->registered_events.erase(event_id);
    }

    core_->triggered_count++;
    if (core_->triggered_events.find(event_id) == core_->triggered_events.end()) {
      core_->triggered_events[event_id] = 0;
    }
    core_->triggered_events[event_id]++;

    return event_id;
  }
};
  
} // namespace coro
} // namespace arc


#endif