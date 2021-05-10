/*
 * File: epoll.cc
 * Project: libarc
 * File Created: Monday, 10th May 2021 8:23:48 pm
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

#include <arc/coro/poller/epoll.h>
#include <arc/exception/io.h>
#include <iostream>

using namespace arc;
using namespace arc::coro::detail;

Poller::Poller() {
  fd_ = epoll_create1(0);
  if (fd_ < 0) {
    throw arc::exception::IOException("Epoll Creation Error");
  }
  interesting_fds_.reserve(kMaxFdInArray_);
  std::fill(std::begin(io_prev_events_), std::end(io_prev_events_), 0);
}

int Poller::WaitIOEvents(events::detail::IOEventBase** todo_events, int timeout) {
  int event_cnt =
      epoll_wait(fd_, events_, kMaxEventsSizePerWait_, timeout);
  int todo_cnt = 0;
  for (int i = 0; i < event_cnt; i++) {
    int fd = events_[i].data.fd;
    int event_type = events_[i].events;
    if (event_type & EPOLLIN) {
      todo_events[todo_cnt] = PopEvent(fd, events::detail::IOEventType::READ);
      todo_cnt++;
    }
    if (event_type & EPOLLOUT) {
      todo_events[todo_cnt] = PopEvent(fd, events::detail::IOEventType::WRITE);
      todo_cnt++;
    }
    if (event_type && ((event_type & EPOLLIN) == 0) &&
        ((event_type & EPOLLOUT) == 0)) {
      std::string exception_str = "Returned Epoll Events Are Not Supported" + std::to_string(event_type);
      throw arc::exception::IOException(exception_str);
    }
  }
  return todo_cnt;
}

void Poller::AddIOEvent(events::detail::IOEventBase* event, bool replace) {
  auto target_fd = event->GetFd();
  events::detail::IOEventType event_type = event->GetIOEventType();

  total_events_++;
  int should_add_event =
      (event_type == events::detail::IOEventType::READ ? EPOLLIN : EPOLLOUT);

  std::deque<arc::events::detail::IOEventBase *>* to_be_pushed_queue = nullptr;
  if (target_fd < kMaxFdInArray_) [[likely]] {
    to_be_pushed_queue = &io_events_[target_fd][static_cast<int>(event_type)];
  } else [[unlikely]] {
    if (extra_io_events_.find(target_fd) == extra_io_events_.end()) {
      extra_io_events_[target_fd] =
          std::vector<std::deque<events::detail::IOEventBase*>>{
              2, std::deque<events::detail::IOEventBase*>{}};
    }
    to_be_pushed_queue = &extra_io_events_[target_fd][static_cast<int>(event_type)];
  }

  if (!replace) [[likely]] {
    // add this fd into interesting list
    interesting_fds_.insert(target_fd);
  } else {
    if (!to_be_pushed_queue->empty()) {
      assert(to_be_pushed_queue->size() == 1);
      delete to_be_pushed_queue->front();
      to_be_pushed_queue->pop_front();
      total_events_--;
    } else {
      interesting_fds_.insert(target_fd);
    }
  }
  to_be_pushed_queue->push_back(event);
}

void Poller::RemoveAllIOEvents(int target_fd) {
  bool need_epoll_ctl = false;

  std::deque<arc::events::detail::IOEventBase*>* read_queue = nullptr;
  std::deque<arc::events::detail::IOEventBase*>* write_queue = nullptr;
  if (target_fd < kMaxFdInArray_) [[likely]] {
    read_queue = &io_events_[target_fd][static_cast<int>(events::detail::IOEventType::READ)];
    write_queue = &io_events_[target_fd][static_cast<int>(events::detail::IOEventType::WRITE)];
    io_prev_events_[target_fd] = 0;
  } else [[unlikely]] {
    if (extra_io_events_.find(target_fd) == extra_io_events_.end()) {
      return;
    }
    read_queue = &extra_io_events_[target_fd][static_cast<int>(events::detail::IOEventType::READ)];
    write_queue = &extra_io_events_[target_fd][static_cast<int>(events::detail::IOEventType::WRITE)];
    extra_io_prev_events_.erase(0);
  }
  auto itr = read_queue->begin();
  while (itr != read_queue->end()) {
    need_epoll_ctl = true;
    (*itr)->Resume();
    delete (*itr);
    itr = read_queue->erase(itr);
    total_events_--;
  }
  itr = write_queue->begin();
  while (itr != write_queue->end()) {
    need_epoll_ctl = true;
    (*itr)->Resume();
    delete (*itr);
    itr = write_queue->erase(itr);
    total_events_--;
  }

  if (interesting_fds_.find(target_fd) != interesting_fds_.end()) {
    interesting_fds_.erase(target_fd);
  }

  if (need_epoll_ctl) {
    int epoll_ret = epoll_ctl(fd_, EPOLL_CTL_DEL, target_fd, nullptr);
    if (epoll_ret != 0) [[unlikely]] {
      throw arc::exception::IOException("Epoll Error When Deleting All IO Events of FD: " + std::to_string(target_fd));
    }
  }
}

void Poller::TrimIOEvents() {
  for (int fd : interesting_fds_) {
    int prev_event = 0;
    int cur_event = GetExistingIOEvent(fd);
    if (fd < kMaxFdInArray_) {
      prev_event = io_prev_events_[fd];
      io_prev_events_[fd] = cur_event;
    } else {
      if (extra_io_prev_events_.find(fd) != extra_io_prev_events_.end()) {
        prev_event = extra_io_prev_events_[fd];
      }
      extra_io_prev_events_[fd] = cur_event;
    }

    if (cur_event == prev_event) {
      continue;
    }

    int op = prev_event == 0 ? EPOLL_CTL_ADD : (cur_event == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD);
    epoll_event e_event{};
    e_event.events = cur_event;
    e_event.data.fd = fd;
    int epoll_ret = epoll_ctl(fd_, op, fd, &e_event);
    if (epoll_ret != 0) {
      throw arc::exception::IOException("Epoll Error When Trimming IO Events");
    }
  }
}

events::detail::IOEventBase* Poller::PopEvent(int fd,
                                      events::detail::IOEventType event_type) {
  arc::events::detail::IOEventBase* event = nullptr;
  if (fd < kMaxFdInArray_) {
    event = io_events_[fd][static_cast<int>(event_type)].front();
    io_events_[fd][static_cast<int>(event_type)].pop_front();
  } else {
    event = extra_io_events_[fd][static_cast<int>(event_type)].front();
    extra_io_events_[fd][static_cast<int>(event_type)].pop_front();
  }
  total_events_--;
  interesting_fds_.insert(fd);
  return event;
}

int Poller::GetExistingIOEvent(int fd) {
  int cur = 0;
  if (fd < kMaxFdInArray_) {
    if (!io_events_[fd][static_cast<int>(events::detail::IOEventType::READ)]
             .empty()) {
      cur |= EPOLLIN;
    }
    if (!io_events_[fd][static_cast<int>(events::detail::IOEventType::WRITE)]
             .empty()) {
      cur |= EPOLLOUT;
    }
  } else {
    if (extra_io_events_.find(fd) != extra_io_events_.end() &&
        !extra_io_events_[fd]
                         [static_cast<int>(events::detail::IOEventType::READ)]
                             .empty()) {
      cur |= EPOLLIN;
    }
    if (extra_io_events_.find(fd) != extra_io_events_.end() &&
        !extra_io_events_[fd]
                         [static_cast<int>(events::detail::IOEventType::WRITE)]
                             .empty()) {
      cur |= EPOLLOUT;
    }
  }
  return cur;
}

Poller& arc::coro::detail::GetLocalPoller() {
  thread_local Poller poller;
  return poller;
}
