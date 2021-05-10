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

#include "socket_base.h"

namespace arc {
namespace io {

template <net::Domain AF = net::Domain::IPV4,
          net::Protocol P = net::Protocol::TCP, Pattern PP = Pattern::SYNC>
class Socket : public detail::SocketBase<AF,
                                         ((P == net::Protocol::TCP)
                                              ? net::SocketType::STREAM
                                              : net::SocketType::DATAGRAM),
                                         P> {
 public:
  using ParentType =
      typename detail::SocketBase<AF,
                                  ((P == net::Protocol::TCP)
                                       ? net::SocketType::STREAM
                                       : net::SocketType::DATAGRAM),
                                  P>;

  Socket()
      : detail::SocketBase<AF,
                           ((P == net::Protocol::TCP)
                                ? net::SocketType::STREAM
                                : net::SocketType::DATAGRAM),
                           P>() {
    if constexpr (PP == Pattern::ASYNC) {
      this->SetNonBlocking(true);
    }
  }
  Socket(int fd, const net::Address<AF>& in_addr)
      : detail::SocketBase<AF,
                           ((P == net::Protocol::TCP)
                                ? net::SocketType::STREAM
                                : net::SocketType::DATAGRAM),
                           P>(fd, in_addr) {
    if constexpr (PP == Pattern::ASYNC) {
      this->SetNonBlocking(true);
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

  virtual ~Socket() {
    if (this->fd_ > 0) {
      coro::GetLocalEventLoop().RemoveAllIOEvents(this->fd_);
    }
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) && (UPP == Pattern::SYNC) ssize_t
      Send(const void* data, int num) {
    return ParentType::template Send<UP>(data, num);
  }

  template <net::Protocol UP = P, Pattern UPP = PP, concepts::Writable DataType>
      requires(UP == net::Protocol::TCP) && (UPP == Pattern::SYNC) ssize_t
      Send(DataType&& data) {
    return ParentType::template Send<UP>(data.c_str(), data.size());
  }

  template <Pattern UP = PP>
  requires(UP == Pattern::ASYNC) auto Send(const void* data, int num) {
    return coro::IOAwaiter(
        std::bind(&Socket<AF, P, PP>::IOReadyFunctor<PP>, this),
        std::bind(&Socket<AF, P, PP>::SendResumeFunctor<PP>, this, data, num),
        this->fd_, io::IOType::WRITE);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) && (UPP == Pattern::SYNC) std::string
      RecvAll(int max_recv_bytes = -1) {
    return ParentType::RecvAll<UP>(max_recv_bytes);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) && (UPP == Pattern::SYNC) ssize_t
      Recv(char* buf, int max_recv_bytes = -1) {
    return ParentType::template Recv<UP>(buf, max_recv_bytes);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::ASYNC) auto Recv(char* buf, int max_recv_bytes = -1) {
    return coro::IOAwaiter(
        std::bind(&Socket<AF, P, PP>::IOReadyFunctor<PP>, this),
        std::bind(&Socket<AF, P, PP>::RecvResumeFunctor<PP>, this, buf,
                  max_recv_bytes),
        this->fd_, io::IOType::READ);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::SYNC) void Connect(const net::Address<AF>& addr) {
    int res = connect(this->fd_, addr.GetCStyleAddress(), addr.AddressSize());
    if (res < 0) {
      throw arc::exception::IOException();
    }
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::ASYNC) auto Connect(const net::Address<AF>& addr) {
    return coro::IOAwaiter(
        std::bind(&Socket<AF, P, PP>::ConnectReadyFunctor<PP>, this, addr),
        std::bind(&Socket<AF, P, PP>::ConnectResumeFunctor<PP>, this),
        this->fd_, io::IOType::WRITE);
  }

  // UDP
  // TODO add coroutine functions
  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::UDP) &&
      (UPP == Pattern::SYNC) int SendTo(const void* data, int num,
                                        const net::Address<AF>& addr) {
    return ParentType::SendTo<AF>(data, num, &addr);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::UDP) && (UPP == Pattern::SYNC) ssize_t
      RecvFrom(char* buf, int max_recv_bytes, net::Address<AF>& addr) {
    return ParentType::RecvFrom<AF>(buf, max_recv_bytes, &addr);
  }

 protected:
  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) bool IOReadyFunctor() {
    return false;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) bool ConnectReadyFunctor(
      const net::Address<AF>& addr) {
    int res = connect(this->fd_, addr.GetCStyleAddress(), addr.AddressSize());
    if (res < 0) {
      if (errno == EINPROGRESS) {
        return false;
      }
      throw arc::exception::IOException("Connection Error");
    }
    return true;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) ssize_t
      SendResumeFunctor(const void* buf, int num) {
    return ParentType::template Send<P>(buf, num);
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) ssize_t
      RecvResumeFunctor(char* buf, int num) {
    return ParentType::template Recv<P>(buf, num);
  }
  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) void ConnectResumeFunctor() {
    return;
  }
};

template <net::Domain AF = net::Domain::IPV4, Pattern PP = Pattern::SYNC>
class Acceptor : virtual public Socket<AF, net::Protocol::TCP, PP> {
 public:
  Acceptor() : Socket<AF, net::Protocol::TCP, PP>() {}
  Acceptor(Acceptor&& other)
      : Socket<AF, net::Protocol::TCP, PP>(std::move(other)) {}
  Acceptor& operator=(Acceptor&& other) {
    Socket<AF, net::Protocol::TCP, PP>::operator=(std::move(other));
    return *this;
  }

  void Listen(int max_pending_connections = 1024) {
    if (listen(this->fd_, max_pending_connections) < 0) {
      throw arc::exception::IOException();
    }
    is_listened_ = true;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) Socket<AF, net::Protocol::TCP, PP> Accept() {
    return InternalAccept();
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) auto Accept() {
    return coro::IOAwaiter(
        std::bind(&Acceptor<AF, UPP>::template IOReadyFunctor<UPP>, this),
        std::bind(&Acceptor<AF, UPP>::InternalAccept, this), this->fd_,
        io::IOType::READ);
  }

 protected:
  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) bool IOReadyFunctor() {
    return false;
  }

  Socket<AF, net::Protocol::TCP, PP> InternalAccept() {
    typename detail::SocketBase<AF, net::SocketType::STREAM,
                                net::Protocol::TCP>::CAddressType in_addr;
    socklen_t addrlen = sizeof(in_addr);
    int accept_fd =
        accept(this->fd_, (struct sockaddr*)&in_addr, (socklen_t*)&addrlen);
    if (accept_fd < 0) {
      throw arc::exception::IOException("Accept Error");
    }
    return Socket<AF, net::Protocol::TCP, PP>(accept_fd, in_addr);
  }

  bool is_listened_{false};
};

}  // namespace io
}  // namespace arc

#endif /* LIBARC__IO__SOCKET_H */
