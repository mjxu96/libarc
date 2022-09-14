/*
 * File: http_server.h
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

#include <arc/coro/eventloop.h>
#include <arc/coro/task.h>
#include <arc/io/socket.h>
#include <arc/logging/logging.h>
#include <arc/net/address.h>

#include <functional>
#include <regex>
#include <thread>

#include "http_config.h"
#include "http_parser.h"

namespace arc {
namespace http {

struct Context {
  arc::io::Socket<arc::net::Domain::IPV4, arc::net::Protocol::TCP,
                  arc::io::Pattern::ASYNC>* conn{nullptr};
};

class HttpServer {
 public:
  HttpServer(const HttpConfig& config = HttpConfig());
  void Start(const std::string& ip = "0.0.0.0", uint16_t port = 8080u);

  void RegisterHandler(
      const std::string& path, HttpMethod method,
      const std::function<arc::coro::Task<void>(
          const HttpRequest*, HttpResponse*, const Context*)>& func);

  void RegisterDefaultHandler(
      arc::http::HttpStatus status,
      const std::function<arc::coro::Task<void>(
          const HttpRequest*, HttpResponse*, const Context*)>& func);

  void RegisterRESTfulHandler(
      const std::string& path, HttpMethod method,
      const std::function<arc::coro::Task<void>(
          const HttpRequest*, HttpResponse*, const Context*)>& func);

 private:
  void InitDefaultHandlers();
  bool Bind(const std::string& ip, uint16_t port);
  arc::coro::Task<void> HandleNewConn(
      arc::io::Socket<arc::net::Domain::IPV4, arc::net::Protocol::TCP,
                      arc::io::Pattern::ASYNC>
          socket_ptr);
  arc::coro::Task<void> StartAccept(int i);
  void InnerStart();

  std::vector<std::shared_ptr<
      arc::io::Acceptor<arc::net::Domain::IPV4, arc::io::Pattern::ASYNC>>>
      listen_socket_ptrs_;
  std::unordered_map<
      std::string,
      std::unordered_map<
          HttpMethod, std::function<arc::coro::Task<void>(
                          const HttpRequest*, HttpResponse*, const Context*)>>>
      handlers_;

  std::unordered_map<arc::http::HttpStatus,
                     std::function<arc::coro::Task<void>(
                         const HttpRequest*, HttpResponse*, const Context*)>>
      default_handlers_;

  std::string GenerateRegexStr(const std::string& path);
  std::vector<std::string> GenerateInitialRESTfulNames(const std::string& path);
  std::unordered_map<
      std::string,
      std::unordered_map<
          HttpMethod,
          std::pair<std::vector<std::string>,
                    std::function<arc::coro::Task<void>(
                        const HttpRequest*, HttpResponse*, const Context*)>>>>
      RESTful_handlers_{};

  HttpConfig config_;
};

}  // namespace http
}  // namespace arc
