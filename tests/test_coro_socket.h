/*
 * File: test_coro_socket.cc
 * Project: libarc
 * File Created: Tuesday, 11th May 2021 9:37:29 pm
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

#ifndef LIBARC__TESTS__TEST_CORO_SOCKET_H
#define LIBARC__TESTS__TEST_CORO_SOCKET_H

#include "utils.h"

#include <arc/coro/eventloop.h>
#include <arc/coro/task.h>
#include <arc/io/socket.h>
#include <arc/io/tls_socket.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>

namespace arc {
namespace test {

enum class SocketCoroTestType {
  BASIC = 0U,
  TLS,
};

template <SocketCoroTestType T, net::Domain AF, net::Protocol P>
class SocketCoroTestTypeTrait {
 public:
  constexpr static SocketCoroTestType socket_type = T;
  constexpr static net::Domain af = AF;
  constexpr static net::Protocol p = P;
};

template <typename T>
class SocketCoroTest : public ::testing::Test {
 protected:
  using SocketType =
      typename std::conditional_t<T::socket_type == SocketCoroTestType::BASIC,
                                  io::Socket<T::af, T::p, io::Pattern::ASYNC>,
                                  io::TLSSocket<T::af, io::Pattern::ASYNC>>;
  using AcceptorType =
      typename std::conditional_t<T::socket_type == SocketCoroTestType::BASIC,
                                  io::Acceptor<T::af, io::Pattern::ASYNC>,
                                  io::TLSAcceptor<T::af, io::Pattern::ASYNC>>;

  std::uint16_t port_{0};
  const static int kSendLength_ = 2047;
  std::string transferred_string_ = std::string(kSendLength_, 'a');
  const static int kClientsCount_ = 5;
  const static int kMaxSleepTime_ = 400;
  const static int kMinSleepTime_ = 100;
  constexpr static int kExpectedRepeatTimes_ = 5;

  float max_allowed_ref_error_ = 0.01;
  int clients_sleep_time_[kClientsCount_] = {0};

  std::string key_dir_;

  virtual void SetUp() override {
    key_dir_ = GetHomeDir() + std::string("/data/keys/");
    if (IsRunningWithValgrind()) {
      max_allowed_ref_error_ = 0.2;
    }
    std::srand(std::time(nullptr));
    for (int i = 0; i < kClientsCount_; i++) {
      clients_sleep_time_[i] =
          std::rand() % (kMaxSleepTime_ - kMinSleepTime_) + kMinSleepTime_;
    }
  }

  coro::Task<void> SomeLongRunTask(int milliseconds) {
    co_await arc::coro::SleepFor(std::chrono::milliseconds(milliseconds));
    co_return;
  }

  coro::Task<void> HandleClient(SocketType client) {
    // verify the address
    auto local_address = client.GetLocalAddress();
    EXPECT_EQ(local_address.GetHost(), "127.0.0.1");
    EXPECT_EQ(local_address.GetPort(), port_);

    int i = 0;
    int received_size_per_read = kSendLength_ / 2 + 1;
    std::unique_ptr<char[]> data(new char[received_size_per_read]);
    bool connection_alive = true;

    // first get sleep time
    std::string sleep_str;
    auto recv = co_await client.Recv(data.get(), received_size_per_read);
    sleep_str.append(data.get(), recv);

    while (true) {
      std::string received;
      while (true) {
        auto recv = co_await client.Recv(data.get(), received_size_per_read);
        received.append(data.get(), recv);
        if (recv < received_size_per_read) {
          if (recv == 0) {
            connection_alive = false;
          }
          break;
        }
      }
      if (connection_alive) {
        // mimic a long time job
        co_await SomeLongRunTask(std::stoi(sleep_str));
        EXPECT_EQ(received, transferred_string_);
        std::unique_ptr<char[]> sent_data(new char[received.size()]);
        std::memcpy(sent_data.get(), received.c_str(), received.size());
        int ret = co_await client.Send(sent_data.get(), received.size());
        EXPECT_EQ(ret, received.size());
      } else {
        break;
      }
      i++;
    }
    EXPECT_EQ(i, this->kExpectedRepeatTimes_);
  }

  coro::Task<void> ClientsConnect() {
    std::vector<SocketType> socks;
    for (int i = 0; i < kClientsCount_; i++) {
      socks.emplace_back();
      co_await socks.back().Connect({"localhost", port_});
    }

    for (int i = 0; i < kClientsCount_; i++) {
      coro::EnsureFuture(ClientReadWrite(i, std::move(socks.back())));
      socks.pop_back();
    }
    co_return;
  }

  coro::Task<void> ClientReadWrite(int client_id, SocketType sock) {
    // verify the address
    auto local_address = sock.GetPeerAddress();
    EXPECT_EQ(local_address.GetHost(), "127.0.0.1");
    EXPECT_EQ(local_address.GetPort(), port_);

    int i = 0;
    int received_size_per_read = kSendLength_;
    std::unique_ptr<char[]> data(new char[kSendLength_]);
    bool connection_alive = true;

    int expected_sleep_time = clients_sleep_time_[client_id];

    // first sent the sleep time
    std::string sleep_str = std::to_string(clients_sleep_time_[client_id]);
    int sent = co_await sock.Send(sleep_str.c_str(), sleep_str.size());

    // wait 100 milliseconds to ensure it is sent
    co_await coro::SleepFor(std::chrono::milliseconds(10));

    while (i < kExpectedRepeatTimes_) {
      std::string received;
      i++;
      int sent = co_await sock.Send(transferred_string_.c_str(),
                                    transferred_string_.size());

      auto now = std::chrono::steady_clock::now();
      int recv = co_await sock.Recv(data.get(), received_size_per_read);
      auto then = std::chrono::steady_clock::now();

      EXPECT_EQ(recv, received_size_per_read);

      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(then - now)
              .count();
      EXPECT_NEAR(
          elapsed,
          expected_sleep_time, expected_sleep_time * max_allowed_ref_error_);

      received.append(data.get(), received_size_per_read);
      EXPECT_EQ(received, transferred_string_);
    }
  }

  template <typename UT = T>
  requires(UT::socket_type == SocketCoroTestType::BASIC) AcceptorType
      InitAcceptor() {
    return AcceptorType();
  }

  template <typename UT = T>
  requires(UT::socket_type == SocketCoroTestType::TLS) AcceptorType
      InitAcceptor() {
    return AcceptorType(key_dir_ + "cert.pem",
                        key_dir_ + "key.pem");
  }

  coro::Task<void> Accept() {
    AcceptorType acceptor = InitAcceptor<T>();
    acceptor.SetOption(arc::net::SocketOption::REUSEADDR, 1);
    acceptor.Bind({"localhost", 0});
    acceptor.Listen();

    auto accept_addr = acceptor.GetLocalAddress();
    port_ = accept_addr.GetPort();
    EXPECT_EQ(accept_addr.GetHost(), "127.0.0.1");

    coro::EnsureFuture(ClientsConnect());

    int i = 0;
    while (i < kClientsCount_) {
      auto in_sock = co_await acceptor.Accept();
      coro::EnsureFuture(HandleClient(std::move(in_sock)));
      i++;
    }
  }
};

using SocketTestTypes = ::testing::Types<
    SocketCoroTestTypeTrait<SocketCoroTestType::BASIC, net::Domain::IPV4,
                            net::Protocol::TCP>,
    SocketCoroTestTypeTrait<SocketCoroTestType::TLS, net::Domain::IPV4,
                            net::Protocol::TCP>>;

TYPED_TEST_CASE(SocketCoroTest, SocketTestTypes);

TYPED_TEST(SocketCoroTest, BasicSocketTest) {
  coro::StartEventLoop(this->Accept());
}

}  // namespace test
}  // namespace arc

#endif