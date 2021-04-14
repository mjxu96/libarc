/*
 * File: ssl.cc
 * Project: libarc
 * File Created: Tuesday, 13th April 2021 8:04:35 pm
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

#include <arc/io/ssl.h>
#include <arc/utils/nameof.hpp>
#include <stdexcept>
#include <string>
#include <openssl/err.h>
#include <unordered_map>

using namespace arc::io;

SSLContext::SSLContext(TLSProtocol protocol, TLSProtocolType type) : 
  protocol_(protocol), type_(type) {
  const SSL_METHOD* method = nullptr;
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  if (type_ == TLSProtocolType::NONE) {
    throw std::logic_error("Must specify whether TLS is for a client or a server");
  }
  if (protocol_ == TLSProtocol::TLSv1) {
    if (type_ == TLSProtocolType::CLIENT) {
      method = TLSv1_client_method();
    } else {
      method = TLSv1_server_method();
    }
  } else if (protocol_ == TLSProtocol::TLSv1_1) {
    if (type_ == TLSProtocolType::CLIENT) {
      method = TLSv1_1_client_method();
    } else {
      method = TLSv1_1_server_method();
    }
  } else if (protocol_ == TLSProtocol::TLSv1_2) {
    if (type_ == TLSProtocolType::CLIENT) {
      method = TLSv1_2_client_method();
    } else {
      method = TLSv1_2_server_method();
    }
  } else {
    std::string error = std::string(nameof::nameof_enum(protocol)) + std::string(" is not accepted");
    throw std::logic_error(error);
  }
  context_ = SSL_CTX_new(method);
  if (!context_) {
    throw std::logic_error(GetSSLError());
  }
}

SSLContext::~SSLContext() {
  if (context_) {
    SSL_CTX_free(context_);
  }
}

SSLContext::SSLContext(SSLContext&& other) : context_(other.context_) {
  other.context_ = nullptr;
  protocol_ = other.protocol_;
  type_ = other.type_;
}

SSLContext& SSLContext::operator=(SSLContext&& other) {
  context_ = other.context_;
  other.context_ = nullptr;
  protocol_ = other.protocol_;
  type_ = other.type_;
  return *this;
}

arc::io::SSL SSLContext::FetchSSL() {
  auto new_ssl = SSL_new(context_);
  if (!new_ssl) {
    throw std::logic_error(GetSSLError());
  }
  return {new_ssl};
}

void SSLContext::FreeSSL(SSL& ssl) {
  SSL_free(ssl.ssl);
}

std::string SSLContext::GetSSLError() {
  return std::string(ERR_reason_error_string(ERR_get_error()));
}

SSLContext& arc::io::GetGlobalSSLContext(TLSProtocol protocol, TLSProtocolType type) {
  static std::unordered_map<TLSProtocol, std::unordered_map<TLSProtocolType, SSLContext>> global_contexts;
  if (global_contexts.find(protocol) == global_contexts.end()) {
    global_contexts[protocol] = std::unordered_map<TLSProtocolType, SSLContext>();
  }
  if (global_contexts[protocol].find(type) == global_contexts[protocol].end()) {
    global_contexts[protocol][type] = SSLContext(protocol, type);
  }
  return global_contexts[protocol][type];
}
