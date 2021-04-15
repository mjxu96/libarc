/*
 * File: ssl.h
 * Project: libarc
 * File Created: Tuesday, 13th April 2021 8:03:40 pm
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

#ifndef LIBARC__IO__SSL_H
#define LIBARC__IO__SSL_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>

namespace arc {
namespace io {

enum class TLSProtocol {
  NOT_SPEC = 0U,
  SSLv2,
  SSLv3,
  SSLv23,
  TLSv1,
  TLSv1_1,
  TLSv1_2,
};

enum class TLSProtocolType {
  CLIENT = 0U,
  SERVER,
  NOT_SPEC,
};

struct SSL {
 public:
  ssl_st* ssl{nullptr};
};

class SSLContext {
 public:
  SSLContext() = default;
  SSLContext(TLSProtocol protocol, TLSProtocolType type);
  SSLContext(const SSLContext&) = delete;
  SSLContext& operator=(const SSLContext&) = delete;
  SSLContext(SSLContext&&);
  SSLContext& operator=(SSLContext&&);
  ~SSLContext();
  void SetCertificateAndKey(const std::string& cert_file, const std::string& key_file);
  SSL FetchSSL();
  void FreeSSL(SSL& ssl);
 private:
  TLSProtocol protocol_;
  TLSProtocolType type_;
  ::SSL_CTX* context_{nullptr};

  std::string GetSSLError();
};

SSLContext& GetGlobalSSLContext(TLSProtocol protocol, TLSProtocolType type);
  
} // namespace io
} // namespace arc


#endif