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

#include <arc/utils/nameof.hpp>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>

#include "utils.h"

namespace arc {
namespace net {

template <Domain AF = Domain::IPV4>
class Address {
 public:
  Address() = default;

  Address(const std::string& host, std::uint16_t port)
      : host_(host), port_(port) {
    if constexpr (AF == Domain::IPV4) {
      addr_.sin_port = htons(port_);
      if (inet_pton(AF_INET, host_.data(), &addr_.sin_addr) > 0) {
        is_valid_ = true;
        addr_.sin_family = AF_INET;
      } else {
        InitDnsAddress(host, port);
      }
    } else if constexpr (AF == Domain::IPV6) {
      addr_.sin6_port = htons(port_);
      if (inet_pton(AF_INET6, host_.data(), &addr_.sin6_addr) > 0) {
        is_valid_ = true;
        addr_.sin6_family = AF_INET6;
      } else {
        InitDnsAddress(host, port);
      }
    } else {
      throw std::logic_error("Address family cannot be supported");
    }
  }

  template <Domain UAF = AF>
  requires(UAF == Domain::IPV4) Address(const sockaddr_in& addr) {
    Init(addr);
  }

  template <Domain UAF = AF>
  requires(UAF == Domain::IPV6) Address(const sockaddr_in6& addr) {
    Init(addr);
  }

  // TODO throw exception when the address is invalid.
  const sockaddr* GetCStyleAddress() const { return (sockaddr*)&addr_; }

  const bool IsValid() const { return is_valid_; }

  const std::size_t AddressSize() const { return sizeof(addr_); }

  const std::string GetHost() const { return host_; }

  const uint16_t GetPort() const { return port_; }

 private:
  using CAddressType = typename std::conditional_t<(AF == Domain::IPV4),
                                                   sockaddr_in, sockaddr_in6>;

  void Init(const CAddressType& addr, bool is_from_dns = false) {
    if (((int)AF) != ((sockaddr*)&addr)->sa_family) {
      throw std::logic_error("Trying to attch a " +
                             std::string(nameof::nameof_enum<Domain>(
                                 (Domain)((sockaddr*)&addr)->sa_family)) +
                             " address with a " +
                             std::string(nameof::nameof_enum<Domain>(AF)) +
                             " address");
    }
    addr_ = addr;
    char tmp[INET6_ADDRSTRLEN];
    if constexpr (AF == Domain::IPV4) {
      inet_ntop(AF_INET, &(addr_.sin_addr), tmp, INET_ADDRSTRLEN);
    } else if constexpr (AF == Domain::IPV6) {
      inet_ntop(AF_INET6, &(addr_.sin6_addr), tmp, INET6_ADDRSTRLEN);
    } else {
      is_valid_ = false;
      return;
    }
    host_ = tmp;
    if (is_from_dns) {
      if constexpr (AF == Domain::IPV4) {
        port_ = ntohs(addr_.sin_port);
      } else if constexpr (AF == Domain::IPV6) {
        port_ = ntohs(addr_.sin6_port);
      }
    } else {
      if constexpr (AF == Domain::IPV4) {
        port_ = addr_.sin_port;
      } else if constexpr (AF == Domain::IPV6) {
        port_ = addr_.sin6_port;
      }
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
      Init(*((CAddressType*)(result->ai_addr)), true);
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
  CAddressType addr_{};
};

}  // namespace net
}  // namespace arc

#endif /* LIBARC__NET__ADDRESS_H */
