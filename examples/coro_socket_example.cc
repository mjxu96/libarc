/*
 * File: coro_socket_example.cc
 * Project: libarc
 * File Created: Sunday, 13th December 2020 6:01:29 pm
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

#include <arc/io/socket.h>
#include <arc/coro/task.h>

using namespace arc::io;
using namespace arc::net;
using namespace arc::coro;

Task<void> HandleClient(Socket<Domain::IPV4, Protocol::TCP, Pattern::ASYNC> sock) {
  auto recv = co_await sock.Recv();
  std::cout << recv << std::endl;
  std::cout << co_await sock.Send(recv) << std::endl;
}

Task<void> Listen() {
  Socket<Domain::IPV4, Protocol::TCP, Pattern::ASYNC> sock;
  sock.Bind({"localhost", 8083});
  sock.Listen();
  std::cout << "start accepting" << std::endl;
  while (true) {
    auto in_sock = co_await sock.Accept();
    co_await HandleClient(std::move(in_sock));
  }
  co_return;
}

int main() {
  StartEventLoop(Listen());
  std::cout << "finished" << std::endl;
}