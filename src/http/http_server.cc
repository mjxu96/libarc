/*
 * File: http_server.cc
 * Project: arc
 * File Created: Sunday, 20th September 2020 8:53:47 pm
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

#include <arc/http/http_server.h>
#include <assert.h>

using namespace arc::http;
using namespace arc::coro;
using namespace arc;

HttpServer::HttpServer(const HttpConfig& config) : config_(config) {
  InitDefaultHandlers();
}

void HttpServer::Start(const std::string& ip, uint16_t port) {
  Bind(ip, port);
  InnerStart();
}

void HttpServer::RegisterHandler(
    const std::string& path, HttpMethod method,
    const std::function<coro::Task<void>(const HttpRequest*, HttpResponse*,
                                         const Context*)>& func) {
  if (handlers_.find(path) == handlers_.end()) {
    handlers_[path] = std::unordered_map<
        HttpMethod, std::function<coro::Task<void>(
                        const HttpRequest*, HttpResponse*, const Context*)>>();
  }
  handlers_[path][method] = func;
  config_.logger->LogInfo("Register handler with method %s and path %s",
                          http_method_str(method), path.c_str());
}

void HttpServer::RegisterDefaultHandler(
    arc::http::HttpStatus status,
    const std::function<coro::Task<void>(const HttpRequest*, HttpResponse*,
                                         const Context*)>& func) {
  default_handlers_[status] = func;
  config_.logger->LogInfo("Register default handler with status %s",
                          http_status_str(status));
  return;
}

void HttpServer::RegisterRESTfulHandler(
    const std::string& path, HttpMethod method,
    const std::function<coro::Task<void>(const HttpRequest*, HttpResponse*,
                                         const Context*)>& func) {
  std::vector<std::string> names = GenerateInitialRESTfulNames(path);
  if (names.empty()) {
    config_.logger->LogWarning(
        "Cannot register RESTful handler because of no {} pair exsits.");
    return;
  }
  std::string regex_str = GenerateRegexStr(path);
  if (RESTful_handlers_.find(regex_str) == RESTful_handlers_.end()) {
    RESTful_handlers_[regex_str] = std::unordered_map<
        HttpMethod,
        std::pair<std::vector<std::string>,
                  std::function<coro::Task<void>(
                      const HttpRequest*, HttpResponse*, const Context*)>>>();
  }
  RESTful_handlers_[regex_str][method] = {names, func};
  config_.logger->LogInfo("Register RESTful handler with method %s and path %s",
                          http_method_str(method), path.c_str());
}

void HttpServer::InitDefaultHandlers() {
  RegisterDefaultHandler(arc::http::HttpStatus::HTTP_STATUS_BAD_REQUEST,
                         [](const HttpRequest* request, HttpResponse* response,
                            const Context* context) -> arc::coro::Task<void> {
                           response->status =
                               arc::http::HttpStatus::HTTP_STATUS_BAD_REQUEST;
                           response->body = "400 Bad request";
                           co_return;
                         });
  RegisterDefaultHandler(arc::http::HttpStatus::HTTP_STATUS_NOT_FOUND,
                         [](const HttpRequest* request, HttpResponse* response,
                            const Context* context) -> arc::coro::Task<void> {
                           response->status =
                               arc::http::HttpStatus::HTTP_STATUS_NOT_FOUND;
                           response->body = "404 Not found";
                           co_return;
                         });
}

bool HttpServer::Bind(const std::string& ip, uint16_t port) {
  for (int i = 0; i < config_.working_thread_num; i++) {
    auto listen_socket_ptr = std::make_shared<
        io::Acceptor<arc::net::Domain::IPV4, arc::io::Pattern::ASYNC>>();
    listen_socket_ptr->SetOption(arc::net::SocketOption::REUSEADDR, 1);
    listen_socket_ptr->SetOption(arc::net::SocketOption::REUSEPORT, 1);
    listen_socket_ptr->Bind({ip, port});
    listen_socket_ptrs_.push_back(listen_socket_ptr);
  }
  return true;
}

Task<void> HttpServer::HandleNewConn(
    io::Socket<arc::net::Domain::IPV4, arc::net::Protocol::TCP,
               arc::io::Pattern::ASYNC>
        socket) {
  bool is_need_return = true;
  HttpParser parser(HTTP_REQUEST);
  HttpRequest* request = new HttpRequest{};
  std::shared_ptr<char[]> data(new char[config_.read_buffer_size]);
  do {
    std::string recv;
    while (true) {
      auto recv_bytes =
          co_await socket.Recv(data.get(), config_.read_buffer_size);
      if (recv_bytes <= 0) {
        break;
      }
      recv.append(data.get(), recv_bytes);
      if (recv_bytes < config_.read_buffer_size) {
        break;
      }
    }

    if (recv.empty()) {
      is_need_return = true;
    } else {
      config_.logger->LogDebug("Read content %s from client", recv.c_str());
      auto ret = parser.ParseOnce(recv, request);
      if (ret == 0 && !request->is_complete) {
        is_need_return = false;
        continue;
      }
      HttpResponse* response = new HttpResponse();
      const Context* context = new Context{.conn = &socket};
      if (ret != 0) {
        config_.logger->LogDebug("Receive bad request from %s:%u, content: %s",
                                 socket.GetAddr().GetHost().c_str(),
                                 socket.GetAddr().GetPort(), recv.c_str());
        co_await default_handlers_
            [arc::http::HttpStatus::HTTP_STATUS_BAD_REQUEST](request, response,
                                                             context);
        is_need_return = false;
      } else {
        // Keep alive setting
        unsigned short http_version =
            request->http_major_version * 10 + request->http_minor_version;
        auto keep_alive_itr = request->headers.find("Connection");
        if (http_version < 11) {
          if (keep_alive_itr != request->headers.end() &&
              keep_alive_itr->second == "Keep-alive") {
            is_need_return = false;
          } else {
            is_need_return = true;
          }
        } else {
          if (keep_alive_itr != request->headers.end() &&
              keep_alive_itr->second == "close") {
            config_.logger->LogDebug("Client requests closing the socket.");
            is_need_return = true;
          } else {
            is_need_return = false;
          }
        }
        if (handlers_.find(request->path) == handlers_.end() ||
            handlers_[request->path].find(request->method) ==
                handlers_[request->path].end()) {
          // First search RESTful handlers
          bool is_RESTful_handler_found = false;
          for (auto& [RESTful_regex_str, handler_method_map] :
               RESTful_handlers_) {
            if (handler_method_map.find(request->method) ==
                handler_method_map.end()) {
              continue;
            }
            std::regex current_regex(RESTful_regex_str);
            std::smatch match;
            if (std::regex_search(request->path, match, current_regex)) {
              for (int i = 1; i < match.size(); i++) {
                request->RESTful_params.insert(
                    {handler_method_map[request->method].first[i - 1],
                     match[i].str()});
              }
              is_RESTful_handler_found = true;
              response->http_major_version = request->http_major_version;
              response->http_minor_version = request->http_minor_version;
              co_await handler_method_map[request->method].second(
                  request, response, context);
              break;
            }
          }

          if (!is_RESTful_handler_found) {
            config_.logger->LogDebug("Unkown caught %s request %s",
                                     request->method_string.c_str(),
                                     request->path.c_str());
            co_await default_handlers_
                [arc::http::HttpStatus::HTTP_STATUS_NOT_FOUND](
                    request, response, context);
          }

        } else {
          response->http_major_version = request->http_major_version;
          response->http_minor_version = request->http_minor_version;
          co_await handlers_[request->path][request->method](request, response,
                                                             context);
        }
      }

      try {
        auto response_ptr = std::make_shared<std::string>(
            std::move(GetReturnStringFromHttpResponse(response)));
        co_await socket.Send(response_ptr->c_str(), response_ptr->size());
      } catch (const std::exception& e) {
        config_.logger->LogWarning(e.what());
      }

      delete request;
      request = new HttpRequest();
      delete response;
      delete context;
      parser = HttpParser(HTTP_REQUEST);
    }

  } while (!is_need_return);
  if (request) {
    delete request;
  }
  config_.logger->LogDebug("Connection lost");
}

Task<void> HttpServer::StartAccept(int i) {
  config_.logger->LogDebug("Start waiting accept with thread: 0x%x",
                           std::this_thread::get_id());
  while (true) {
    auto new_socketr = co_await listen_socket_ptrs_[i]->Accept();
    config_.logger->LogDebug("Accept one conn on thread: 0x%x",
                             std::this_thread::get_id());
    arc::coro::EnsureFuture(HandleNewConn(std::move(new_socketr)));
  }
  co_return;
}

void HttpServer::InnerStart() {
  std::vector<std::thread> threads;
  for (int i = 0; i < config_.working_thread_num; i++) {
    threads.emplace_back([i, this]() {
      listen_socket_ptrs_[i]->Listen();
      StartEventLoop(StartAccept(i));
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

std::string HttpServer::GenerateRegexStr(const std::string& path) {
  std::string valid_url_word_regex =
      "([0-9a-zA-Z%\\.\\-_~!\\$&'\\(\\)\\*\\+,;=:@]+)";
  std::regex replace_bracket_regex("(\\{\\w+\\})");
  return std::regex_replace(path, replace_bracket_regex, valid_url_word_regex);
}

std::vector<std::string> HttpServer::GenerateInitialRESTfulNames(
    const std::string& path) {
  std::vector<std::string> names;
  std::regex replace_bracket_regex("(\\{\\w+\\})");
  auto word_begin_itr =
      std::sregex_iterator(path.begin(), path.end(), replace_bracket_regex);
  auto word_end_itr = std::sregex_iterator();
  for (auto itr = word_begin_itr; itr != word_end_itr; itr++) {
    std::string name = itr->str();
    if (name.size() <= 2 || name.front() != '{' || name.back() != '}') {
      return {};
    }
    names.push_back(name.substr(1, name.size() - 2));
  }
  return names;
}
