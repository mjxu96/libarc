/*
 * File: base.h
 * Project: libarc
 * File Created: Wednesday, 14th April 2021 8:29:40 pm
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

#ifndef LIBARC__EXCEPTION__BASE_H
#define LIBARC__EXCEPTION__BASE_H

#ifdef __clang__
#else
#include <experimental/source_location>
#endif
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>


namespace arc {
namespace exception {

namespace detail {

class ExceptionBase : public std::exception {
 public:
#ifdef __clang__
  ExceptionBase(const std::string& msg);
#else
  ExceptionBase(const std::string& msg,
                const std::experimental::source_location& source_location);
#endif
  virtual ~ExceptionBase() = default;
  virtual const char* what() const noexcept;

 protected:
  std::string exception_position_{};
  std::string msg_{};
#ifdef __clang__
#else
  std::experimental::source_location source_location_{};
#endif
};

class ErrnoException : public ExceptionBase {
 public:
#ifdef __clang__
  ErrnoException(const std::string& msg = "");
#else
  ErrnoException(const std::string& msg = "",
              const std::experimental::source_location& source_location =
                  std::experimental::source_location::current());
#endif
};


};  // namespace detail
};
};

#endif