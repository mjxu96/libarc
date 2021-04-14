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
  int err = errno;
  if (err != 0) {
    std::string err_string =
        std::error_code(err, std::generic_category()).message();
    std::string err_code = " [" + std::to_string(err) + "] ";
    msg_ += err_code + err_string;
  }
}

#ifdef __clang__
TLSException::TLSException(const std::string& msg) : ExceptionBase(msg) {
#else
TLSException::TLSException(
    const std::string& msg,
    const std::experimental::source_location& source_location)
    : ExceptionBase(msg, source_location) {
#endif
  msg_ += GetSSLError();
}

std::string TLSException::GetSSLError() {
  int err = ERR_get_error();
  if (err != 0) {
    return std::string(ERR_reason_error_string(err));
  }
  return "";
}
