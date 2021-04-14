/*
 * File: base.cc
 * Project: libarc
 * File Created: Wednesday, 14th April 2021 8:31:32 pm
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

#include <arc/exception/base.h>

using namespace arc::exception::detail;

ExceptionBase::ExceptionBase(
#ifdef __clang__
    const std::string& msg) {
#else
    const std::string& msg,
    const std::experimental::source_location& source_location)
    : msg_(msg), source_location_(source_location) {
  const std::string file_name = std::string(source_location.file_name());
  exception_position_ = file_name.substr(file_name.find_last_of('/') + 1) +
                        ":L" + std::to_string(source_location.line()) + ":" +
                        std::string(source_location.function_name()) + " ";
#endif
  msg_ = exception_position_ + msg_;
}

const char* ExceptionBase::what() const noexcept { return msg_.c_str(); }

#ifdef __clang__
ErrnoException::ErrnoException(const std::string& msg) : ExceptionBase(msg) {
#else
ErrnoException::ErrnoException(
    const std::string& msg,
    const std::experimental::source_location& source_location)
    : ExceptionBase(msg, source_location) {
#endif
  int err = errno;
  if (err != 0) {
    std::string err_string =
        std::error_code(err, std::generic_category()).message();
    std::string err_code = " [" + std::to_string(err) + "] ";
    msg_ += err_code + err_string;
  }
}
