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
#include <sys/eventfd.h>

using namespace arc;
using namespace arc::coro;

Poller::Poller() {
  fd_ = epoll_create1(0);
  if (fd_ < 0) {
    throw arc::exception::IOException("Epoll Creation Error");
  }
  event_fd_ = eventfd(0, EFD_NONBLOCK);
  if (event_fd_ < 0) {
    throw arc::exception::IOException("EventFd Creation Error");
  }
  interesting_fds_.reserve(kMaxFdInArray_);
  std::fill(std::begin(io_prev_events_), std::end(io_prev_events_), 0);
}

Poller::~Poller() {
  if (event_fd_ >= 0) {
    close(event_fd_);
  }
  if (dispatch_fd_ >= 0) {
    close(dispatch_fd_);
  }
}

int Poller::WaitEvents(events::EventBase** todo_events) {
  int event_cnt =
      epoll_wait(fd_, events_, kMaxEventsSizePerWait, next_wait_timeout_);
  int todo_cnt = 0;

  bool is_user_event_triggered = false;
  bool is_dispatched_triggered = false;

  // io events
  for (int i = 0; i < event_cnt; i++) {
    int fd = events_[i].data.fd;
    if (fd == event_fd_) {
      is_user_event_triggered = true;
      assert(events_[i].events & EPOLLIN);
      continue;
    }
    if (fd == dispatch_fd_) {
      is_dispatched_triggered = true;
      assert(events_[i].events & EPOLLIN);
      continue;
    }
    int event_type = events_[i].events;
    if (event_type & EPOLLIN) {
      todo_events[todo_cnt] = PopIOEvent(fd, io::IOType::READ);
      todo_cnt++;
    }
    if (event_type & EPOLLOUT) {
      todo_events[todo_cnt] = PopIOEvent(fd, io::IOType::WRITE);
      todo_cnt++;
    }
    if (event_type && ((event_type & EPOLLIN) == 0) &&
        ((event_type & EPOLLOUT) == 0)) {
      throw arc::exception::IOException(
          "Returned Epoll Events Are Not Supported" +
          std::to_string(event_type));
    }
  }

  // time events
  if (!time_events_.empty() && todo_cnt < kMaxEventsSizePerWait) {
    std::int64_t current_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            (std::chrono::steady_clock::now()).time_since_epoch())
            .count();
    while (!time_events_.empty() && todo_cnt < kMaxEventsSizePerWait &&
           current_time >= time_events_.top()->GetWakeupTime()) {
      todo_events[todo_cnt] = time_events_.top();
      time_events_.pop();
      todo_cnt++;
    }
  }

  // user events
  if (is_user_event_triggered) {
    int read_bytes = read(event_fd_, &event_read_, sizeof(event_read_));
    if (read_bytes != sizeof(event_read_)) {
      throw arc::exception::IOException("Read user event error");
    }
    auto itr = user_events_.begin();
    while (itr != user_events_.end()) {
      auto user_event = (*itr);
      if (todo_cnt < kMaxEventsSizePerWait) {
        if (user_event->CanResume()) {
          todo_events[todo_cnt] = user_event;
          todo_cnt++;
          // We do not directly resume this user event is because
          // it might change the value below like dispatch_fd_, which
          // will cause a read error
          // user_event->Resume();
          // delete user_event;
          itr = user_events_.erase(itr);
          continue;
        } else {
          itr++;
        }
      } else {
        // need to do it in the next wait routine
        event_read_ = 1;
        int wrote = write(event_fd_, &event_read_, sizeof(event_read_));
        if (wrote != sizeof(event_read_)) {
          throw arc::exception::IOException(
              "Write user event in epoll wait error");
        }
        break;
      }
    }
  }

  // dispatched
  if (is_dispatched_triggered) {
    std::uint64_t count = 0;
    if (read(dispatch_fd_, &count, sizeof(count)) < 0) {
      throw arc::exception::IOException("Read dispatched count error");
    }
  }

  return todo_cnt;
}

void Poller::AddIOEvent(events::IOEvent* event) {
  auto target_fd = event->GetFd();
  io::IOType event_type = event->GetIOType();

  total_io_events_++;
  int should_add_event = (event_type == io::IOType::READ ? EPOLLIN : EPOLLOUT);

  std::deque<arc::events::IOEvent*>* to_be_pushed_queue = nullptr;
  if (target_fd < kMaxFdInArray_)
    [[likely]] {
      to_be_pushed_queue = &io_events_[target_fd][static_cast<int>(event_type)];
    } else [[unlikely]] {
      if (extra_io_events_.find(target_fd) == extra_io_events_.end()) {
        extra_io_events_[target_fd] = std::vector<std::deque<events::IOEvent*>>{
            2, std::deque<events::IOEvent*>{}};
      }
      to_be_pushed_queue =
          &extra_io_events_[target_fd][static_cast<int>(event_type)];
    }

    interesting_fds_.insert(target_fd);
  to_be_pushed_queue->push_back(event);
}

void Poller::AddTimeEvent(events::TimeEvent* event) {
  time_events_.push(event);
}

void Poller::AddUserEvent(events::UserEvent* event) {
  user_events_.push_back(event);
}

void Poller::RemoveAllIOEvents(int target_fd) {
  bool need_epoll_ctl = false;

  std::deque<arc::events::IOEvent*>* read_queue = nullptr;
  std::deque<arc::events::IOEvent*>* write_queue = nullptr;
  if (target_fd < kMaxFdInArray_)
    [[likely]] {
      read_queue = &io_events_[target_fd][static_cast<int>(io::IOType::READ)];
      write_queue = &io_events_[target_fd][static_cast<int>(io::IOType::WRITE)];
      io_prev_events_[target_fd] = 0;
    } else [[unlikely]] {
      if (extra_io_events_.find(target_fd) == extra_io_events_.end()) {
        return;
      }
      read_queue =
          &extra_io_events_[target_fd][static_cast<int>(io::IOType::READ)];
      write_queue =
          &extra_io_events_[target_fd][static_cast<int>(io::IOType::WRITE)];
      extra_io_prev_events_.erase(0);
    } auto itr = read_queue->begin();
  while (itr != read_queue->end()) {
    need_epoll_ctl = true;
    (*itr)->Resume();
    delete (*itr);
    itr = read_queue->erase(itr);
    total_io_events_--;
  }
  itr = write_queue->begin();
  while (itr != write_queue->end()) {
    need_epoll_ctl = true;
    (*itr)->Resume();
    delete (*itr);
    itr = write_queue->erase(itr);
    total_io_events_--;
  }

  if (interesting_fds_.find(target_fd) != interesting_fds_.end()) {
    interesting_fds_.erase(target_fd);
  }

  if (need_epoll_ctl) {
    int epoll_ret = epoll_ctl(fd_, EPOLL_CTL_DEL, target_fd, nullptr);
    if (epoll_ret != 0)
      [[unlikely]] {
        throw arc::exception::IOException(
            "Epoll Error When Deleting All IO Events of FD: " +
            std::to_string(target_fd));
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

    int op = prev_event == 0 ? EPOLL_CTL_ADD
                             : (cur_event == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD);
    epoll_event e_event{};
    e_event.events = cur_event;
    e_event.data.fd = fd;
    int epoll_ret = epoll_ctl(fd_, op, fd, &e_event);
    if (epoll_ret != 0) {
      throw arc::exception::IOException("Epoll Error When Trimming IO Events");
    }
  }
}

void Poller::TrimUserEvents() {
  bool should_add_epoll = !user_events_.empty();
  if (is_event_fd_added_ == should_add_epoll) {
    return;
  }
  int op = should_add_epoll ? EPOLL_CTL_ADD : EPOLL_CTL_DEL;
  epoll_event e_event{};
  e_event.events = EPOLLIN;
  e_event.data.fd = event_fd_;
  int epoll_ret = epoll_ctl(fd_, op, event_fd_, &e_event);
  if (epoll_ret != 0) {
    throw arc::exception::IOException("Epoll Error When Trimming User Events");
  }
  is_event_fd_added_ = should_add_epoll;
}

void Poller::TrimTimeEvents() {
  if (time_events_.empty()) {
    next_wait_timeout_ = -1;  // wait infinitely if no time event
    return;
  }
  auto top_event = time_events_.top();
  std::int64_t current_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          (std::chrono::steady_clock::now()).time_since_epoch())
          .count();
  next_wait_timeout_ =
      std::max((std::int64_t)0, top_event->GetWakeupTime() - current_time);
}

int Poller::Register() {
  dispatch_fd_ = eventfd(0, EFD_NONBLOCK);
  if (dispatch_fd_ < 0) {
    throw arc::exception::IOException("Create dispatch eventfd error");
  }
  epoll_event e_event{};
  e_event.events = EPOLLIN;
  e_event.data.fd = dispatch_fd_;
  int epoll_ret = epoll_ctl(fd_, EPOLL_CTL_ADD, dispatch_fd_, &e_event);
  if (epoll_ret != 0) {
    throw arc::exception::IOException(
        "Epoll error when adding dispatch evetfd");
  }
  return dispatch_fd_;
}

void Poller::DeRegister() {
  if (dispatch_fd_ < 0) {
    return;
  }
  int epoll_ret = epoll_ctl(fd_, EPOLL_CTL_DEL, dispatch_fd_, nullptr);
  if (epoll_ret != 0) {
    throw arc::exception::IOException(
        "Epoll error when removing dispatch evetfd");
  }
  close(dispatch_fd_);
  dispatch_fd_ = -1;
}

events::IOEvent* Poller::PopIOEvent(int fd, io::IOType event_type) {
  arc::events::IOEvent* event = nullptr;
  if (fd < kMaxFdInArray_) {
    event = io_events_[fd][static_cast<int>(event_type)].front();
    io_events_[fd][static_cast<int>(event_type)].pop_front();
  } else {
    event = extra_io_events_[fd][static_cast<int>(event_type)].front();
    extra_io_events_[fd][static_cast<int>(event_type)].pop_front();
  }
  total_io_events_--;
  interesting_fds_.insert(fd);
  return event;
}

int Poller::GetExistingIOEvent(int fd) {
  int cur = 0;
  if (fd < kMaxFdInArray_) {
    if (!io_events_[fd][static_cast<int>(io::IOType::READ)].empty()) {
      cur |= EPOLLIN;
    }
    if (!io_events_[fd][static_cast<int>(io::IOType::WRITE)].empty()) {
      cur |= EPOLLOUT;
    }
  } else {
    if (extra_io_events_.find(fd) != extra_io_events_.end() &&
        !extra_io_events_[fd][static_cast<int>(io::IOType::READ)].empty()) {
      cur |= EPOLLIN;
    }
    if (extra_io_events_.find(fd) != extra_io_events_.end() &&
        !extra_io_events_[fd][static_cast<int>(io::IOType::WRITE)].empty()) {
      cur |= EPOLLOUT;
    }
  }
  return cur;
}
