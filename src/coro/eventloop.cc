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
  std::cout << "begin unfinished event" << std::endl;
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

  std::cout << "waitingg io event" << std::endl;

  // Then we will handle all others
  // epoll_event events[kMaxEventsSizePerWait_];
  // events::detail::IOEventBase* todo_events[kMaxEventsSizePerWait_];
  int event_cnt = epoll_wait(fd_, events, kMaxEventsSizePerWait_, -1);
  int todo_cnt = 0;
  std::cout << "after waiting" << std::endl;
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
    if (event_type && (event_type & EPOLLIN == 0) && (event_type & EPOLLOUT == 0)) {
      arc::utils::ThrowErrnoExceptions();
    }

    // std::cout << arc::utils::GetStdBitSet(events[i].data.u64) << std::endl;
    // auto [fd, io_type] = GetFdAndIOTypeFromEpollEvent(events[i].data.u64);
    std::cout << "fd: " << fd << " receive update from io_events: " << arc::utils::GetStdBitSet(event_type) << std::endl;
    // todo_events[i] =
    //     io_events_[io_type][fd].front();

    // if ((events[i].events & EPOLLIN) != 0) {
    //   todo_events[i] = read_events_[events[i].data.fd].front();
    //   read_events_[events[i].data.fd].pop();
    // }
    // if ((events[i].events & EPOLLOUT) != 0) {
    //   todo_events[i] = write_events_[events[i].data.fd].front();
    //   read_events_[events[i].data.fd].pop();
    // }
  }
  // currently we enforce this constraint
  // in future we will remove this one
  assert(todo_cnt == event_cnt);

  // std::unordered_map<io::IOType, std::unordered_set<int>> to_delete_events;
  for (int i = 0; i < todo_cnt; i++) {
    RemoveIOEvent(todo_events[i]);
    todo_events[i]->Resume();
  }

  // Do cleanup
  // for (int i = 0; i < event_cnt; i++) {
  //   if (to_delete_events.find(todo_events[i]->GetIOEventType()) ==
  //       to_delete_events.end()) {
  //     to_delete_events[todo_events[i]->GetIOEventType()] =
  //         std::unordered_set<int>();
  //   }
  //   to_delete_events[todo_events[i]->GetIOEventType()].insert(
  //       todo_events[i]->GetFd());
  //   // if (todo_events[i]->GetIOEventType() == io::IOType::READ) {
  //   //   if (read_events_[todo_events[i]->GetFd()].empty()) {
  //   //     to_delete_read_events.insert(todo_events[i]->GetFd());
  //   //   }
  //   // } else {
  //   //   if (write_events_[todo_events[i]->GetFd()].empty()) {
  //   //     to_delete_write_events.insert(todo_events[i]->GetFd());
  //   //   }
  //   // }
  // }

  // int epoll_ctl_ret = -1;
  // for (int target_fd : to_delete_read_events) {
  //   if (write_events_.find(target_fd) == write_events_.end() ||
  //       write_events_[target_fd].empty() ||
  //       to_delete_write_events.find(target_fd) !=
  //           to_delete_write_events.end()) {
  //     std::cout << "remove fd: " << target_fd << " from epoll" << std::endl;
  //     epoll_ctl_ret = epoll_ctl(fd_, EPOLL_CTL_DEL, target_fd, NULL);
  //     if (epoll_ctl_ret != 0) {
  //       arc::utils::ThrowErrnoExceptions();
  //     }
  //   } else {
  //     epoll_event e_event{};
  //     e_event.events = EPOLLET | EPOLLOUT;
  //     e_event.data.fd = target_fd;
  //     std::cout << "modify fd: " << target_fd << " to EPOLLOUT" << std::endl;
  //     epoll_ctl_ret = epoll_ctl(fd_, EPOLL_CTL_MOD, target_fd, &e_event);
  //     if (epoll_ctl_ret != 0) {
  //       arc::utils::ThrowErrnoExceptions();
  //     }
  //   }
  // }

  for (int i = 0; i < todo_cnt; i++) {
  }

  // for (int target_fd : to_delete_write_events) {
  //   if (read_events_.find(target_fd) == read_events_.end() ||
  //       read_events_[target_fd].empty() ||
  //       to_delete_read_events.find(target_fd) != to_delete_read_events.end()) {
  //     std::cout << "remove fd: " << target_fd << " from epoll" << std::endl;
  //     epoll_ctl_ret = epoll_ctl(fd_, EPOLL_CTL_DEL, target_fd, NULL);
  //     if (epoll_ctl_ret != 0) {
  //       arc::utils::ThrowErrnoExceptions();
  //     }
  //   } else {
  //     epoll_event e_event{};
  //     e_event.events = EPOLLET | EPOLLIN;
  //     e_event.data.fd = target_fd;
  //     std::cout << "modify fd: " << target_fd << " to EPOLLIN" << std::endl;
  //     epoll_ctl_ret = epoll_ctl(fd_, EPOLL_CTL_MOD, target_fd, &e_event);
  //     if (epoll_ctl_ret != 0) {
  //       arc::utils::ThrowErrnoExceptions();
  //     }
  //   }
  // }

  for (int i = 0; i < todo_cnt; i++) {
    delete todo_events[i];
  }

  total_added_task_num_ -= todo_cnt;
}

void EventLoop::AddIOEvent(events::detail::IOEventBase* event) {
  auto target_fd = event->GetFd();
  events::detail::IOEventType event_type = event->GetIOEventType();
  std::cout << "in add for fd: " << target_fd << " for type: " << (int)event_type << std::endl;

  total_added_task_num_++;
  // std::unordered_map<int, std::queue<events::detail::IOEventBase*>>*
  //     related_events_ptr = nullptr;
  // std::unordered_map<int, std::queue<events::detail::IOEventBase*>>*
  //     other_events_ptr = nullptr;
  // if (event_type == io::IOType::READ) {
  //   related_events_ptr = &read_events_;
  //   other_events_ptr = &write_events_;
  // } else {
  //   related_events_ptr = &write_events_;
  //   other_events_ptr = &read_events_;
  // }
  // if (related_events_ptr->find(target_fd) == related_events_ptr->end()) {
  //   (*related_events_ptr)[target_fd] =
  //       std::queue<events::detail::IOEventBase*>();
  // } else {
  //   prev_events |= (event_type == io::IOType::READ ? EPOLLIN : EPOLLOUT);
  // }
  // (*related_events_ptr)[target_fd].push(event);
  // if (other_events_ptr->find(target_fd) != other_events_ptr->end()) {
  //   prev_events |= (event_type == io::IOType::READ ? EPOLLOUT : EPOLLIN);
  // }

  // if (prev_events == (event_type == io::IOType::READ ? EPOLLIN : EPOLLOUT)) {
  //   // This means previously we have an event registered.
  //   return;
  // }
  int should_add_event = (event_type == events::detail::IOEventType::READ ? EPOLLIN : EPOLLOUT);
  int prev_events = GetExistIOEvent(target_fd);
  if ((prev_events & should_add_event) == 0) {
    if (target_fd < kMaxFdInArray_) {
      assert(!io_events_[target_fd][static_cast<int>(event_type)]);
      io_events_[target_fd][static_cast<int>(event_type)] = event;
    } else {
      assert(extra_io_events_.find(target_fd) == extra_io_events_.end() || !extra_io_events_[target_fd][static_cast<int>(event_type)]);
      if (extra_io_events_.find(target_fd) == extra_io_events_.end()) {
        extra_io_events_[target_fd] = std::vector<events::detail::IOEventBase*>{2, nullptr};
      }
      extra_io_events_[target_fd][static_cast<int>(event_type)] = event;
    }

    int op = (prev_events == 0 ? EPOLL_CTL_ADD : EPOLL_CTL_MOD);
    epoll_event e_event{};
    e_event.events = prev_events | should_add_event | EPOLLET;
    e_event.data.fd = target_fd;

    std::cout << "epoll_ctl op: " << op << " fd: " << target_fd << " events: " << std::bitset<sizeof(e_event.events) * 8U>(e_event.events) << std::endl;
    int epoll_ret = epoll_ctl(fd_, op, target_fd, &e_event);
    if (epoll_ret != 0) {
      arc::utils::ThrowErrnoExceptions();
    }
  } else {
    // already added
    if (target_fd < kMaxFdInArray_) {
      assert(event->GetIOType() == io::IOType::ACCEPT || io_events_[target_fd][static_cast<int>(event_type)] != event);
      io_events_[target_fd][static_cast<int>(event_type)] = event;
    } else {
      assert(event->GetIOType() == io::IOType::ACCEPT || extra_io_events_[target_fd][static_cast<int>(event_type)] != event);
      extra_io_events_[target_fd][static_cast<int>(event_type)] = event;
    }
    return;
  }

  // if (io_events_[static_cast<int>(event_type)].find(target_fd) == io_events_[static_cast<int>(event_type)].end()) {
  //   io_events_[static_cast<int>(event_type)][target_fd] = std::list<events::detail::IOEventBase*>();
  // } else {
  //   prev_events |= 
  // }
  // io_events_[static_cast<int>(event_type)][target_fd].push_back(event);

  // int other_events_of_this_fd = IsFdExistInOtherTypeTodoEvents(target_fd, event_type);
  // int modified_flags = GetEpollFlagFromIOType(event_type);
  // bool is_add = true;
  // for (auto& [io_type, events] : io_events_) {
  //   if (arc::utils::GetBit(other_events_of_this_fd, (int)io_type)) {
  //     modified_flags |= GetEpollFlagFromIOType(io_type);
  //     is_add = false;
  //   }
  // }

  // epoll_event e_event{};
  // e_event.events = modified_flags;
  // e_event.data.u64 = (((decltype(e_event.data.u64))event_type) << sizeof(decltype(e_event.data.u64)) * 4UL) + target_fd;
  //   std::cout << arc::utils::GetStdBitSet(e_event.data.u64) << std::endl;
  // int ret = -1;
  // if (is_add) {
  // std::cout << "add fd: " << target_fd << " with event: " << (int)event_type
  //           << std::endl;
  //   ret = epoll_ctl(fd_, EPOLL_CTL_ADD, target_fd, &e_event);
  // } else {
  // std::cout << "mod add fd: " << target_fd << " with event: " << (int)event_type
  //           << std::endl;
  //   ret = epoll_ctl(fd_, EPOLL_CTL_MOD, target_fd, &e_event);
  // }
  // assert(ret == 0);
  // if (ret != 0) {
  //   // TODO add warning here.
  //   arc::utils::ThrowErrnoExceptions();
  // }
}

arc::events::detail::IOEventBase* EventLoop::GetEvent(int fd, events::detail::IOEventType event_type) {
  if (fd < kMaxFdInArray_) {
    return io_events_[fd][static_cast<int>(event_type)];
  }
  return extra_io_events_[fd][static_cast<int>(event_type)];
}

void EventLoop::RemoveIOEvent(int fd, io::IOType io_type, bool forced) {
  events::detail::IOEventType event_type = (io_type == io::IOType::READ || io_type == io::IOType::ACCEPT ? events::detail::IOEventType::READ : events::detail::IOEventType::WRITE);
  events::detail::IOEventBase* event = GetEvent(fd, event_type);
  RemoveIOEvent(event, forced);
}

void EventLoop::RemoveIOEvent(events::detail::IOEventBase* event, bool forced) {
  if (!event->ShouldRemoveEveryTime() && !forced) {
    std::cout << "not remove" << std::endl;
    return;
  }
  std::cout << "in remove" << std::endl;
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
    std::cout << "remove fd: " << target_fd << " from epoll" << std::endl;
    epoll_ret = epoll_ctl(fd_, op, target_fd, nullptr);
  } else {
    epoll_event e_event{};
    e_event.events = after_io_events | EPOLLET;
    e_event.data.fd = target_fd;
    std::cout << "change fd: " << target_fd << "'s event to " << std::bitset<sizeof(e_event.events) * 8U>(e_event.events) << " delete event: " << (int)event_type << std::endl;
    epoll_ret = epoll_ctl(fd_, op, target_fd, &e_event);
  }
  if (epoll_ret != 0) {
    arc::utils::ThrowErrnoExceptions();
  }


  // std::list<events::detail::IOEventBase*>& list = io_events_[type][fd];
  // auto itr = list.begin();
  // for (; itr != list.end(); itr++) {
  //   if (*itr == event) {
  //     list.erase(itr);
  //     break;
  //   }
  // }
  // if (list.empty()) {
  //   io_events_[type].erase(fd);
  // }
  // assert(itr != list.end());

  // int other_events_of_this_fd = IsFdExistInOtherTypeTodoEvents(fd, type);
  // int modified_flags = 0;
  // for (auto& [io_type, events] : io_events_) {
  //   if (arc::utils::GetBit(other_events_of_this_fd, (int)io_type)) {
  //     modified_flags |= GetEpollFlagFromIOType(io_type);
  //   }
  // }
  // int ret = -1;
  // if (modified_flags == 0) {
  //   std::cout << "remove fd: " << fd << " from epoll" << std::endl;
  //   ret = epoll_ctl(fd_, EPOLL_CTL_DEL, fd, NULL);
  // } else {
  //   epoll_event e_event;
  //   e_event.events = modified_flags;
  //   e_event.data.u64 = (((decltype(e_event.data.u64))type) << sizeof(decltype(e_event.data.u64)) * 4UL) + fd;;
  //   std::cout << arc::utils::GetStdBitSet(e_event.data.u64) << std::endl;
  //   std::cout << "modify fd: " << fd << " to " << arc::utils::GetStdBitSet(modified_flags) << std::endl;
  //   ret = epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &e_event);
  // }
  // assert(ret == 0);
  // if (ret != 0) {
  //   arc::utils::ThrowErrnoExceptions();
  // }
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

// int EventLoop::IsFdExistInOtherTypeTodoEvents(int fd,
//                                               io::IOType exclude_event) {
//   int ret = 0;
//   for (auto& [io_type, io_events] : io_events_) {
//     if (io_type == exclude_event) {
//       continue;
//     }
//     if (io_events.find(fd) != io_events.end() && !io_events[fd].empty()) {
//       ret = arc::utils::BitSet(ret, (int)io_type, 1);
//     }
//   }
//   return ret;
// }

// int EventLoop::GetEpollFlagFromIOType(io::IOType type) {
//   switch (type) {
//     case io::IOType::READ:
//       return EPOLLIN | EPOLLET;
//     case io::IOType::WRITE:
//       return EPOLLOUT | EPOLLET;
//     case io::IOType::ACCEPT:
//       return EPOLLIN;
//     case io::IOType::CONNECT:
//       return EPOLLOUT | EPOLLET;
//     default:
//       return EPOLLET;
//   }
// }

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
    if (extra_io_events_.find(fd) != extra_io_events_.end() && extra_io_events_[fd][static_cast<int>(events::detail::IOEventType::READ)]) {
      prev |= EPOLLIN;
    }
    if (extra_io_events_.find(fd) != extra_io_events_.end() && extra_io_events_[fd][static_cast<int>(events::detail::IOEventType::WRITE)]) {
      prev |= EPOLLOUT;
    }
  }
  return prev;
}

// std::pair<int, arc::io::IOType> EventLoop::GetFdAndIOTypeFromEpollEvent(uint64_t u) {
//   constexpr int half_bit_count = sizeof(decltype(u)) * 4UL;
//   auto lower = arc::utils::GetLowerBits(u, half_bit_count);
//   auto higher = arc::utils::GetLowerBits(u >> half_bit_count, half_bit_count);
//   return {(int)lower, (arc::io::IOType)higher};
// }

arc::coro::EventLoop& arc::coro::GetLocalEventLoop() {
  thread_local EventLoop loop{};
  return loop;
}
