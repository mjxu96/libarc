/*
 * File: tls_socket.h
 * Project: libarc
 * File Created: Sunday, 9th May 2021 9:21:38 am
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

#ifndef LIBARC__IO__TLS_SOCKET_H
#define LIBARC__IO__TLS_SOCKET_H

#include <arc/coro/utils/cancellation_token.h>

#include "socket.h"

namespace arc {
namespace io {

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
  requires(UPP == Pattern::SYNC) ssize_t Recv(char* buf, int max_recv_bytes) {
    return SSL_read(ssl_.ssl, buf, max_recv_bytes);
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<ssize_t> Recv(char* buf,
                                                           int max_recv_bytes) {
    while (true) {
      ssize_t ret = SSL_read(ssl_.ssl, buf, max_recv_bytes);
      if (ret <= 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::READ);
        } else if (err == SSL_ERROR_WANT_WRITE) {
          co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::WRITE);
        } else if (err == SSL_ERROR_ZERO_RETURN) {
          co_return 0;
        } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
          co_return 0;
        } else {
          co_return ret;
        }
      } else {
        co_return ret;
      }
    }
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<ssize_t> Recv(
      char* buf, int max_recv_bytes, const coro::CancellationToken& token) {
    while (true) {
      ssize_t ret = SSL_read(ssl_.ssl, buf, max_recv_bytes);
      if (ret <= 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          bool is_abort = co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeInterruptedFunctor,
                        this),
              this->fd_, arc::io::IOType::READ, token);
          if (is_abort) {
            errno = EAGAIN;
            co_return -1;
          }
        } else if (err == SSL_ERROR_WANT_WRITE) {
          bool is_abort = co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeInterruptedFunctor,
                        this),
              this->fd_, arc::io::IOType::WRITE, token);
          if (is_abort) {
            errno = EAGAIN;
            co_return -1;
          }
        } else if (err == SSL_ERROR_ZERO_RETURN) {
          co_return 0;
        } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
          co_return 0;
        } else {
          co_return ret;
        }
      } else {
        co_return ret;
      }
    }
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<ssize_t> Recv(
      char* buf, int max_recv_bytes,
      const std::chrono::steady_clock::duration& timeout) {
    while (true) {
      ssize_t ret = SSL_read(ssl_.ssl, buf, max_recv_bytes);
      if (ret <= 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          bool is_abort = co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeInterruptedFunctor,
                        this),
              this->fd_, arc::io::IOType::READ, timeout);
          if (is_abort) {
            errno = EAGAIN;
            co_return -1;
          }
        } else if (err == SSL_ERROR_WANT_WRITE) {
          bool is_abort = co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeInterruptedFunctor,
                        this),
              this->fd_, arc::io::IOType::WRITE, timeout);
          if (is_abort) {
            errno = EAGAIN;
            co_return -1;
          }
        } else if (err == SSL_ERROR_ZERO_RETURN) {
          co_return 0;
        } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
          co_return 0;
        } else {
          co_return ret;
        }
      } else {
        co_return ret;
      }
    }
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) int Send(const void* data, int num) {
    return SSL_write(ssl_.ssl, data, num);
  }

  template <Pattern UPP = PP, concepts::Writable DataType>
  requires(UPP == Pattern::SYNC) int Send(DataType&& data) {
    return Send(data.c_str(), data.size());
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<int> Send(const void* data,
                                                       int num) {
    int ret = -1;
    do {
      ret = SSL_write(ssl_.ssl, data, num);
      if (ret < 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::READ);
        } else if (err == SSL_ERROR_WANT_WRITE) {
          co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::WRITE);
        } else {
          co_return ret;
        }
      }
    } while (ret < 0);
    co_return ret;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<int> Send(
      const void* data, int num, const coro::CancellationToken& token) {
    bool is_abort = false;
    int ret = -1;
    do {
      ret = SSL_write(ssl_.ssl, data, num);
      if (ret < 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          is_abort = co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeInterruptedFunctor,
                        this),
              this->fd_, arc::io::IOType::READ, token);
          if (is_abort) {
            errno = EAGAIN;
            co_return -1;
          }
        } else if (err == SSL_ERROR_WANT_WRITE) {
          is_abort = co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeInterruptedFunctor,
                        this),
              this->fd_, arc::io::IOType::WRITE, token);
          if (is_abort) {
            errno = EAGAIN;
            co_return -1;
          }
        } else {
          co_return ret;
        }
      }
    } while (ret < 0);
    co_return ret;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<int> Send(
      const void* data, int num, std::chrono::steady_clock::duration& timeout) {
    bool is_abort = false;
    int ret = -1;
    do {
      ret = SSL_write(ssl_.ssl, data, num);
      if (ret < 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          is_abort = co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeInterruptedFunctor,
                        this),
              this->fd_, arc::io::IOType::READ, timeout);
          if (is_abort) {
            errno = EAGAIN;
            co_return -1;
          }
        } else if (err == SSL_ERROR_WANT_WRITE) {
          is_abort = co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeInterruptedFunctor,
                        this),
              this->fd_, arc::io::IOType::WRITE, timeout);
          if (is_abort) {
            errno = EAGAIN;
            co_return -1;
          }
        } else {
          co_return ret;
        }
      }
    } while (ret < 0);
    co_return ret;
  }

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
        co_await arc::coro::IOAwaiter(
            std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
            std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this), this->fd_,
            arc::io::IOType::WRITE);
      } else if (err == SSL_ERROR_WANT_READ) {
        co_await arc::coro::IOAwaiter(
            std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
            std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this), this->fd_,
            arc::io::IOType::READ);
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
          co_await arc::coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::WRITE);
        } else if (err == SSL_ERROR_WANT_READ) {
          co_await arc::coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::READ);
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

  bool TLSIOReadyFunctor() { return false; }
  bool TLSIOResumeFunctor() { return false; }

  bool TLSIOResumeInterruptedFunctor() { return true; }

 protected:
  void BindFdWithSSL() { SSL_set_fd(ssl_.ssl, this->fd_); }

  io::SSLContext* context_ptr_{nullptr};
  io::SSL ssl_{};
  TLSProtocol protocol_;
  TLSProtocolType type_;
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
    co_return std::move(tls_socket);
  }

 private : bool TLSIOReadyFunctor() { return false; }
  void TLSIOResumeFunctor() { return; }

  void LoadCertificateAndKey(const std::string& cert_file,
                             const std::string& key_file) {
    this->context_ptr_->SetCertificateAndKey(cert_file, key_file);
  }

  std::string cert_file_;
  std::string key_file_;
};

}  // namespace io

}  // namespace arc

#endif