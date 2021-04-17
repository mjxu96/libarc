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
          getprotobyname(std::string(nameof::nameof_enum<P>()).c_str());
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

  template <net::Domain UAF>
  int InternalSendTo(const void* data, int num, const net::Address<UAF>* addr) {
    int sent = sendto(this->fd_, data, num, 0,
                      (addr ? addr->GetCStyleAddress() : nullptr),
                      (addr ? addr->AddressSize() : 0));
    if (sent < 0) {
      throw arc::exception::IOException();
    }
    return sent;
  }

  template <net::Domain UAF>
  int InternalTrySendTo(const void* data, int num,
                        const net::Address<UAF>* addr) {
    return sendto(this->fd_, data, num, 0,
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
        throw arc::exception::IOException();
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
      int this_read_size = std::min(left_size, RECV_BUFFER_SIZE);
      ssize_t tmp_read =
          recvfrom(this->fd_, buffer_, this_read_size, 0, nullptr, nullptr);
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

 private:
  void MoveFrom(SocketBase&& other) {
    std::memset(buffer_, 0, RECV_BUFFER_SIZE);
    std::memcpy(buffer_, other.buffer_, RECV_BUFFER_SIZE);
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

  virtual ~Socket() {}

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::SYNC) int Send(const void* data, int num) {
    return InternalSend(data, num);
  }

  template <net::Protocol UP = P, Pattern UPP = PP, concepts::Writable DataType>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::SYNC) int Send(DataType&& data) {
    return InternalSend(data.c_str(), data.size());
  }

  template <Pattern UP = PP>
  requires(UP == Pattern::ASYNC)
      coro::IOAwaiter<int, int, io::IOType::WRITE> Send(const void* data,
                                                        int num) {
    return {std::bind(&Socket<AF, P, PP>::InternalTrySend<P>, this, data, num),
            std::bind(&Socket<AF, P, PP>::InternalSend<P>, this, data, num),
            this->fd_};
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) && (UPP == Pattern::SYNC) std::string
      Recv(int max_recv_bytes = -1) {
    return InternalRecv(max_recv_bytes);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::ASYNC)
          coro::IOAwaiter<void, std::string, io::IOType::READ> Recv(
              int max_recv_bytes = -1) {
    return {
        std::bind(&Socket<AF, P, PP>::DoNothingVoid<PP>, this),
        std::bind(&Socket<AF, P, PP>::InternalRecv<P>, this, max_recv_bytes),
        this->fd_};
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::SYNC) void Connect(const net::Address<AF>& addr) {
    return InternalConnect(addr);
  }

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::TCP) &&
      (UPP == Pattern::ASYNC)
          coro::IOAwaiter<bool, void, io::IOType::CONNECT> Connect(
              const net::Address<AF>& addr) {
    return {std::bind(&Socket<AF, P, PP>::InternalTryConnect, this, addr),
            std::bind(&Socket<AF, P, PP>::DoNothingVoid<PP>, this), this->fd_};
  }

  // UDP
  // TODO add coroutine functions

  template <net::Protocol UP = P, Pattern UPP = PP>
      requires(UP == net::Protocol::UDP) &&
      (UPP == Pattern::SYNC) int SendTo(const void* data, int num,
                                        const net::Address<AF>& addr) {
    return this->template InternalSendTo<AF>(data, num, &addr);
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

 protected:
  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) bool DoNothingBool() {
    return true;
  }
  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) int DoNothingInt() {
    return 0;
  }
  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) void DoNothingVoid() {}

  template <net::Protocol UP = P>
  requires(UP == net::Protocol::TCP) int InternalSend(const void* data,
                                                      int num) {
    return this->template InternalSendTo<AF>(data, num, nullptr);
  }

  template <net::Protocol UP = P>
  requires(UP == net::Protocol::TCP) int InternalTrySend(const void* data,
                                                         int num) {
    return this->template InternalTrySendTo<AF>(data, num, nullptr);
  }

  template <net::Protocol UP = P>
  requires(UP == net::Protocol::TCP) std::string
      InternalRecv(int max_recv_bytes = -1) {
    return this->template InternalRecvFrom<AF>(max_recv_bytes, nullptr);
  }

  void InternalConnect(const net::Address<AF>& addr) {
    static_assert(P != net::Protocol::UDP);
    int res = connect(this->fd_, addr.GetCStyleAddress(), addr.AddressSize());
    if (res < 0) {
      throw arc::exception::IOException();
    }
  }

  bool InternalTryConnect(const net::Address<AF>& addr) {
    static_assert(P != net::Protocol::UDP);
    return (connect(this->fd_, addr.GetCStyleAddress(), addr.AddressSize()) ==
            0);
  }
};

template <net::Domain AF = net::Domain::IPV4, Pattern PP = Pattern::SYNC>
class TLSSocket : virtual public Socket<AF, net::Protocol::TCP, PP> {
 public:
  TLSSocket(TLSProtocol protocol = TLSProtocol::NOT_SPEC,
            TLSProtocolType type = TLSProtocolType::CLIENT)
      : Socket<AF, net::Protocol::TCP, PP>(),
        protocol_(protocol),
        type_(type),
        context_ptr_(&GetLocalSSLContext(protocol, type)) {
    ssl_ = context_ptr_->FetchSSL();
    BindFdWithSSL();
  }

  TLSSocket(Socket<AF, net::Protocol::TCP, PP>&& other, TLSProtocol protocol,
            TLSProtocolType type)
      : Socket<AF, net::Protocol::TCP, PP>(std::move(other)),
        protocol_(protocol),
        type_(type),
        context_ptr_(&GetLocalSSLContext(protocol, type)) {
    ssl_ = context_ptr_->FetchSSL();
    BindFdWithSSL();
  }

  virtual ~TLSSocket() {
    if (context_ptr_) {
      context_ptr_->FreeSSL(ssl_);
    }
  }
  TLSSocket(int fd, const net::Address<AF>& in_addr,
            TLSProtocol protocol = TLSProtocol::NOT_SPEC,
            TLSProtocolType type = TLSProtocolType::CLIENT)
      : Socket<AF, net::Protocol::TCP, PP>(fd, in_addr),
        protocol_(protocol),
        type_(type),
        context_ptr_(&GetLocalSSLContext(protocol, type)) {
    ssl_ = context_ptr_->FetchSSL();
    BindFdWithSSL();
  }

  TLSSocket(const TLSSocket&) = delete;
  TLSSocket& operator=(const TLSSocket&) = delete;
  TLSSocket(TLSSocket&& other)
      : Socket<AF, net::Protocol::TCP, PP>(std::move(other)),
        protocol_(other.protocol_),
        type_(other.type_),
        context_ptr_(other.context_ptr_),
        ssl_(other.ssl_) {
    other.ssl_.ssl = nullptr;
    other.context_ptr_ = nullptr;
    BindFdWithSSL();
  }

  TLSSocket& operator=(TLSSocket&& other) {
    Socket<AF, net::Protocol::TCP, PP>::operator=(std::move(other));
    if (ssl_.ssl && context_ptr_) {
      context_ptr_->FreeSSL(ssl_);
    }
    protocol_ = other.protocol_;
    type_ = other.type_;
    context_ptr_ = other.context_ptr_;
    ssl_ = other.ssl_;
    other.ssl_.ssl = nullptr;
    other.context_ptr_ = nullptr;
    BindFdWithSSL();
    return *this;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) std::string Recv(int max_recv_bytes = -1) {
    std::string buffer;
    max_recv_bytes = (max_recv_bytes < 0
                          ? std::numeric_limits<decltype(max_recv_bytes)>::max()
                          : max_recv_bytes);
    int left_size = max_recv_bytes;
    while (left_size > 0) {
      int this_read_size = std::min(left_size, RECV_BUFFER_SIZE);
      ssize_t tmp_read = SSL_read(ssl_.ssl, this->buffer_, this_read_size);
      if (tmp_read > 0) {
        buffer.append(this->buffer_, tmp_read);
      }
      if (tmp_read == -1) {
        throw arc::exception::TLSException("Read Error");
      } else if (tmp_read < RECV_BUFFER_SIZE) {
        // we read all contents;
        break;
      }
      left_size -= tmp_read;
    }
    return buffer;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<std::string> Recv(
      int max_recv_bytes = -1) {
    if (max_recv_bytes >= 0) {
      throw std::logic_error(
          "Async Recv of TLSSocket cannot specify the max_recv_bytes.");
    }
    int ret = -1;
    bool is_data_read = false;
    std::string ret_str;
    while (true) {
      ret = SSL_read(ssl_.ssl, this->buffer_, RECV_BUFFER_SIZE);
      if (ret <= 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          if (!is_data_read) {
            co_await coro::TLSIOAwaiter(this->fd_, arc::io::IOType::READ);
          } else {
            break;
          }
        } else if (err == SSL_ERROR_WANT_WRITE) {
          co_await coro::TLSIOAwaiter(this->fd_, arc::io::IOType::WRITE);
        } else if (err == SSL_ERROR_ZERO_RETURN) {
          // no data, we exit
          break;
        } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
          break;
        } else {
          throw exception::TLSException("Read Error");
        }
      } else {
        is_data_read = true;
        ret_str.append(this->buffer_, ret);
      }
    }
    co_return ret_str;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) int Send(const void* data, int num) {
    int writtern_size = SSL_write(ssl_.ssl, data, num);
    if (writtern_size < 0) {
      throw arc::exception::TLSException("Write Error");
    }
    return writtern_size;
  }

  template <Pattern UPP = PP, concepts::Writable DataType>
  requires(UPP == Pattern::SYNC) int Send(DataType&& data) {
    return Send(data.c_str(), data.size());
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<int> Send(const void* data,
                                                       int num) {
    co_await coro::TLSIOAwaiter(this->fd_, arc::io::IOType::WRITE);
    int ret = -1;
    do {
      ret = SSL_write(ssl_.ssl, data, num);
      if (ret < 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          co_await coro::TLSIOAwaiter(this->fd_, arc::io::IOType::READ);
        } else if (err == SSL_ERROR_WANT_WRITE) {
          co_await coro::TLSIOAwaiter(this->fd_, arc::io::IOType::WRITE);
        } else {
          throw exception::TLSException("Write Error", err);
        }
      }
    } while (ret < 0);
    co_return ret;
  }

  // template <Pattern UPP = PP, concepts::Writable DataType>
  // requires(UPP == Pattern::ASYNC) coro::Task<int> Send(DataType&& data) {
  //   std::shared_ptr<std::string> data_ptr =
  //   std::make_shared<std::string>(std::forward<DataType>(data)); return
  //   co_await Send(data_ptr->c_str(), data_ptr->size());
  // }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) void Connect(const net::Address<AF>& addr) {
    Socket<AF, net::Protocol::TCP, UPP>::Connect(addr);
    if (SSL_connect(ssl_.ssl) != 1) {
      throw arc::exception::TLSException("Connection Error");
    }
  }

  template <Pattern UPP = PP>
  requires(UPP ==
           Pattern::ASYNC) coro::Task<void> Connect(net::Address<AF> addr) {
    // initial TCP connect
    co_await Socket<AF, net::Protocol::TCP, UPP>::Connect(addr);

    // then TLS handshakes
    SSL_set_connect_state(ssl_.ssl);
    int r = 0;
    while ((r = HandShake()) != 1) {
      int err = SSL_get_error(ssl_.ssl, r);
      if (err == SSL_ERROR_WANT_WRITE) {
        co_await arc::coro::TLSIOAwaiter(this->fd_, arc::io::IOType::WRITE);
      } else if (err == SSL_ERROR_WANT_READ) {
        co_await arc::coro::TLSIOAwaiter(this->fd_, arc::io::IOType::READ);
      } else {
        throw arc::exception::TLSException("Connection Error");
      }
    }
    co_return;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) void Shutdown() {
    if (!ssl_.ssl) {
      return;
    }
    int ret = 0;
    while (true) {
      ret = SSL_shutdown(ssl_.ssl);
      if (ret == 1) {
        // successfully shutdown
        break;
      } else if (ret == 0) {
        // not finished yet, call again
        continue;
      } else {
        int ssl_err = SSL_get_error(ssl_.ssl, ret);
        throw arc::exception::TLSException("Shutdown Error", ssl_err);
      }
    }
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<void> Shutdown() {
    if (!ssl_.ssl) {
      co_return;
    }
    int ret = 0;
    while (true) {
      ret = SSL_shutdown(ssl_.ssl);
      if (ret == 1) {
        // successfully shutdown
        break;
      } else if (ret == 0) {
        // not finished yet, call again
        continue;
      } else {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_WRITE) {
          co_await arc::coro::TLSIOAwaiter(this->fd_, arc::io::IOType::WRITE);
        } else if (err == SSL_ERROR_WANT_READ) {
          co_await arc::coro::TLSIOAwaiter(this->fd_, arc::io::IOType::READ);
        } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
          break;
        } else {
          throw arc::exception::TLSException("Shutdown Error", err);
        }
      }
    }
    co_return;
  }

  void SetAcceptState() { SSL_set_accept_state(ssl_.ssl); }

  io::SSL& GetSSLObject() { return ssl_; }

  int HandShake() { return SSL_do_handshake(ssl_.ssl); }

 protected:
  void BindFdWithSSL() { SSL_set_fd(ssl_.ssl, this->fd_); }

  io::SSLContext* context_ptr_{nullptr};
  io::SSL ssl_{};
  TLSProtocol protocol_;
  TLSProtocolType type_;
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
  requires(UPP == Pattern::ASYNC)
      coro::IOAwaiter<bool, Socket<AF, arc::net::Protocol::TCP, Pattern::ASYNC>,
                      io::IOType::ACCEPT> Accept() {
    return {std::bind(&Acceptor<AF, PP>::HasCachedAvailableSocket<PP>, this),
            std::bind(&Acceptor<AF, PP>::GetNextAvailableSocket<PP>, this),
            this->fd_};
  }

 protected:
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
        throw arc::exception::IOException();
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
      throw arc::exception::IOException();
    }
    return Socket<AF, net::Protocol::TCP, PP>(accept_fd, in_addr);
  }

  bool is_listened_{false};
};

template <net::Domain AF = net::Domain::IPV4, Pattern PP = Pattern::SYNC>
class TLSAcceptor : public TLSSocket<AF, PP>, public Acceptor<AF, PP> {
 public:
  using TLSSocket<AF, PP>::Connect;
  using TLSSocket<AF, PP>::Send;
  using TLSSocket<AF, PP>::Recv;

  TLSAcceptor(const std::string& cert_file, const std::string& key_file,
              TLSProtocol protocol = TLSProtocol::NOT_SPEC,
              TLSProtocolType type = TLSProtocolType::SERVER)
      : TLSSocket<AF, PP>(protocol, type),
        Acceptor<AF, PP>(),
        cert_file_(cert_file),
        key_file_(key_file) {
    LoadCertificateAndKey(cert_file_, key_file_);
  }
  TLSAcceptor(TLSAcceptor&& other)
      : TLSSocket<AF, PP>(std::move(other)),
        Acceptor<AF, PP>(std::move(other)),
        cert_file_(std::move(other.cert_file_)),
        key_file_(std::move(other.key_file_)) {
    LoadCertificateAndKey(cert_file_, key_file_);
  }
  TLSAcceptor& operator=(TLSAcceptor&& other) {
    cert_file_ = std::move(other.cert_file_);
    key_file_ = std::move(other.key_file_);
    TLSSocket<AF, PP>::operator=(std::move(other));
    Acceptor<AF, PP>::operator=(std::move(other));
    LoadCertificateAndKey(cert_file_, key_file_);
    return *this;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) TLSSocket<AF, PP> Accept() {
    TLSSocket<AF, PP> tls_socket(Acceptor<AF, PP>::InternalAccept(),
                                 this->protocol_, this->type_);
    tls_socket.SetAcceptState();
    if (tls_socket.HandShake() != 1) {
      throw arc::exception::TLSException("Accept Error");
    }
    return tls_socket;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<TLSSocket<AF, PP>> Accept() {
    TLSSocket<AF, PP> tls_socket(co_await Acceptor<AF, PP>::Accept(),
                                 this->protocol_, this->type_);
    tls_socket.SetAcceptState();
    int r = 0;
    co_await arc::coro::TLSIOAwaiter(tls_socket.GetFd(), arc::io::IOType::READ);
    while ((r = tls_socket.HandShake()) != 1) {
      int err = SSL_get_error(tls_socket.GetSSLObject().ssl, r);
      if (err == SSL_ERROR_WANT_WRITE) {
        co_await arc::coro::TLSIOAwaiter(tls_socket.GetFd(),
                                         arc::io::IOType::WRITE);
      } else if (err == SSL_ERROR_WANT_READ) {
        co_await arc::coro::TLSIOAwaiter(tls_socket.GetFd(),
                                         arc::io::IOType::READ);
      } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
        break;
      } else {
        throw arc::exception::TLSException("Accept Error", err);
      }
    }
    co_return std::move(tls_socket);
  }

 private:
  void LoadCertificateAndKey(const std::string& cert_file,
                             const std::string& key_file) {
    this->context_ptr_->SetCertificateAndKey(cert_file, key_file);
  }

  std::string cert_file_;
  std::string key_file_;
};

}  // namespace io
}  // namespace arc

#endif /* LIBARC__IO__SOCKET_H */
