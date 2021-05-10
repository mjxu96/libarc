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

#include <arc/exception/io.h>
#include <arc/io/ssl.h>

#include <arc/utils/nameof.hpp>
#include <string>
#include <unordered_map>

void SSLInfoCallback(const ::SSL* s, int where, int ret) {
  const char* str;
  int w;

  w = where & ~SSL_ST_MASK;

  if (w & SSL_ST_CONNECT)
    str = "SSL_connect";
  else if (w & SSL_ST_ACCEPT)
    str = "SSL_accept";
  else
    str = "undefined";

  if (where & SSL_CB_LOOP) {
    printf("%s:%s\n", str, SSL_state_string_long(s));
  } else if (where & SSL_CB_ALERT) {
    str = (where & SSL_CB_READ) ? "read" : "write";
    printf("SSL3 alert %s:%s:%s\n", str, SSL_alert_type_string_long(ret),
           SSL_alert_desc_string_long(ret));
  } else if (where & SSL_CB_EXIT) {
    if (ret == 0)
      printf("%s:failed in %s\n", str, SSL_state_string_long(s));
    else if (ret < 0) {
      printf("%s:error in %s\n", str, SSL_state_string_long(s));
    }
  }
}

using namespace arc::io;

SSLContext::SSLContext(TLSProtocol protocol, TLSProtocolType type)
    : protocol_(protocol), type_(type) {
  const SSL_METHOD* method = nullptr;
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  if (type_ == TLSProtocolType::NOT_SPEC) {
    method = TLS_method();
  } else if (type_ == TLSProtocolType::CLIENT) {
    method = TLS_client_method();
  } else {
    method = TLS_server_method();
  }
  context_ = SSL_CTX_new(method);
  if (!context_) {
    throw arc::exception::TLSException("SSL Context Creation Error");
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

void SSLContext::SetCertificateAndKey(const std::string& cert_file,
                                      const std::string& key_file) {
  if (SSL_CTX_use_certificate_file(context_, cert_file.c_str(),
                                   SSL_FILETYPE_PEM) != 1) {
    throw arc::exception::TLSException("Load Cert Failure");
  }
  if (SSL_CTX_use_PrivateKey_file(context_, key_file.c_str(),
                                  SSL_FILETYPE_PEM) != 1) {
    throw arc::exception::TLSException("Load PKey Failure");
  }
  if (SSL_CTX_check_private_key(context_) != 1) {
    throw arc::exception::TLSException("Check PKey Failure");
  }
}

void SSLContext::SetDebugMode(bool debug) {
  if (debug) {
    SSL_CTX_set_info_callback(context_, SSLInfoCallback);
  } else {
    SSL_CTX_set_info_callback(context_, nullptr);
  }
}

arc::io::SSL SSLContext::FetchSSL() {
  auto new_ssl = SSL_new(context_);
  if (!new_ssl) {
    throw std::logic_error(GetSSLError());
  }
  return {new_ssl};
}

void SSLContext::FreeSSL(SSL& ssl) { SSL_free(ssl.ssl); }

std::string SSLContext::GetSSLError() {
  return std::string(ERR_reason_error_string(ERR_get_error()));
}

SSLContext& arc::io::GetLocalSSLContext(TLSProtocol protocol,
                                        TLSProtocolType type) {
  thread_local static std::unordered_map<
      TLSProtocol, std::unordered_map<TLSProtocolType, SSLContext>>
      global_contexts;
  if (global_contexts.find(protocol) == global_contexts.end()) {
    global_contexts[protocol] =
        std::unordered_map<TLSProtocolType, SSLContext>();
  }
  if (global_contexts[protocol].find(type) == global_contexts[protocol].end()) {
    global_contexts[protocol][type] = SSLContext(protocol, type);
  }
  return global_contexts[protocol][type];
}
