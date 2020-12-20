/*
 * File: logging.cc
 * Project: libarc
 * File Created: Monday, 27th July 2020 10:13:46 pm
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

#include <arc/logging/logging.h>

std::unordered_map<std::string, arc::logging::Logger> loggers = {
    {"", arc::logging::Logger("")}};
arc::logging::Logger* arc::logging::default_logger = &loggers[""];

arc::logging::Logger& arc::logging::GetLogger(const std::string& name) {
  if (loggers.find(name) == loggers.end()) {
    loggers.insert({name, arc::logging::Logger(name)});
  }
  return loggers[name];
}

void arc::logging::SetFormat(const std::string& format) {
  default_logger->SetFormat(format);
}

void arc::logging::SetSink(const std::ostream& out) {
  default_logger->SetSink(out);
}

void arc::logging::SetLevel(arc::logging::Level level) {
  default_logger->SetLevel(level);
}
