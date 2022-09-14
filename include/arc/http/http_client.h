/*
 * File: http_client.h
 * Project: libarc
 * File Created: Wednesday, 14th September 2022 1:08:04 am
 * Author: Minjun Xu (mjxu96@outlook.com)
 * -----
 * MIT License
 * Copyright (c) 2022 Minjun Xu
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

#pragma once

#include <arc/coro/task.h>
#include <arc/io/socket.h>
#include <arc/io/tls_socket.h>
#include <arc/net/address.h>

#include "http_parser.h"

namespace arc {
namespace http {

template <arc::net::Domain D = arc::net::Domain::IPV4, bool SSL = true>
class HttpClient {
 public:
  using SockType = typename std::conditional_t<
      SSL, arc::io::TLSSocket<D, arc::io::Pattern::ASYNC>,
      arc::io::Socket<D, arc::net::Protocol::TCP, arc::io::Pattern::ASYNC>>;
  HttpClient(const arc::net::Address<D>& server_addr)
      : server_addr_(server_addr) {}
  arc::coro::Task<HttpResponse> Request(const HttpRequest& request) {
    HttpResponse response;
    co_await conn_.Connect(server_addr_);
    auto wrote_string = arc::http::GetReturnStringFromHttpRequest(request);
    auto wrote_size =
        co_await conn_.Send(wrote_string.c_str(), wrote_string.size());
    if (wrote_size <= 0) {
      response.is_valid = false;
      co_return response;
    }

    bool is_need_one_other_read = false;
    HttpParser parser(HTTP_RESPONSE);
    std::shared_ptr<char[]> data(new char[1024]);
    do {
      std::string recv = "";
      int recv_bytes = 0;
      while (true) {
        recv_bytes = co_await conn_.Recv(data.get(), 1024);
        if (recv_bytes <= 0) {
          break;
        }
        recv.append(data.get(), recv_bytes);
        if (recv_bytes < 1024) {
          break;
        }
      }

      if (recv_bytes == EOF) {
        is_need_one_other_read = false;
      }

      int ret = parser.ParseResponse(recv, &response);
      if (ret != 0) {
        response.is_valid = false;
        co_return response;
      }

      is_need_one_other_read = !response.is_complete;
    } while (is_need_one_other_read);
    co_return response;
  }

 private:
  SockType conn_;
  arc::net::Address<D> server_addr_;
};

}  // namespace http
}  // namespace arc
