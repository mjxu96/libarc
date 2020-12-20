/*
 * File: eventloop.c
 * Project: libarc
 * File Created: Monday, 7th December 2020 9:17:50 pm
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

#include <arc/coro/eventloop.h>

using namespace arc::coro;

EventLoop::EventLoop() {
  fd_ = epoll_create1(0);
  if (fd_ < 0) {
    arc::utils::ThrowErrnoExceptions();
  }
}

bool EventLoop::IsDone() {
  return total_added_task_num_ <= 0 && coro_events_.empty() &&
         finished_coro_events_.empty();
}

void EventLoop::Do() {
  // First we iterate over unfinished coroutine event
  auto finished_coro_events_itr = finished_coro_events_.begin();
  while (finished_coro_events_itr != finished_coro_events_.end()) {
    (*finished_coro_events_itr)->Resume();
    delete (*finished_coro_events_itr);
    finished_coro_events_itr =
        finished_coro_events_.erase(finished_coro_events_itr);
  }

  if (total_added_task_num_ <= 0) {
    return;
  }

  // Then we will handle all others
  int event_cnt = epoll_wait(fd_, events, kMaxEventsSizePerWait_, -1);
  int todo_cnt = 0;
  for (int i = 0; i < event_cnt; i++) {
    int fd = events[i].data.fd;
    int event_type = events[i].events;
    if (event_type & EPOLLIN) {
      todo_events[todo_cnt] = GetEvent(fd, events::detail::IOEventType::READ);
      todo_cnt++;
    }
    if (event_type & EPOLLOUT) {
      todo_events[todo_cnt] = GetEvent(fd, events::detail::IOEventType::WRITE);
      todo_cnt++;
    }
    if (event_type && (event_type & EPOLLIN == 0) &&
        (event_type & EPOLLOUT == 0)) {
      arc::utils::ThrowErrnoExceptions();
    }
  }
  // currently we enforce this constraint
  // in future we will remove this one
  assert(todo_cnt == event_cnt);

  for (int i = 0; i < todo_cnt; i++) {
    RemoveIOEvent(todo_events[i]);
    todo_events[i]->Resume();
  }

  for (int i = 0; i < todo_cnt; i++) {
    delete todo_events[i];
  }

  total_added_task_num_ -= todo_cnt;
}

void EventLoop::AddIOEvent(events::detail::IOEventBase* event) {
  auto target_fd = event->GetFd();
  events::detail::IOEventType event_type = event->GetIOEventType();
  total_added_task_num_++;
  int should_add_event =
      (event_type == events::detail::IOEventType::READ ? EPOLLIN : EPOLLOUT);
  int prev_events = GetExistIOEvent(target_fd);
  if ((prev_events & should_add_event) == 0) {
    if (target_fd < kMaxFdInArray_) {
      assert(!io_events_[target_fd][static_cast<int>(event_type)]);
      io_events_[target_fd][static_cast<int>(event_type)] = event;
    } else {
      assert(extra_io_events_.find(target_fd) == extra_io_events_.end() ||
             !extra_io_events_[target_fd][static_cast<int>(event_type)]);
      if (extra_io_events_.find(target_fd) == extra_io_events_.end()) {
        extra_io_events_[target_fd] =
            std::vector<events::detail::IOEventBase*>{2, nullptr};
      }
      extra_io_events_[target_fd][static_cast<int>(event_type)] = event;
    }

    int op = (prev_events == 0 ? EPOLL_CTL_ADD : EPOLL_CTL_MOD);
    epoll_event e_event{};
    e_event.events = prev_events | should_add_event | EPOLLET;
    e_event.data.fd = target_fd;

    int epoll_ret = epoll_ctl(fd_, op, target_fd, &e_event);
    if (epoll_ret != 0) {
      arc::utils::ThrowErrnoExceptions();
    }
  } else {
    // already added
    if (target_fd < kMaxFdInArray_) {
      assert(event->GetIOType() == io::IOType::ACCEPT ||
             io_events_[target_fd][static_cast<int>(event_type)] != event);
      io_events_[target_fd][static_cast<int>(event_type)] = event;
    } else {
      assert(event->GetIOType() == io::IOType::ACCEPT ||
             extra_io_events_[target_fd][static_cast<int>(event_type)] !=
                 event);
      extra_io_events_[target_fd][static_cast<int>(event_type)] = event;
    }
    return;
  }
}

arc::events::detail::IOEventBase* EventLoop::GetEvent(
    int fd, events::detail::IOEventType event_type) {
  if (fd < kMaxFdInArray_) {
    return io_events_[fd][static_cast<int>(event_type)];
  }
  return extra_io_events_[fd][static_cast<int>(event_type)];
}

void EventLoop::RemoveIOEvent(int fd, io::IOType io_type, bool forced) {
  events::detail::IOEventType event_type =
      (io_type == io::IOType::READ || io_type == io::IOType::ACCEPT
           ? events::detail::IOEventType::READ
           : events::detail::IOEventType::WRITE);
  events::detail::IOEventBase* event = GetEvent(fd, event_type);
  RemoveIOEvent(event, forced);
}

void EventLoop::RemoveIOEvent(events::detail::IOEventBase* event, bool forced) {
  if (!event->ShouldRemoveEveryTime() && !forced) {
    return;
  }
  int target_fd = event->GetFd();
  events::detail::IOEventType event_type = event->GetIOEventType();

  int after_io_events = 0;
  if (target_fd < kMaxFdInArray_) {
    assert(io_events_[target_fd][static_cast<int>(event_type)]);
    if (io_events_[target_fd][static_cast<int>(event_type)] == event) {
      io_events_[target_fd][static_cast<int>(event_type)] = nullptr;
    } else {
      return;
    }
  } else {
    assert(extra_io_events_[target_fd][static_cast<int>(event_type)]);
    if (extra_io_events_[target_fd][static_cast<int>(event_type)] == event) {
      extra_io_events_[target_fd][static_cast<int>(event_type)] = nullptr;
    } else {
      return;
    }
  }
  after_io_events = GetExistIOEvent(target_fd);

  int op = (after_io_events == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD);
  int epoll_ret = -1;
  if (op == EPOLL_CTL_DEL) {
    epoll_ret = epoll_ctl(fd_, op, target_fd, nullptr);
  } else {
    epoll_event e_event{};
    e_event.events = after_io_events | EPOLLET;
    e_event.data.fd = target_fd;
    epoll_ret = epoll_ctl(fd_, op, target_fd, &e_event);
  }
  if (epoll_ret != 0) {
    arc::utils::ThrowErrnoExceptions();
  }
}

void EventLoop::AddCoroutine(events::CoroTaskEvent* event) {
  event->SetCoroId(current_coro_id_);
  assert(coro_events_.find(current_coro_id_) == coro_events_.end());
  coro_events_[current_coro_id_] = event;
  current_coro_id_++;
}

void EventLoop::FinishCoroutine(std::uint64_t coro_id) {
  assert(coro_events_.find(coro_id) != coro_events_.end());
  finished_coro_events_.push_back(coro_events_[coro_id]);
  coro_events_.erase(coro_id);
}

void EventLoop::CleanUp() {
  for (auto [id, event] : coro_events_) {
    delete event;
  }
  for (auto event : finished_coro_events_) {
    delete event;
  }
  for (auto& io_events : io_events_) {
    for (auto& event : io_events) {
      if (event) {
        delete event;
        event = nullptr;
      }
    }
  }
  for (auto& [fd, io_events] : extra_io_events_) {
    for (auto& event : io_events) {
      if (event) {
        delete event;
        event = nullptr;
      }
    }
  }
}

int EventLoop::GetExistIOEvent(int fd) {
  int prev = 0;
  if (fd < kMaxFdInArray_) {
    if (io_events_[fd][static_cast<int>(events::detail::IOEventType::READ)]) {
      prev |= EPOLLIN;
    }
    if (io_events_[fd][static_cast<int>(events::detail::IOEventType::WRITE)]) {
      prev |= EPOLLOUT;
    }
  } else {
    if (extra_io_events_.find(fd) != extra_io_events_.end() &&
        extra_io_events_[fd]
                        [static_cast<int>(events::detail::IOEventType::READ)]) {
      prev |= EPOLLIN;
    }
    if (extra_io_events_.find(fd) != extra_io_events_.end() &&
        extra_io_events_[fd][static_cast<int>(
            events::detail::IOEventType::WRITE)]) {
      prev |= EPOLLOUT;
    }
  }
  return prev;
}

arc::coro::EventLoop& arc::coro::GetLocalEventLoop() {
  thread_local EventLoop loop{};
  return loop;
}
