/*
 * File: coro_http_client_example.cc
 * Project: libarc
 * File Created: Wednesday, 14th September 2022 1:39:10 am
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

#include <arc/coro/task.h>
#include <arc/http/http_client.h>
#include <curl/curl.h>

using namespace arc::http;

void PrintRequest(const HttpRequest& request) {
  std::cout << "request queries are: " << std::endl;
  for (const auto& [query_key, query_value] : request.queries) {
    std::cout << "\t" << query_key << ": " << query_value << std::endl;
  }
  std::cout << "request headers are: " << std::endl;
  for (const auto& [header_key, header_value] : request.headers) {
    std::cout << "\t" << header_key << ": " << header_value << std::endl;
  }
  std::cout << "request body is: " << std::endl;
  std::cout << request.body << std::endl;
}

void PrintResponse(const HttpResponse& response) {
  std::cout << "response status is: " << std::endl;
  std::cout << "\t" << response.status << " " << response.status_string
            << std::endl;
  std::cout << "response headers are: " << std::endl;
  for (const auto& [header_key, header_value] : response.headers) {
    std::cout << "\t" << header_key << ": " << header_value << std::endl;
  }
  std::cout << "response body is: " << std::endl;
  std::cout << response.body << std::endl;
}

arc::coro::Task<void> InnerTask() {
  // %E5%A5%A5%E9%87%91%E6%96%A7
  HttpClient client({"tavern.blizzard.cn", 443});
  HttpRequest request;
  request.path = "/action/api/common/v3/wow/classic/meetinghorn/list";
  std::cout << request.path << std::endl;
  request.method = HttpMethod::HTTP_GET;
  std::string s = "奥金斧";
  request.queries = {
      {"realm", s},         {"unionId", "null"},  {"pid", "5"},
      {"faction", "Horde"}, {"activityName", ""}, {"keyword", ""},
      {"activityMode", ""}, {"p", "2"},           {"pageSize", "10"}};
  request.headers = {
      {"Host", "tavern.blizzard.cn"},
  };
  PrintRequest(request);
  std::cout << arc::http::GetReturnStringFromHttpRequest(request) << std::endl;
  std::cout << "---------------------------" << std::endl;
  auto response = co_await client.Request(request);
  if (!response.is_valid || !response.is_complete) {
    std::cout << "not valid response" << std::endl;
    co_return;
  }
  PrintResponse(response);
  co_return;
}

int main() {
  arc::coro::StartEventLoop(InnerTask());
  return 0;
}