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
#include <thread>

using namespace arc::io;
using namespace arc::net;
using namespace arc::coro;

const std::string ret = "HTTP/1.1 200 OK\r\nContent-Length: 140\r\nContent-Type: text/plain\r\n\r\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

Task<void> HandleClient(Socket<Domain::IPV4, Protocol::TCP, Pattern::ASYNC> sock) {
  // std::cout << "start handling one client" << std::endl;
  while (true) {
    // try
    // {
    //   /* code */
    // }
    // catch(const std::exception& e)
    // {
    //   std::cerr << e.what() << '\n';
    // }
    
    auto recv = co_await sock.Recv();
    // std::cout << recv << std::endl;
    if (recv.size() == 0) {
      break;
    }
    // std::cout << co_await sock.Send(co_await sock.Recv()) << std::endl;
    co_await sock.Send(ret);
  }
}

Task<void> Listen() {
  Acceptor<Domain::IPV4, Pattern::ASYNC> accpetor;
  accpetor.SetOption(arc::net::SocketOption::REUSEADDR, 1);
  accpetor.SetOption(arc::net::SocketOption::REUSEPORT, 1);
  accpetor.Bind({"localhost", 8086});
  accpetor.Listen();
  std::cout << "start accepting" << std::endl;
  int i = 0;
  while (true) {
    auto in_sock = co_await accpetor.Accept();
    arc::coro::EnsureFuture(HandleClient(std::move(in_sock)));
    i++;
    std::cout << "thread: 0x" << std::hex << std::this_thread::get_id() << std::dec << " handled " << i << " clients" << std::endl;
  }
  co_return;
}

void Start() {
  StartEventLoop(Listen());
}

int main(int argc, char** argv) {
  std::vector<std::thread> threads;
  int thread_num = std::stoi(std::string(argv[1]));
  for (int i = 0; i < thread_num; i++) {
    threads.emplace_back(Start);
    // threads.back().detach();
  }

  for (int i = 0; i < thread_num; i++) {
    threads[i].join();
  }

  std::cout << "finished" << std::endl;
}