/*
 * File: logger.h
 * Project: libarc
 * File Created: Monday, 27th July 2020 8:54:39 pm
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

#ifndef LIBARC__LOGGING__LOGGER_H
#define LIBARC__LOGGING__LOGGER_H

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <exception>
#ifdef __clang__
#else
#include <experimental/source_location>
#endif
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <vector>
// TODO add format

namespace arc {
namespace logging {

struct LoggingFormatWrapper {
#ifdef __clang__
  constexpr LoggingFormatWrapper(const char* format, const char* file_name = "",
                                 const char* func_name = "", unsigned line = 0)
      : format_{format},
        file_name_{file_name},
        func_name_(func_name),
        line_{line} {}
#else
  constexpr LoggingFormatWrapper(
      const char* format,
      const char* file_name =
          std::filesystem::path(
              std::experimental::source_location::current().file_name())
              .filename()
              .c_str(),
      const char* func_name =
          std::experimental::source_location::current().function_name(),
      unsigned line =
          std::experimental::source_location::current().line()) noexcept
      : format_{format},
        file_name_{file_name},
        func_name_(func_name),
        line_{line} {}
#endif

  const char* format_;
  const char* file_name_;
  const char* func_name_;
  unsigned line_;
};

enum Level {
  DEBUG = 0u,
  INFO = 1u,
  WARNING = 2u,
  ERROR = 3u,
  FATAL = 4u,
};

class Logger {
 private:
  std::string logger_name_{};
  std::ostream* out_{&std::cout};
  std::string format_{"[{%time}] [{%filename}:{%line}] [{%level}]\t{%msg}"};
  Level level_{Level::INFO};

  const static std::unordered_map<uint8_t, std::string> level_map_;
  const static std::vector<std::string> support_formats_;

  std::string ConvertToInternalFormat(const std::string& out_format) {
    return out_format;
  }

  std::string GetTimeStr() const {
    std::time_t now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    char buffer[100];
    auto char_cnt =
        std::strftime(buffer, 100, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buffer, char_cnt);
  }

  template <typename... Args>
  std::string GetOutStr(Level log_level, LoggingFormatWrapper format,
                        Args&&... args) const {
    std::string out = format_;
    for (const std::string& support_format : support_formats_) {
      auto pos = out.find(support_format);
      if (pos != std::string::npos) {
        std::string text;
        if (support_format == "{%time}") {
          text = GetTimeStr();
        } else if (support_format == "{%level}") {
          text = level_map_.at((uint8_t)log_level);
        } else if (support_format == "{%line}") {
          text = std::to_string(format.line_);
        } else if (support_format == "{%filename}") {
          text = format.file_name_;
        } else if (support_format == "{%funcname}") {
          text = std::string(format.func_name_);
        } else if (support_format == "{%msg}") {
          char buf[512] = {'\0'};
          if constexpr (sizeof...(args) == 0) {
            text = std::string(format.format_);
          } else {
            int wrote = std::snprintf(buf, 512, format.format_,
                                      std::forward<Args>(args)...);
            if (wrote >= 0) {
              text = std::string(buf, wrote);
            }
          }
        }
        out.replace(pos, support_format.size(), text);
      }
    }
    return out;
  }

 public:
  Logger() = default;
  Logger(const std::string& name) : logger_name_(name) {}
  void SetFormat(const std::string& format) { format_ = format; }
  const std::string& GetName() const { return logger_name_; }

  void SetSink(const std::ostream& out) { out_ = (std::ostream*)&out; }
  void SetLevel(Level level) { level_ = level; }

  template <typename... Args>
  void LogInternal(Level log_level, LoggingFormatWrapper format,
                   Args&&... args) const {
    if (level_ <= log_level) {
      (*out_) << GetOutStr(log_level, format, std::forward<Args>(args)...)
              << std::endl;
    }
  }
  template <typename... Args>
  void LogDebug(LoggingFormatWrapper format, Args&&... args) const {
    LogInternal(Level::DEBUG, format, std::forward<Args&&>(args)...);
  }

  template <typename... Args>
  void LogInfo(LoggingFormatWrapper format, Args&&... args) const {
    LogInternal(Level::INFO, format, std::forward<Args&&>(args)...);
  }

  template <typename... Args>
  void LogWarning(LoggingFormatWrapper format, Args&&... args) const {
    LogInternal(Level::WARNING, format, std::forward<Args&&>(args)...);
  }

  template <typename... Args>
  void LogError(LoggingFormatWrapper format, Args&&... args) const {
    LogInternal(Level::ERROR, format, std::forward<Args&&>(args)...);
  }

  template <typename... Args>
  void LogFatal(LoggingFormatWrapper format, Args&&... args) const {
    LogInternal(Level::FATAL, format, std::forward<Args&&>(args)...);
  }
};
}  // namespace logging
}  // namespace arc

#endif /* LIBARC__LOGGING__LOGGER_H */
