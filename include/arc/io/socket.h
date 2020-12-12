/*
 * File: socket.h
 * Project: libarc
 * File Created: Saturday, 12th December 2020 7:26:15 pm
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

#ifndef LIBARC__IO__SOCKET_H
#define LIBARC__IO__SOCKET_H

#include <arc/net/address.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <iostream>

#include "io_base.h"

namespace arc {
namespace io {

namespace detail {

template <net::Domain AF = net::Domain::IPV4,
          net::SocketType ST = net::SocketType::STREAM,
          net::Protocol P = net::Protocol::AUTO>
class SocketBase : public IOBase {
 public:
  SocketBase() {
    fd_ =
        socket(static_cast<int>(AF), static_cast<int>(ST), static_cast<int>(P));
    if (fd_ < 0) {
      throw std::system_error(errno, std::generic_category());
    }
    buffer_ = new char[kRecvBufferSize_];
  }

  SocketBase(int fd, const net::Address<AF>& in_addr) {
    fd_ = fd;
    addr_ = in_addr;
    buffer_ = new char[kRecvBufferSize_];
  }

  ~SocketBase() { delete[] buffer_; }

  void Bind(const net::Address<AF>& addr) {
    addr_ = addr;
    if (bind(this->fd_, addr_.GetCStyleAddress(), addr_.AddressSize()) < 0) {
      throw std::system_error(errno, std::generic_category());
    }
  }

 protected:
  net::Address<AF> addr_{};
  const int kRecvBufferSize_{1024};
  char* buffer_{nullptr};

  template <net::Domain UAF>
  int InternalSendTo(const std::string& data, const net::Address<UAF>* addr) {
    int sent = sendto(this->fd_, data.c_str(), data.size(), 0,
                      (addr ? addr->GetCStyleAddress() : nullptr),
                      (addr ? addr->AddressSize() : 0));
    if (sent < 0) {
      throw std::system_error(errno, std::generic_category());
    }
    return sent;
  }

  template <net::Domain UAF>
  std::string InternalRecvFrom(int max_recv_bytes, net::Address<UAF>* addr) {
    std::string buffer;
    max_recv_bytes = (max_recv_bytes < 0
                          ? std::numeric_limits<decltype(max_recv_bytes)>::max()
                          : max_recv_bytes);
    int left_size = max_recv_bytes;
    while (left_size > 0) {
      int this_read_size = std::min(left_size, kRecvBufferSize_);
      ssize_t tmp_read =
          recvfrom(this->fd_, buffer_, this_read_size, 0, nullptr, nullptr);
      buffer.append(buffer_, tmp_read);
      if (tmp_read == -1) {
        throw std::system_error(errno, std::generic_category());
      } else if (tmp_read < kRecvBufferSize_) {
        // we read all contents;
        break;
      }
      left_size -= tmp_read;
    }
    return buffer;
  }
};

}  // namespace detail

template <net::Domain AF = net::Domain::IPV4,
          net::SocketType ST = net::SocketType::STREAM,
          net::Protocol P = net::Protocol::AUTO>
class Socket : public detail::SocketBase<AF, ST, P> {
 public:
  Socket() : detail::SocketBase<AF, ST, P>() {}
  Socket(int fd, const net::Address<AF>& in_addr)
      : detail::SocketBase<AF, ST, P>(fd, in_addr) {}
  ~Socket() = default;
};

// TCP specilization
template <net::Domain AF, net::Protocol P>
class Socket<AF, net::SocketType::STREAM, P>
    : public detail::SocketBase<AF, net::SocketType::STREAM, P> {
 public:
  Socket() : detail::SocketBase<AF, net::SocketType::STREAM, P>() {}
  Socket(int fd, const net::Address<AF>& in_addr)
      : detail::SocketBase<AF, net::SocketType::STREAM, P>(fd, in_addr) {}

  int Send(const std::string& data) {
    return this->template InternalSendTo<AF>(data, nullptr);
  }

  std::string Recv(int max_recv_bytes = -1) {
    return this->template InternalRecvFrom<AF>(max_recv_bytes, nullptr);
  }

  void Listen(int max_pending_connections = 1024) {
    if (listen(this->fd_, max_pending_connections) < 0) {
      throw std::system_error(errno, std::generic_category());
    }
  }

  void Connect(const net::Address<AF>& addr) {
    int res = connect(this->fd_, addr.GetCStyleAddress(), addr.AddressSize());
    if (res < 0) {
      throw std::system_error(errno, std::generic_category());
    }
  }

  Socket Accept() {
    sockaddr_in in_addr;
    int addrlen = sizeof(in_addr);
    int accept_fd =
        accept(this->fd_, (struct sockaddr*)&in_addr, (socklen_t*)&addrlen);
    if (accept_fd < 0) {
      throw std::system_error(errno, std::generic_category());
    }
    return Socket<AF, net::SocketType::STREAM, P>(accept_fd, in_addr);
  }
};

// UDP specilization
template <net::Domain AF, net::Protocol P>
class Socket<AF, net::SocketType::DATAGRAM, P>
    : public detail::SocketBase<AF, net::SocketType::STREAM, P> {
 public:
  int SendTo(const std::string& data, const net::Address<AF>& addr);
  std::pair<net::Address<AF>, std::string> RecvFrom(int max_recv_bytes = -1);
};

}  // namespace io
}  // namespace arc

#endif /* LIBARC__IO__SOCKET_H */
