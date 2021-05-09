/*
 * File: socket_example.cc
 * Project: libarc
 * File Created: Saturday, 12th December 2020 10:03:54 pm
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
using namespace arc::io;
using namespace arc::net;

std::string req = "GET / HTTP/1.1\r\nHost: www.google.com\r\nConnection: keep-alive\r\n\r\n";
std::string response = "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 48\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: Closed\r\n\r\n"
                    "<html><body><h1>Hello, World!</h1></body></html>";

void ConnectTest() {
  Socket sock;
  std::cout << "http connect starts" << std::endl;
  sock.Connect({"www.google.com.hk", 80});
  std::cout << sock.Send(req) << std::endl;
  char result[1024] = {};
  std::string ret;
  int received = 0;
  while (true) {
    received = sock.Recv(result, 1024);
    std::cout << "read " << received << std::endl;
    if (received < 1024) {
      break;
    }
    ret.append(result, received);
  }
  std::cout << ret << std::endl;
}

// void TLSConnectTest() {
//   TLSSocket sock;
//   std::cout << "https connect starts" << std::endl;
//   sock.Connect({"www.baidu.com", 443});
//   std::cout << sock.Send(req) << std::endl;
//   std::cout << sock.Recv() << std::endl;
// }

void HandleClient(Socket<Domain::IPV4, Protocol::TCP, Pattern::SYNC> client) {
  char result[1024] = {};
  auto recv = client.Recv(result, 1024);
  std::cout << recv << std::endl;
  std::cout << client.Send(response) << std::endl;
}

void MoveTest(Acceptor<Domain::IPV4> acceptor) {
  int cnt = 0;
  while (cnt < 3) {
    auto new_sock = acceptor.Accept();
    HandleClient(std::move(new_sock));
    cnt++;
  }
}

void AcceptTest() {
  std::cout << "http accept starts" << std::endl;
  Acceptor<Domain::IPV4, Pattern::SYNC> acceptor;
  acceptor.SetOption(arc::net::SocketOption::REUSEADDR, 1);
  acceptor.Bind({"localhost", 8084});
  acceptor.Listen();
  acceptor.SetNonBlocking(false);
  MoveTest(std::move(acceptor));
}

// void TLSAcceptTest() {
//   std::cout << "https accept start" << std::endl;
//   TLSAcceptor acceptor("/home/minjun/data/keys/server.pem", "/home/minjun/data/keys/server.pem");
//   acceptor.Bind({"localhost", 8084});
//   acceptor.Listen();
//   acceptor.SetNonBlocking(false);
//   int cnt = 0;
//   while (cnt < 3) {
//     try {
//       auto new_sock = acceptor.Accept();
//       std::cout << new_sock.Recv() << std::endl;
//       std::cout << new_sock.Send(response.c_str(), response.size()) << std::endl;
//       new_sock.Shutdown();
//     } catch (const std::exception& e) {
//       std::cout << e.what() << std::endl;
//     }
//     cnt++;
//   }
//   std::cout << "here" << std::endl;
// }

// int main() { AcceptTest(); }
// int main() { 
int main() {
  AcceptTest();
  ConnectTest();
  // TLSConnectTest();
  // TLSAcceptTest();
}
