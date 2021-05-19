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

#include <iostream>

using namespace arc;
using namespace arc::coro;

Poller::Poller() {
  fd_ = epoll_create1(0);
  if (fd_ < 0) {
    throw arc::exception::IOException("Epoll Creation Error");
  }
  user_event_fd_ = eventfd(0, EFD_NONBLOCK);
  if (user_event_fd_ < 0) {
    throw arc::exception::IOException("EventFd Creation Error");
  }
  interesting_fds_.reserve(kMaxFdInArray_);
  std::fill(std::begin(io_prev_events_), std::end(io_prev_events_), 0);
}

Poller::~Poller() {
  if (user_event_fd_ >= 0) {
    close(user_event_fd_);
  }
}

int Poller::WaitEvents(coro::EventBase** todo_events) {
  int event_cnt =
      epoll_wait(fd_, events_, kMaxEventsSizePerWait, next_wait_timeout_);
  int todo_cnt = 0;

  bool is_user_event_triggered = false;
  bool is_dispatched_triggered = false;

  // io events
  for (int i = 0; i < event_cnt; i++) {
    int fd = events_[i].data.fd;
    if (fd == user_event_fd_) {
      is_user_event_triggered = true;
      assert(events_[i].events & EPOLLIN);
      continue;
    }
    int event_type = events_[i].events;
    if (event_type & EPOLLIN) {
      todo_events[todo_cnt] = PopIOEvent(fd, io::IOType::READ);
      self_triggered_event_ids_[todo_cnt] = todo_events[todo_cnt]->GetEventID();
      todo_cnt++;
    }
    if (event_type & EPOLLOUT) {
      todo_events[todo_cnt] = PopIOEvent(fd, io::IOType::WRITE);
      self_triggered_event_ids_[todo_cnt] = todo_events[todo_cnt]->GetEventID();
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

  if (!is_user_event_triggered) {
    std::lock_guard guard(user_event_lock_);
    RemoveCancellationEvent(todo_cnt);
  } else {
    // user events or dispatched events
    std::lock_guard guard(user_event_lock_);

    std::uint64_t event_read = 0;
    int read_bytes = read(user_event_fd_, &event_read, sizeof(event_read));
    if (read_bytes != sizeof(event_read)) {
      throw arc::exception::IOException(
          "Read user event or dispatched event error");
    }

    // check triggered user event
    bool need_to_write_again = false;
    auto triggered_event_itr = triggered_user_events_.begin();
    while (triggered_event_itr != triggered_user_events_.end()) {
      if (todo_cnt < kMaxEventsSizePerWait) {
        todo_events[todo_cnt] = *triggered_event_itr;
        self_triggered_event_ids_[todo_cnt] =
            todo_events[todo_cnt]->GetEventID();
        todo_cnt++;
        triggered_event_itr = triggered_user_events_.erase(triggered_event_itr);
      } else {
        need_to_write_again = true;
        break;
      }
    }

    // remove triggered cancellation events
    RemoveCancellationEvent(todo_cnt);

    // check triggered cancellation event
    auto triggered_cancellation_itr = triggered_cancellation_events_.begin();
    while (triggered_cancellation_itr != triggered_cancellation_events_.end()) {
      if (todo_cnt < kMaxEventsSizePerWait) {
        auto triggered_bind_event =
            PopCancelledEvent(*triggered_cancellation_itr);
        if (triggered_bind_event) {
          todo_events[todo_cnt] = triggered_bind_event;
          todo_cnt++;
        }
      } else {
        need_to_write_again = true;
        break;
      }
      delete *triggered_cancellation_itr;
      triggered_cancellation_itr =
          triggered_cancellation_events_.erase(triggered_cancellation_itr);
    }

    // re-write again if todo count supercede the max allowed events
    if (need_to_write_again) {
      // need to do it in the next wait routine
      event_read = 1;
      int wrote = write(user_event_fd_, &event_read, sizeof(event_read));
      if (wrote != sizeof(event_read)) {
        throw arc::exception::IOException(
            "Write user event or dispatched event in epoll wait error");
      }
    }
  }

  return todo_cnt;
}

void Poller::AddIOEvent(coro::IOEvent* event) {
  event->SetEventID(max_event_id_.fetch_add(1, std::memory_order::relaxed));
  auto target_fd = event->GetFd();
  io::IOType event_type = event->GetIOType();

  total_io_events_++;
  int should_add_event = (event_type == io::IOType::READ ? EPOLLIN : EPOLLOUT);

  std::deque<arc::coro::IOEvent*>* to_be_pushed_queue = nullptr;
  if (target_fd < kMaxFdInArray_) [[likely]] {
    to_be_pushed_queue = &io_events_[target_fd][static_cast<int>(event_type)];
  } else [[unlikely]] {
    if (extra_io_events_.find(target_fd) == extra_io_events_.end()) {
      extra_io_events_[target_fd] = std::vector<std::deque<coro::IOEvent*>>{
          2, std::deque<coro::IOEvent*>{}};
    }
    to_be_pushed_queue =
        &extra_io_events_[target_fd][static_cast<int>(event_type)];
  }

  interesting_fds_.insert(target_fd);
  to_be_pushed_queue->push_back(event);
}

void Poller::AddTimeEvent(coro::TimeEvent* event) {
  event->SetEventID(max_event_id_.fetch_add(1, std::memory_order::relaxed));
  time_events_.push(event);
}

void Poller::AddUserEvent(coro::UserEvent* event) {
  std::lock_guard guard(user_event_lock_);
  event->SetEventID(max_event_id_.fetch_add(1, std::memory_order::relaxed));
  pending_user_events_.push_back(event);
  event->SetIterator(std::prev(pending_user_events_.end()));
}

void Poller::AddCancellationEvent(coro::CancellationEvent* event) {
  std::lock_guard guard(user_event_lock_);
  pending_cancellation_events_.push_back(event);
  auto itr = std::prev(pending_cancellation_events_.end());
  event_pending_cancallation_token_map_[event->GetBindEventID()] = itr;
  event->SetIterator(itr);
}

void Poller::RemoveAllIOEvents(int target_fd) {
  bool need_epoll_ctl = false;

  std::deque<arc::coro::IOEvent*>* read_queue = nullptr;
  std::deque<arc::coro::IOEvent*>* write_queue = nullptr;
  if (target_fd < kMaxFdInArray_) [[likely]] {
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
  }
  auto itr = read_queue->begin();
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
    if (epoll_ret != 0) [[unlikely]] {
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
  std::lock_guard guard(user_event_lock_);
  bool should_add_epoll = !pending_user_events_.empty() ||
                          !triggered_user_events_.empty() ||
                          is_dispatcher_registered_;
  if (is_event_fd_added_ == should_add_epoll) {
    return;
  }
  int op = should_add_epoll ? EPOLL_CTL_ADD : EPOLL_CTL_DEL;
  epoll_event e_event{};
  e_event.events = EPOLLIN;
  e_event.data.fd = user_event_fd_;
  int epoll_ret = epoll_ctl(fd_, op, user_event_fd_, &e_event);
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

void Poller::TriggerUserEvent(coro::UserEvent* event) {
  std::lock_guard guard(user_event_lock_);
  pending_user_events_.erase(event->GetIterator());
  triggered_user_events_.push_back(event);
}

void Poller::TriggerCancellationEvent(int bind_event_id,
                                      coro::CancellationEvent* event) {
  std::lock_guard guard(user_event_lock_);
  if (event_pending_cancallation_token_map_.find(bind_event_id) !=
      event_pending_cancallation_token_map_.end()) {
    pending_cancellation_events_.erase(event->GetIterator());
    triggered_cancellation_events_.push_back(event);
    event_pending_cancallation_token_map_.erase(bind_event_id);
  }
}

int Poller::Register() {
  std::lock_guard guard(user_event_lock_);
  is_dispatcher_registered_ = true;
  return user_event_fd_;
}

void Poller::DeRegister() {
  std::lock_guard guard(user_event_lock_);
  is_dispatcher_registered_ = false;
}

coro::IOEvent* Poller::PopIOEvent(int fd, io::IOType event_type) {
  arc::coro::IOEvent* event = nullptr;
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

EventBase* Poller::PopCancelledEvent(coro::CancellationEvent* event) {
  switch (event->GetType()) {
    case detail::CancellationType::IO_EVENT: {
      int fd = static_cast<int>(event->GetBindHelper());
      if (fd < kMaxFdInArray_) {
        for (auto& vec : io_events_[fd]) {
          for (auto itr = vec.begin(); itr != vec.end(); itr++) {
            if ((*itr)->GetEventID() == event->GetBindEventID()) {
              assert(event->GetEvent() == (*itr));
              interesting_fds_.insert(fd);
              vec.erase(itr);
              return *itr;
            }
          }
        }
      } else {
        for (auto& vec : extra_io_events_[fd]) {
          for (auto itr = vec.begin(); itr != vec.end(); itr++) {
            if ((*itr)->GetEventID() == event->GetBindEventID()) {
              assert(event->GetEvent() == (*itr));
              interesting_fds_.insert(fd);
              vec.erase(itr);
              return *itr;
            }
          }
        }
      }
      break;
    }
    case detail::CancellationType::USER_EVENT: {
      for (auto itr = pending_user_events_.begin();
           itr != pending_user_events_.end(); itr++) {
        if ((*itr)->GetEventID() == event->GetBindEventID()) {
          assert(event->GetEvent() == *itr);
          pending_user_events_.erase(itr);
          return event->GetEvent();
        }
      }
      break;
    }
    case detail::CancellationType::TIME_EVENT: {
      throw arc::exception::detail::ExceptionBase(
          "Cancelling a time event is not allowed");
      break;
    }
    default:
      throw arc::exception::detail::ExceptionBase(
          "Cancellation Event Type not Support!");
      break;
  }
  return nullptr;
}

void Poller::RemoveCancellationEvent(int count) {
  for (int i = 0; i < count && !event_pending_cancallation_token_map_.empty();
       i++) {
    int event_id = self_triggered_event_ids_[i];
    if (event_pending_cancallation_token_map_.find(event_id) !=
        event_pending_cancallation_token_map_.end()) {
      auto itr = event_pending_cancallation_token_map_[event_id];
      delete *itr;
      event_pending_cancallation_token_map_.erase(event_id);
      pending_cancellation_events_.erase(itr);
    }
  }
}
