/*
 * File: coro_http_example.cc
 * Project: libarc
 * File Created: Wednesday, 14th September 2022 1:32:23 am
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

#include <arc/arc.h>
#include <arc/http/http_server.h>
#include <arc/logging/logging.h>

int main(int argc, char** argv) {
  arc::logging::SetLevel(arc::logging::Level::INFO);
  arc::logging::LogInfo("arc version: %s", arc::Version().c_str());
  arc::http::HttpServer server;
  server.RegisterHandler(
      "/", arc::http::HttpMethod::HTTP_GET,
      [](const arc::http::HttpRequest* request,
         arc::http::HttpResponse* response,
         const arc::http::Context* context) -> arc::coro::Task<void> {
        response->body = request->body;
        co_return;
      });
  server.RegisterHandler(
      "/info", arc::http::HttpMethod::HTTP_POST,
      [](const arc::http::HttpRequest* request,
         arc::http::HttpResponse* response,
         const arc::http::Context* context) -> arc::coro::Task<void> {
        std::cout << "query method is: " << std::endl;
        std::cout << "\t" << request->method_string << std::endl;
        std::cout << "query path is: " << std::endl;
        std::cout << "\t" << request->path << std::endl;
        std::cout << "http version is: " << std::endl;
        std::cout << "\t" << request->http_major_version << "."
                  << request->http_minor_version << std::endl;
        std::cout << "query strings are: " << std::endl;
        for (const auto& [query_key, query_value] : request->queries) {
          std::cout << "\t" << query_key << ": " << query_value << std::endl;
        }
        std::cout << "headers are: " << std::endl;
        for (const auto& [key, value] : request->headers) {
          std::cout << "\t" << key << ": " << value << std::endl;
        }
        std::cout << "http body is: " << std::endl;
        std::cout << request->body << std::endl;

        response->body = request->body;
        // Mimic a long time function
        // co_await arc::coro::SleepFor(std::chrono::seconds(2));
        co_return;
      });
  server.RegisterRESTfulHandler(
      "/api/v1/{name}/{id}", arc::http::HttpMethod::HTTP_GET,
      [](const arc::http::HttpRequest* request,
         arc::http::HttpResponse* response,
         const arc::http::Context* context) -> arc::coro::Task<void> {
        std::cout << "get a RESTful request" << std::endl;
        std::cout << request->RESTful_params.find("name")->second << std::endl;
        std::cout << request->RESTful_params.find("id")->second << std::endl;
        // Mimic a long time function
        // co_await arc::coro::SleepFor(std::chrono::seconds(2));
        co_return;
      });
  server.Start();
  return 0;
}
