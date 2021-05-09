/*
 * File: socket_bash.h
 * Project: libarc
 * File Created: Sunday, 9th May 2021 8:57:22 am
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

#ifndef LIBARC__IO__SOCKET_BASE_H
#define LIBARC__IO__SOCKET_BASE_H

#include <arc/concept/io.h>
#include <arc/coro/awaiter/io_awaiter.h>
#include <arc/coro/awaiter/tls_io_awaiter.h>
#include <arc/coro/eventloop.h>
#include <arc/coro/events/io_event_base.h>
#include <arc/coro/task.h>
#include <arc/exception/io.h>
#include <arc/io/ssl.h>
#include <arc/io/utils.h>
#include <arc/net/address.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <functional>
#include <iostream>

#include "io_base.h"

#define RECV_BUFFER_SIZE 1024

namespace arc {
namespace io {

namespace detail {

template <net::Domain AF = net::Domain::IPV4,
          net::SocketType ST = net::SocketType::STREAM,
          net::Protocol P = net::Protocol::AUTO>
class SocketBase : public IOBase {
 public:
  SocketBase() {
    if constexpr (P == net::Protocol::AUTO) {
      fd_ = socket(static_cast<int>(AF), static_cast<int>(ST),
                   static_cast<int>(P));
    } else {
      protoent* protocol_info =
          getprotobyname(nameof::nameof_enum<P>().data());
      if (protocol_info) {
        fd_ = socket(static_cast<int>(AF), static_cast<int>(ST),
                     protocol_info->p_proto);
      } else {
        fd_ = socket(static_cast<int>(AF), static_cast<int>(ST), 0);
      }
    }
    if (fd_ < 0) {
      throw arc::exception::IOException();
    }
  }

  SocketBase(int fd, const net::Address<AF>& in_addr) {
    fd_ = fd;
    addr_ = in_addr;
  }

  SocketBase(const SocketBase&) = delete;
  SocketBase& operator=(const SocketBase&) = delete;

  SocketBase(SocketBase&& other) : IOBase(std::move(other)) {
    MoveFrom(std::move(other));
  }

  SocketBase& operator=(SocketBase&& other) {
    IOBase::MoveFrom(std::move(other));
    MoveFrom(std::move(other));
    return *this;
  }

  virtual ~SocketBase() {}

  void Bind(const net::Address<AF>& addr) {
    addr_ = addr;
    if (bind(this->fd_, addr_.GetCStyleAddress(), addr_.AddressSize()) < 0) {
      throw arc::exception::IOException();
    }
    is_bound_ = true;
  }

  void SetOption(arc::net::SocketOption option_name, int opt_value) {
    if (setsockopt(fd_, (int)(arc::net::SocketLevel::SOCKET), (int)option_name,
                   &opt_value, sizeof(opt_value)) < 0) {
      throw arc::exception::IOException();
    }
  }

  void SetNonBlocking(bool is_enabled = true) {
    int flags = fcntl(fd_, F_GETFL);
    if (flags < 0) {
      throw arc::exception::IOException();
    }
    if (is_enabled) {
      flags = fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    } else {
      flags = fcntl(fd_, F_SETFL, flags & (~O_NONBLOCK));
    }
    if (flags < 0) {
      throw arc::exception::IOException();
    }
    is_non_blocking_ = is_enabled;
  }

  net::Address<AF> GetAddr() const { return addr_; }

 protected:
  using CAddressType =
      typename std::conditional_t<(AF == arc::net::Domain::IPV4), sockaddr_in,
                                  sockaddr_in6>;

  net::Address<AF> addr_{};
  char buffer_[RECV_BUFFER_SIZE] = {0};
  bool is_non_blocking_{false};
  bool is_bound_{false};

  template <net::Protocol UP = P> requires (UP != net::Protocol::UDP)
  ssize_t Send(const void* data, int num, int flags = 0) {
    ssize_t sent = send(this->fd_, data, num, flags);
    if (sent < 0 && (errno != EAGAIN || errno != EWOULDBLOCK)) {
      throw arc::exception::IOException();
    }
    return sent;
  }

  template <net::Protocol UP = P> requires (UP != net::Protocol::UDP)
  ssize_t Write(const void* data, int num) {
    return Send<UP>(data, num);
  }

  template <net::Domain UAF, net::Protocol UP = P> requires (UP == net::Protocol::UDP)
  ssize_t SendTo(const void* data, int num, const net::Address<UAF>* addr) {
    ssize_t sent = sendto(this->fd_, data, num, 0,
                      (addr ? addr->GetCStyleAddress() : nullptr),
                      (addr ? addr->AddressSize() : 0));
    if (sent < 0 && (errno != EAGAIN || errno != EWOULDBLOCK)) {
      throw arc::exception::IOException();
    }
    return sent;
  }

  template <net::Protocol UP = P> requires (UP != net::Protocol::UDP)
  std::string RecvAll(int max_recv_bytes) {
    std::string buffer;
    max_recv_bytes = (max_recv_bytes < 0
                          ? std::numeric_limits<decltype(max_recv_bytes)>::max()
                          : max_recv_bytes);
    int left_size = max_recv_bytes;
    while (left_size > 0) {
      int this_read_size = std::min(left_size, RECV_BUFFER_SIZE);
      ssize_t tmp_read =
          recv(this->fd_, buffer_, this_read_size, 0);
      if (tmp_read > 0) {
        buffer.append(buffer_, tmp_read);
      }
      if (tmp_read == -1) {
        if (errno != EAGAIN || errno != EWOULDBLOCK) {
          throw arc::exception::IOException();
        }
        break;
      } else if (tmp_read < RECV_BUFFER_SIZE) {
        // we read all contents;
        break;
      }
      left_size -= tmp_read;
    }
    return buffer;
  }

  template <net::Protocol UP = P> requires (UP != net::Protocol::UDP)
  ssize_t Recv(char* buf, int max_recv_bytes, int flags = 0) {
    ssize_t ret = recv(this->fd_, buf, max_recv_bytes, flags);
    if (ret < 0 && (errno != EAGAIN || errno != EWOULDBLOCK)) {
      throw arc::exception::IOException();
    }
    return ret;
  }
  template <net::Protocol UP = P> requires (UP != net::Protocol::UDP)
  ssize_t Read(char* buf, int max_recv_bytes) {
    return Recv<UP>(buf, max_recv_bytes, 0);
  }

  template <net::Domain UAF, net::Protocol UP = P> requires (UP == net::Protocol::UDP)
  ssize_t RecvFrom(char* buf, int max_recv_bytes, net::Address<UAF>* addr) {
      sockaddr in_addr;
      socklen_t in_addr_len;
      ssize_t tmp_read = recvfrom(this->fd_, buf, max_recv_bytes, 0,
                                  &in_addr, &in_addr_len);
      if (tmp_read == -1 && (errno != EAGAIN || errno != EWOULDBLOCK)) {
        throw arc::exception::IOException();
      }
      (*addr) = *((CAddressType*)(&in_addr));
      return tmp_read;
  }

 private:
  void MoveFrom(SocketBase&& other) {
    std::memset(buffer_, 0, RECV_BUFFER_SIZE);
    std::memcpy(buffer_, other.buffer_, RECV_BUFFER_SIZE);
    addr_ = std::move(other.addr_);
    is_non_blocking_ = other.is_non_blocking_;
  }
};

}  // namespace detail
}
}


#endif