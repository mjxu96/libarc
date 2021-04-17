/*
 * File: io.cc
 * Project: libarc
 * File Created: Wednesday, 14th April 2021 7:33:28 pm
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

#include <arc/exception/io.h>
#include <arc/utils/bits.h>
#include <openssl/err.h>

#include <system_error>

using namespace arc::exception;
using namespace arc::exception::detail;

#ifdef __clang__
IOException::IOException(const std::string& msg) : ErrnoException(msg) {
#else
IOException::IOException(
    const std::string& msg,
    const std::experimental::source_location& source_location)
    : ErrnoException(msg, source_location) {
#endif
}

#ifdef __clang__
TLSException::TLSException(const std::string& msg, int ssl_err)
    : ExceptionBase(msg), ssl_err_(ssl_err) {
#else
TLSException::TLSException(
    const std::string& msg, int ssl_err,
    const std::experimental::source_location& source_location)
    : ExceptionBase(msg, source_location), ssl_err_(ssl_err) {
#endif
  msg_ += GetSSLError();
}

std::string TLSException::GetSSLError() {
  if (ssl_err_ == SSL_ERROR_SSL || ssl_err_ == SSL_ERROR_SYSCALL) {
    int err = 0;
    std::string err_str;
    bool is_first_get_error = true;
    while (true) {
      err = ERR_get_error();
      if (err == 0) {
        if (is_first_get_error) {
          err_str =
              std::error_code(errno, std::generic_category()).message();
          std::string err_code = " [" + arc::utils::GetHexString(errno) + "] ";
          err_str = err_code + err_str;
        }
        break;
      } else {
        is_first_get_error = false;
        err_str += " [" + arc::utils::GetHexString(err) + "] " +
                  std::string(ERR_reason_error_string(err));
      }
    }
    return err_str;
  } else {
    return " [SSL_get_error() " + arc::utils::GetHexString(ssl_err_) + "]";
  }
}
