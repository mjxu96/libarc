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

EventLoop::EventLoop() { fd_ = epoll_create1(0); }

bool EventLoop::IsDone() { return total_added_task_num_ <= 0 && coro_events_.empty() && finished_coro_events_.empty(); }

void EventLoop::Do() {
  // First we iterate over unfinished coroutine event
  auto finished_coro_events_itr = finished_coro_events_.begin();
  while (finished_coro_events_itr != finished_coro_events_.end()) {
    (*finished_coro_events_itr)->Resume();
    finished_coro_events_itr = finished_coro_events_.erase(finished_coro_events_itr);
  }

  if (total_added_task_num_ <= 0) {
    return;
  }

  std::cout << "waitingg io event" << std::endl;
  
  // Then we will handle all others
  epoll_event events[kMaxEventsSizePerWait];
  events::detail::IOEventBase* todo_events[kMaxEventsSizePerWait];
  int event_cnt = epoll_wait(fd_, events, kMaxEventsSizePerWait, -1);
  for (int i = 0; i < event_cnt; i++) {
    if ((events[i].events & EPOLLIN) != 0) {
      todo_events[i] = read_events_[events[i].data.fd].front();
      read_events_[events[i].data.fd].pop();
    }
    if ((events[i].events & EPOLLOUT) != 0) {
      todo_events[i] = write_events_[events[i].data.fd].front();
      read_events_[events[i].data.fd].pop();
    }
  }

  std::unordered_set<int> to_delete_read_events;
  std::unordered_set<int> to_delete_write_events;
  for (int i = 0; i < event_cnt; i++) {
    todo_events[i]->Resume();
  }

  // Do cleanup
  for (int i = 0; i < event_cnt; i++) {
    if (todo_events[i]->GetIOEventType() == events::detail::IOEventType::READ) {
      if (read_events_[todo_events[i]->GetFd()].empty()) {
        to_delete_read_events.insert(todo_events[i]->GetFd());
      }
    } else {
      if (write_events_[todo_events[i]->GetFd()].empty()) {
        to_delete_write_events.insert(todo_events[i]->GetFd());
      }
    }
  }

  int epoll_ctl_ret = -1;
  for (int target_fd : to_delete_read_events) {
    if (write_events_.find(target_fd) == write_events_.end() || write_events_[target_fd].empty() ||  
        to_delete_write_events.find(target_fd) != to_delete_write_events.end()) {
      epoll_ctl_ret = epoll_ctl(fd_, EPOLL_CTL_DEL, target_fd, NULL);
    } else {
      epoll_event e_event{};
      e_event.events = EPOLLET | EPOLLOUT;
      e_event.data.fd = target_fd;
      epoll_ctl_ret = epoll_ctl(fd_, EPOLL_CTL_MOD, target_fd, &e_event);
    }
  }

  for (int target_fd : to_delete_write_events) {
    if (read_events_.find(target_fd) == read_events_.end() || read_events_[target_fd].empty() ||  
        to_delete_read_events.find(target_fd) != to_delete_read_events.end()) {
      epoll_ctl_ret = epoll_ctl(fd_, EPOLL_CTL_DEL, target_fd, NULL);
    } else {
      epoll_event e_event{};
      e_event.events = EPOLLET | EPOLLIN;
      e_event.data.fd = target_fd;
      epoll_ctl_ret = epoll_ctl(fd_, EPOLL_CTL_MOD, target_fd, &e_event);
    }
  }

  for (int i = 0; i < event_cnt; i++) {
    delete todo_events[i];
  }

  total_added_task_num_ -= event_cnt;

  assert(epoll_ctl_ret == 0);
}

void EventLoop::AddEvent(events::detail::IOEventBase* event) {
  auto target_fd = event->GetFd();
  events::detail::IOEventType event_type = event->GetIOEventType();

  total_added_task_num_++;
  std::unordered_map<int, std::queue<events::detail::IOEventBase*>>* related_events_ptr =
      nullptr;
  if (event_type == events::detail::IOEventType::READ) {
    related_events_ptr = &read_events_;
  } else {
    related_events_ptr = &write_events_;
  }
  if (related_events_ptr->find(target_fd) == related_events_ptr->end()) {
    (*related_events_ptr)[target_fd] = std::queue<events::detail::IOEventBase*>();
  }
  (*related_events_ptr)[target_fd].push(event);

  if (related_events_ptr->find(target_fd)->second.size() > 1) {
    // This means previously we have an event registered.
    return;
  }

  epoll_event e_event{};
  int epoll_related_events =
      EPOLLET | (event_type == events::detail::IOEventType::READ ? EPOLLIN : EPOLLOUT);
  e_event.events = epoll_related_events;
  e_event.data.fd = target_fd;
  int ret = epoll_ctl(fd_, EPOLL_CTL_ADD, target_fd, &e_event);
  assert(ret == 0);
  if (ret != 0) {
    // TODO add warning here.
    std::cout << errno << std::endl;
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

void EventLoop::Open() {

}

void EventLoop::Close() {

}

EventLoop& arc::coro::GetLocalEventLoop() {
  thread_local EventLoop loop{};
  return loop;
}
