/*
 * File: exception.h
 * Project: libarc
 * File Created: Sunday, 13th December 2020 3:29:00 pm
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

#ifndef LIBARC__UTILS__EXCEPTION_H
#define LIBARC__UTILS__EXCEPTION_H

#ifdef __clang__
#else
#include <experimental/source_location>
#endif
#include <iostream>
#include <system_error>

namespace arc {
namespace utils {

#ifdef __clang__
inline void ThrowErrnoExceptions() {
  throw std::system_error(errno, std::generic_category());
}
#else
inline void ThrowErrnoExceptions(
    const std::experimental::source_location& source_location =
        std::experimental::source_location::current()) {
  const std::string file_name = std::string(source_location.file_name());
  const std::string report = file_name.substr(file_name.find_last_of('/') + 1) +
                             ":L" + std::to_string(source_location.line()) +
                             ":" + std::string(source_location.function_name());
  throw std::system_error(errno, std::generic_category(), report);
}
#endif


}  // namespace utils
}  // namespace arc

#endif /* LIBARC__UTILS__EXCEPTION_H */
