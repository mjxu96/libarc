/*
 * File: address.h
 * Project: libarc
 * File Created: Saturday, 12th December 2020 7:56:52 pm
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

#ifndef LIBARC__NET__ADDRESS_H
#define LIBARC__NET__ADDRESS_H

#include <arpa/inet.h>
#include <netdb.h>

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>

#include "utils.h"
#include <arc/utils/nameof.hpp>

namespace arc {
namespace net {

template <Domain AF = Domain::IPV4>
class Address {
 public:
  Address() = default;

  Address(const std::string& host, std::uint16_t port)
      : host_(host), port_(port) {
    addr_.sin_port = htons(port_);
    if constexpr (AF == Domain::IPV4) {
      if (inet_pton(AF_INET, host_.data(), &addr_.sin_addr) > 0) {
        is_valid_ = true;
        addr_.sin_family = AF_INET;
      } else {
        InitDnsAddress(host, port);
      }
    } else if constexpr (AF == Domain::IPV6) {
      if (inet_pton(AF_INET, host_.data(), &addr_.sin_addr) > 0) {
        is_valid_ = true;
        addr_.sin_family = AF_INET;
      } else {
        InitDnsAddress(host, port);
      }
    } else {
      throw std::logic_error("Address family cannot be supported");
    }
  }

  Address(const sockaddr_in& addr) { Init(addr); }

  // TODO throw exception when the address is invalid.
  const sockaddr* GetCStyleAddress() const { return (sockaddr*)&addr_; }

  const bool IsValid() const { return is_valid_; }

  const std::size_t AddressSize() const { return sizeof(addr_); }

  const std::string GetHost() const { return host_; }

  const uint16_t GetPort() const { return port_; }

 private:
  void Init(const sockaddr_in& addr, bool is_from_dns = false) {
    addr_ = addr;
    char tmp[INET6_ADDRSTRLEN];
    switch (addr.sin_family) {
      case AF_INET:
        inet_ntop(AF_INET, &(addr.sin_addr), tmp, INET_ADDRSTRLEN);
        break;

      case AF_INET6:
        inet_ntop(AF_INET6, &(addr.sin_addr), tmp, INET6_ADDRSTRLEN);
        break;
      default:
        is_valid_ = false;
        return;
    }
    host_ = tmp;
    if (is_from_dns) {
      port_ = ntohs(addr_.sin_port);
    } else {
      port_ = addr_.sin_port;
    }
    is_valid_ = true;
  }

  void InitDnsAddress(const std::string& host, std::uint16_t port) {
    struct addrinfo hints;
    struct addrinfo* result = nullptr;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = static_cast<int>(AF);
    hints.ai_socktype = 0; /* TODO: is it correct? */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    int s = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints,
                        &result);
    if (s == 0 && result != nullptr) {
      Init(*((struct sockaddr_in*)result->ai_addr), true);
      freeaddrinfo(result);
    } else {
      freeaddrinfo(result);
      throw std::logic_error("Cannot resolve " + host + ":" +
                             std::to_string(port) + " to address family " +
                             std::string(nameof::nameof_enum<AF>()));
    }
  }

  bool is_valid_{false};
  std::string host_{};
  uint16_t port_{};
  sockaddr_in addr_{};
};

}  // namespace net
}  // namespace arc

#endif /* LIBARC__NET__ADDRESS_H */
