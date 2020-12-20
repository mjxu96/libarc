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

#include <arc/coro/eventloop.h>
#include <arc/coro/events/io_event_base.h>
#include <arc/io/utils.h>
#include <arc/net/address.h>
#include <arc/utils/exception.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <iostream>

#include "io_base.h"

namespace arc {
namespace coro {
template <net::Domain AF, net::Protocol P, io::IOType T>
class IOAwaiter;

}  // namespace coro

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
          getprotobyname(std::string(nameof::nameof_enum<P>()).c_str());
      if (protocol_info) {
        fd_ = socket(static_cast<int>(AF), static_cast<int>(ST),
                     protocol_info->p_proto);
      } else {
        fd_ = socket(static_cast<int>(AF), static_cast<int>(ST), 0);
      }
    }
    if (fd_ < 0) {
      arc::utils::ThrowErrnoExceptions();
    }
    buffer_ = new char[kRecvBufferSize_];
  }

  SocketBase(int fd, const net::Address<AF>& in_addr) {
    fd_ = fd;
    addr_ = in_addr;
    buffer_ = new char[kRecvBufferSize_];
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

  ~SocketBase() { delete[] buffer_; }

  void Bind(const net::Address<AF>& addr) {
    addr_ = addr;
    if (bind(this->fd_, addr_.GetCStyleAddress(), addr_.AddressSize()) < 0) {
      arc::utils::ThrowErrnoExceptions();
    }
    is_bound_ = true;
  }

  void SetOption(arc::net::SocketOption option_name, int opt_value) {
    if (setsockopt(fd_, (int)(arc::net::SocketLevel::SOCKET), (int)option_name,
                   &opt_value, sizeof(opt_value)) < 0) {
      arc::utils::ThrowErrnoExceptions();
    }
  }

  void SetNonBlocking(bool is_enabled = true) {
    int flags = fcntl(fd_, F_GETFL);
    if (flags < 0) {
      arc::utils::ThrowErrnoExceptions();
    }
    if (is_enabled) {
      flags = fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    } else {
      flags = fcntl(fd_, F_SETFL, flags & (~O_NONBLOCK));
    }
    if (flags < 0) {
      arc::utils::ThrowErrnoExceptions();
    }
    is_non_blocking_ = is_enabled;
  }

 protected:
  using CAddressType =
      typename std::conditional_t<(AF == arc::net::Domain::IPV4), sockaddr_in,
                                  sockaddr_in6>;

  net::Address<AF> addr_{};
  const int kRecvBufferSize_{1024};
  char* buffer_{nullptr};
  bool is_non_blocking_{false};
  bool is_bound_{false};

  template <net::Domain UAF>
  int InternalSendTo(const std::string& data, const net::Address<UAF>* addr) {
    int sent = sendto(this->fd_, data.c_str(), data.size(), 0,
                      (addr ? addr->GetCStyleAddress() : nullptr),
                      (addr ? addr->AddressSize() : 0));
    if (sent < 0) {
      arc::utils::ThrowErrnoExceptions();
    }
    return sent;
  }

  template <net::Domain UAF>
  int InternalTrySendTo(const std::string& data,
                        const net::Address<UAF>* addr) {
    return sendto(this->fd_, data.c_str(), data.size(), 0,
                  (addr ? addr->GetCStyleAddress() : nullptr),
                  (addr ? addr->AddressSize() : 0));
  }

  template <net::Domain UAF>
  std::string InternalRecvFrom(int max_recv_bytes, net::Address<UAF>* addr) {
    if (addr) {
      // Call UDP::RecvFrom
      if (max_recv_bytes < 0) {
        throw std::logic_error(
            "Calling UDP's RecvFrom function should spcifiy the receiving "
            "bytes.");
      }
      sockaddr in_addr;
      socklen_t in_addr_len;
      std::string ret;
      ret.reserve(max_recv_bytes);
      ssize_t tmp_read = recvfrom(this->fd_, ret.data(), max_recv_bytes, 0,
                                  &in_addr, &in_addr_len);
      if (tmp_read == -1) {
        arc::utils::ThrowErrnoExceptions();
      }
      (*addr) = *((CAddressType*)(&in_addr));
      return ret;
    }

    // Call TCP::Recv
    std::string buffer;
    max_recv_bytes = (max_recv_bytes < 0
                          ? std::numeric_limits<decltype(max_recv_bytes)>::max()
                          : max_recv_bytes);
    int left_size = max_recv_bytes;
    while (left_size > 0) {
      int this_read_size = std::min(left_size, kRecvBufferSize_);
      ssize_t tmp_read =
          recvfrom(this->fd_, buffer_, this_read_size, 0, nullptr, nullptr);
      if (tmp_read > 0) {
        buffer.append(buffer_, tmp_read);
      }
      if (tmp_read == -1) {
        if (errno != EAGAIN || errno != EWOULDBLOCK) {
          arc::utils::ThrowErrnoExceptions();
        }
        std::cout << "break" << std::endl;
        break;
      } else if (tmp_read < kRecvBufferSize_) {
        // we read all contents;
        break;
      }
      left_size -= tmp_read;
    }
    return buffer;
  }

 private:
  void MoveFrom(SocketBase&& other) {
    if (!buffer_) {
      buffer_ = new char[kRecvBufferSize_];
    }
    std::memcpy(buffer_, other.buffer_, other.kRecvBufferSize_);
    addr_ = std::move(other.addr_);
    is_non_blocking_ = other.is_non_blocking_;
  }
};

}  // namespace detail

template <net::Domain AF = net::Domain::IPV4,
          net::Protocol P = net::Protocol::TCP, Pattern PP = Pattern::SYNC>
class Socket : public detail::SocketBase<AF,
                                         ((P == net::Protocol::TCP)
                                              ? net::SocketType::STREAM
                                              : net::SocketType::DATAGRAM),
                                         P> {
 public:
  Socket()
      : detail::SocketBase<AF,
                           ((P == net::Protocol::TCP)
                                ? net::SocketType::STREAM
                                : net::SocketType::DATAGRAM),
                           P>() {
    if constexpr (PP == Pattern::ASYNC) {
      this->template SetNonBlocking(true);
    }
  }
  Socket(int fd, const net::Address<AF>& in_addr)
      : detail::SocketBase<AF,
                           ((P == net::Protocol::TCP)
                                ? net::SocketType::STREAM
                                : net::SocketType::DATAGRAM),
                           P>(fd, in_addr) {
    if constexpr (PP == Pattern::ASYNC) {
      this->template SetNonBlocking(true);
    }
  }
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
  Socket(Socket&& other)
      : detail::SocketBase<AF,
                           ((P == net::Protocol::TCP)
                                ? net::SocketType::STREAM
                                : net::SocketType::DATAGRAM),
                           P>(std::move(other)) {}

  Socket& operator=(Socket&& other) {
    detail::SocketBase<AF,
                       ((P == net::Protocol::TCP) ? net::SocketType::STREAM
                                                  : net::SocketType::DATAGRAM),
                       P>::operator=(std::move(other));
    return *this;
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::SYNC) int Send(const std::string& data) {
    return InternalSend(data);
  }

  template <Pattern UP = PP>
  requires(UP == Pattern::ASYNC)
      coro::IOAwaiter<AF, net::Protocol::TCP, io::IOType::WRITE> Send(
          const std::string& data) {
    return {this, &data};
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) && (UPP == Pattern::SYNC) std::string
      Recv(int max_recv_bytes = -1) {
    return InternalRecv(max_recv_bytes);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::ASYNC)
          coro::IOAwaiter<AF, net::Protocol::TCP, io::IOType::READ> Recv(
              int max_recv_bytes = -1) {
    return {this, max_recv_bytes};
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::SYNC) void Connect(const net::Address<AF>& addr) {
    return InternalConnect(addr);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::ASYNC)
          coro::IOAwaiter<AF, net::Protocol::TCP, io::IOType::CONNECT> Connect(
              const net::Address<AF>& addr) {
    return {this, &addr};
  }

  // UDP

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::UDP) &&
      (UPP == Pattern::SYNC) int SendTo(const std::string& data,
                                        const net::Address<AF>& addr) {
    return this->template InternalSendTo<AF>(data, &addr);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::UDP) &&
      (UPP == Pattern::SYNC) std::pair<net::Address<AF>, std::string> RecvFrom(
          int max_recv_bytes = -1) {
    std::pair<net::Address<AF>, std::string> ret =
        std::make_pair<net::Address<AF>, std::string>({}, {});
    // TODO complete UDP implementation of InternalFrom
    ret.second =
        this->template InternalRecvFrom<AF>(max_recv_bytes, &(ret.first));
    return ret;
  }

  friend class arc::coro::IOAwaiter<AF, P, io::IOType::CONNECT>;
  friend class arc::coro::IOAwaiter<AF, P, io::IOType::READ>;
  friend class arc::coro::IOAwaiter<AF, P, io::IOType::WRITE>;

 private:
  template <net::Protocol UP = P>
  requires(UP == net::Protocol::TCP) int InternalSend(const std::string& data) {
    return this->template InternalSendTo<AF>(data, nullptr);
  }

  template <net::Protocol UP = P>
  requires(UP ==
           net::Protocol::TCP) int InternalTrySend(const std::string& data) {
    return this->template InternalTrySendTo<AF>(data, nullptr);
  }

  template <net::Protocol UP = P>
  requires(UP == net::Protocol::TCP) std::string
      InternalRecv(int max_recv_bytes = -1) {
    return this->template InternalRecvFrom<AF>(max_recv_bytes, nullptr);
  }

  template <net::Protocol UP = P>
  requires(UP == net::Protocol::TCP) void InternalConnect(
      const net::Address<AF>& addr) {
    int res = connect(this->fd_, addr.GetCStyleAddress(), addr.AddressSize());
    if (res < 0) {
      arc::utils::ThrowErrnoExceptions();
    }
  }
  template <net::Protocol UP = P>
  requires(UP == net::Protocol::TCP) bool InternalTryConnect(
      const net::Address<AF>& addr) {
    return (connect(this->fd_, addr.GetCStyleAddress(), addr.AddressSize()) ==
            0);
  }
};

template <net::Domain AF = net::Domain::IPV4, Pattern PP = Pattern::SYNC>
class Acceptor : public Socket<AF, net::Protocol::TCP, PP> {
 public:
  Acceptor() : Socket<AF, net::Protocol::TCP, PP>() {}
  Acceptor(Acceptor&& other)
      : Socket<AF, net::Protocol::TCP, PP>(std::move(other)) {}
  Acceptor& operator=(Acceptor&& other) {
    Socket<AF, net::Protocol::TCP, PP>::operator=(std::move(other));
    return *this;
  }
  ~Acceptor() {
    if constexpr (PP == Pattern::ASYNC) {
      if (is_listened_) {
        arc::coro::GetLocalEventLoop().RemoveIOEvent(this->fd_,
                                                     io::IOType::ACCEPT, true);
      }
    }
  }

  void Listen(int max_pending_connections = 1024) {
    if (listen(this->fd_, max_pending_connections) < 0) {
      arc::utils::ThrowErrnoExceptions();
    }
    is_listened_ = true;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) Socket<AF, net::Protocol::TCP, PP> Accept() {
    return InternalAccept();
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC)
      coro::IOAwaiter<AF, net::Protocol::TCP, io::IOType::ACCEPT> Accept() {
    return {this, nullptr};
  }

  friend class arc::coro::IOAwaiter<AF, net::Protocol::TCP, io::IOType::ACCEPT>;

 private:
  std::queue<Socket<AF, net::Protocol::TCP, PP>> cached_incoming_sockets_{};

  template <io::Pattern UPP = PP>
  requires(UPP == io::Pattern::ASYNC) bool HasCachedAvailableSocket() {
    return !cached_incoming_sockets_.empty();
  }

  template <io::Pattern UPP = PP>
  requires(UPP == io::Pattern::ASYNC)
      Socket<AF, net::Protocol::TCP, UPP> GetNextAvailableSocket() {
    if (!cached_incoming_sockets_.empty()) {
      Socket<AF, net::Protocol::TCP, PP> next_socket =
          std::move(cached_incoming_sockets_.front());
      cached_incoming_sockets_.pop();
      return std::move(next_socket);
    }
    typename detail::SocketBase<AF, net::SocketType::STREAM,
                                net::Protocol::TCP>::CAddressType in_addr;
    socklen_t addrlen = sizeof(in_addr);
    int accept_fd = 0;
    while (accept_fd == 0) {
      int accept_fd =
          accept(this->fd_, (struct sockaddr*)&in_addr, (socklen_t*)&addrlen);
      if (accept_fd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        arc::utils::ThrowErrnoExceptions();
      }
      if (accept_fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        break;
      }
      cached_incoming_sockets_.emplace(accept_fd, in_addr);
    }
    Socket<AF, net::Protocol::TCP, PP> next_socket =
        std::move(cached_incoming_sockets_.front());
    cached_incoming_sockets_.pop();
    return std::move(next_socket);
  }

  Socket<AF, net::Protocol::TCP, PP> InternalAccept() {
    typename detail::SocketBase<AF, net::SocketType::STREAM,
                                net::Protocol::TCP>::CAddressType in_addr;
    socklen_t addrlen = sizeof(in_addr);
    int accept_fd =
        accept(this->fd_, (struct sockaddr*)&in_addr, (socklen_t*)&addrlen);
    if (accept_fd < 0) {
      arc::utils::ThrowErrnoExceptions();
    }
    return Socket<AF, net::Protocol::TCP, PP>(accept_fd, in_addr);
  }

 private:
  bool is_listened_{false};
};

}  // namespace io
}  // namespace arc

#include <arc/coro/awaiter/io_awaiter.ipp>

#endif /* LIBARC__IO__SOCKET_H */
