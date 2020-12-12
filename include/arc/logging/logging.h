/*
 * File: logging.h
 * Project: libarc
 * File Created: Monday, 27th July 2020 10:12:04 pm
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

#ifndef _MO_LOGGING_LOGGING_H_
#define _MO_LOGGING_LOGGING_H_

#include "logger.h"

namespace mo {
namespace logging {

Logger& GetLogger(const std::string& name);

extern Logger* default_logger;

template <typename... Args>
void LogDebug(LoggingFormatWrapper format, Args&&... args) {
  default_logger->LogInternal(Level::DEBUG, format,
                              std::forward<Args&&>(args)...);
}

template <typename... Args>
void LogInfo(LoggingFormatWrapper format, Args&&... args) {
  default_logger->LogInternal(Level::INFO, format, std::forward<Args&&>(args)...);
}

template <typename... Args>
void LogWarning(LoggingFormatWrapper format, Args&&... args) {
  default_logger->LogInternal(Level::WARNING, format,
                              std::forward<Args&&>(args)...);
}

template <typename... Args>
void LogError(LoggingFormatWrapper format, Args&&... args) {
  default_logger->LogInternal(Level::ERROR, format,
                              std::forward<Args&&>(args)...);
}

template <typename... Args>
void LogFatal(LoggingFormatWrapper format, Args&&... args) {
  default_logger->LogInternal(Level::FATAL, format,
                              std::forward<Args&&>(args)...);
}

void SetFormat(const std::string& format);
void SetSink(const std::ostream& out);
void SetLevel(Level level);

}  // namespace logging
}  // namespace mo

#endif