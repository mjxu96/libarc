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
#include <arc/coro/task.h>
#include <arc/exception/io.h>
#include <sys/eventfd.h>

#include <memory>

std::mutex global_event_loop_lock;
std::unordered_map<std::thread::id, std::shared_ptr<arc::coro::EventLoop>>
    global_event_loops;

using namespace arc::coro;

EventLoop::EventLoop() {
  fd_ = epoll_create1(0);
  if (fd_ < 0) {
    throw arc::exception::IOException();
  }
  eventfd_ = eventfd(0, EFD_NONBLOCK);
  if (eventfd_ < 0) {
    throw arc::exception::IOException("Epoll Eventfd Creation Error");
  }
  // add eventfd into epoll
  epoll_event e_event{};
  e_event.events = EPOLLIN | EPOLLET;
  e_event.data.fd = eventfd_;
  int ret = epoll_ctl(fd_, EPOLL_CTL_ADD, eventfd_, &e_event);
  if (ret < 0) {
    throw arc::exception::IOException("Epoll Eventfd Creation Error");
  }
}

EventLoop::~EventLoop() {
  if (eventfd_ >= 0) {
    epoll_ctl(fd_, EPOLL_CTL_DEL, eventfd_, nullptr);
    close(eventfd_);
  }
}

bool EventLoop::IsDone() {
  std::lock_guard<std::recursive_mutex> guard(eventloop_op_lock_);
  return (to_resume_handles_.empty() && total_added_task_num_ <= 0);
}

void EventLoop::Do() {
  // {
  //   std::lock_guard<std::recursive_mutex> guard(eventloop_op_lock_);
  //   int cnt = to_resume_handles_.size();
  //   for (int i = 0; i < cnt; i++) {
  //     to_resume_handles_[i].resume();
  //   }
  //   for (int i = 0; i < cnt; i++) {
  //     to_resume_handles_.pop_front();
  //   }
  // }

  CleanUpFinishedCoroutines();

  if (total_added_task_num_ <= 0) {
    assert(to_clean_up_handles_.empty());
    return;
  }

  // Then we will handle all others
  int event_cnt = epoll_wait(fd_, events, kMaxEventsSizePerWait_, -1);
  int todo_cnt = 0;
  bool is_resumable_coroutines_added = false;
  for (int i = 0; i < event_cnt; i++) {
    int fd = events[i].data.fd;
    if (fd == eventfd_) {
      is_resumable_coroutines_added = true;
      continue;
    }
    int event_type = events[i].events;
    if (event_type & EPOLLIN) {
      todo_events[todo_cnt] = GetEvent(fd, events::detail::IOEventType::READ);
      todo_cnt++;
    }
    if (event_type & EPOLLOUT) {
      todo_events[todo_cnt] = GetEvent(fd, events::detail::IOEventType::WRITE);
      todo_cnt++;
    }
    if (event_type && ((event_type & EPOLLIN) == 0) &&
        ((event_type & EPOLLOUT) == 0)) {
      throw arc::exception::IOException();
    }
  }

  // currently we enforce this constraint
  // in future we will remove this one
  assert((todo_cnt == event_cnt) ||
         ((todo_cnt + 1 == event_cnt) && is_resumable_coroutines_added));

  for (int i = 0; i < todo_cnt; i++) {
    RemoveIOEvent(todo_events[i]);
    todo_events[i]->Resume();
  }

  for (int i = 0; i < todo_cnt; i++) {
    delete todo_events[i];
  }

  if (is_resumable_coroutines_added) {
    TriggerResumableCoroutines();
  }

  total_added_task_num_ -= todo_cnt;
}

void EventLoop::AddIOEvent(events::detail::IOEventBase* event, bool replace) {
  auto target_fd = event->GetFd();
  events::detail::IOEventType event_type = event->GetIOEventType();
  total_added_task_num_++;
  int should_add_event =
      (event_type == events::detail::IOEventType::READ ? EPOLLIN : EPOLLOUT);
  int prev_events = GetExistIOEvent(target_fd);
  std::deque<arc::events::detail::IOEventBase *>* to_be_added_queue = nullptr;
  if ((prev_events & should_add_event) == 0) {
    if (target_fd < kMaxFdInArray_) {
      to_be_added_queue = &(io_events_[target_fd][static_cast<int>(event_type)]);
    } else {
      if (extra_io_events_.find(target_fd) == extra_io_events_.end()) {
        extra_io_events_[target_fd] =
            std::vector<std::deque<events::detail::IOEventBase*>>{
                2, std::deque<events::detail::IOEventBase*>{}};
      }
      to_be_added_queue = &(extra_io_events_[target_fd][static_cast<int>(event_type)]);
    }

    if (replace) {
      assert(to_be_added_queue->empty());
      for (auto item : *to_be_added_queue) {
        delete item;
      }
      to_be_added_queue->clear();
    }
    to_be_added_queue->push_back(event);

    int op = (prev_events == 0 ? EPOLL_CTL_ADD : EPOLL_CTL_MOD);
    epoll_event e_event{};
    e_event.events = prev_events | should_add_event | EPOLLET;
    e_event.data.fd = target_fd;

    int epoll_ret = epoll_ctl(fd_, op, target_fd, &e_event);
    if (epoll_ret != 0) {
      throw arc::exception::IOException();
    }
  } else {
    // already added
    if (target_fd < kMaxFdInArray_) {
      to_be_added_queue = &(io_events_[target_fd][static_cast<int>(event_type)]);
    } else {
      to_be_added_queue = &(extra_io_events_[target_fd][static_cast<int>(event_type)]);
    }
    if (replace) {
      assert(to_be_added_queue->size() == 1);
      for (auto item : *to_be_added_queue) {
        delete item;
      }
      to_be_added_queue->clear();
    }
    to_be_added_queue->push_back(event);
    return;
  }
}

arc::events::detail::IOEventBase* EventLoop::GetEvent(
    int fd, events::detail::IOEventType event_type) {
  arc::events::detail::IOEventBase* event = nullptr;
  if (fd < kMaxFdInArray_) {
    event = io_events_[fd][static_cast<int>(event_type)].front();
  } else {
    event = extra_io_events_[fd][static_cast<int>(event_type)].front();
  }
  return event;
}

void EventLoop::RemoveIOEvent(events::detail::IOEventBase* event) {
  int target_fd = event->GetFd();
  events::detail::IOEventType event_type = event->GetIOEventType();

  int after_io_events = 0;
  if (target_fd < kMaxFdInArray_) {
    assert(!io_events_[target_fd][static_cast<int>(event_type)].empty());
    assert(io_events_[target_fd][static_cast<int>(event_type)].front() ==
           event);
    io_events_[target_fd][static_cast<int>(event_type)].pop_front();
  } else {
    assert(!extra_io_events_[target_fd][static_cast<int>(event_type)].empty());
    assert(extra_io_events_[target_fd][static_cast<int>(event_type)].front() ==
           event);
    extra_io_events_[target_fd][static_cast<int>(event_type)].pop_front();
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
    throw arc::exception::IOException();
  }
}

int EventLoop::GetExistIOEvent(int fd) {
  int prev = 0;
  if (fd < kMaxFdInArray_) {
    if (!io_events_[fd][static_cast<int>(events::detail::IOEventType::READ)]
             .empty()) {
      prev |= EPOLLIN;
    }
    if (!io_events_[fd][static_cast<int>(events::detail::IOEventType::WRITE)]
             .empty()) {
      prev |= EPOLLOUT;
    }
  } else {
    if (extra_io_events_.find(fd) != extra_io_events_.end() &&
        !extra_io_events_[fd]
                         [static_cast<int>(events::detail::IOEventType::READ)]
                             .empty()) {
      prev |= EPOLLIN;
    }
    if (extra_io_events_.find(fd) != extra_io_events_.end() &&
        !extra_io_events_[fd]
                         [static_cast<int>(events::detail::IOEventType::WRITE)]
                             .empty()) {
      prev |= EPOLLOUT;
    }
  }
  return prev;
}

void EventLoop::AddToCleanUpCoroutine(std::coroutine_handle<> handle) {
  to_clean_up_handles_.push_back(handle);
}

void EventLoop::AddToResumeCoroutine(std::coroutine_handle<void> handle) {
  std::lock_guard<std::recursive_mutex> guard(eventloop_op_lock_);
  to_resume_handles_.push_back(handle);
  uint64_t inc = 1;
  int written = write(eventfd_, &inc, sizeof(inc));
  if (written < 0) {
    throw arc::exception::IOException("Add Resumable Croutine Failure");
  }
}

void EventLoop::TriggerResumableCoroutines() {
  std::lock_guard<std::recursive_mutex> guard(eventloop_op_lock_);
  uint64_t num = 0;
  int read_size = read(eventfd_, &num, sizeof(num));
  if (read_size < 0 || num > to_resume_handles_.size()) {
    throw arc::exception::IOException("Trigger Resumable Coroutines Failure");
  }
  for (int i = 0; i < num; i++) {
    to_resume_handles_[i].resume();
  }
  for (int i = 0; i < num; i++) {
    to_resume_handles_.pop_front();
  }
}

void EventLoop::CleanUpFinishedCoroutines() {
  for (auto& to_clean_up_handle : to_clean_up_handles_) {
    to_clean_up_handle.destroy();
  }
  to_clean_up_handles_.clear();
}

void EventLoop::EnsureFuture(arc::coro::Task<void>&& task) {
  task.Start(true, this, true);
}

void EventLoop::StartLoop(arc::coro::Task<void>&& task) {
  task.Start(true, this, false);
  RunUntilComplelete();
}

void EventLoop::RunUntilComplelete() {
  while (!IsDone()) {
    Do();
  }
  CleanUpFinishedCoroutines();
}

// arc::coro::EventLoop& arc::coro::GetLocalEventLoop() {
//   thread_local EventLoop loop{};
//   return loop;
// }

arc::coro::EventLoop& arc::coro::GetEventLoop(std::thread::id id) {
  std::lock_guard<std::mutex> guard(global_event_loop_lock);
  if (global_event_loops.find(id) == global_event_loops.end()) {
    global_event_loops[id] = std::make_shared<arc::coro::EventLoop>();
  }
  return *global_event_loops[id];
}

std::unordered_map<std::thread::id, std::shared_ptr<arc::coro::EventLoop>>&
arc::coro::GetAllEventLoops() {
  return global_event_loops;
}
