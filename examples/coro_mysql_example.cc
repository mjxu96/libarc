/*
 * File: coro_mysql.cc
 * Project: libarc
 * File Created: Tuesday, 13th September 2022 9:53:35 pm
 * Author: Minjun Xu (mjxu96@outlook.com)
 * -----
 * MIT License
 * Copyright (c) 2022 Minjun Xu
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

#include <arc/arc.h>
#include <arc/coro/task.h>
#include <arc/db/mysql.h>

#include <iostream>

using namespace arc::coro;

Task<void> TestConn() {
  arc::db::MySQLConnection conn;
  co_await conn.Connect("127.0.0.1", 3306, "root", "pwd");
  std::cout << "connected" << std::endl;
  auto res = co_await conn.Run("SHOW STATUS;");
  int columns = res.GetColumnCount();
  while (true) {
    auto row = co_await res.FetchNextRow();
    // std::cout << "fetched one row" << std::endl;
    if (row.IsEmpty()) {
      conn.CheckError();
      break;
    }
    std::string output = "";
    for (int i = 0; i < columns; i++) {
      output += std::string(row[i]) + std::string("\t");
    }
    std::cout << output << std::endl;
  }
  co_await conn.Close();
  std::cout << "disconnected" << std::endl;
}

int main(int argc, char** argv) {
  // static_assert(std::movable<arc::db::MySQLResult>);
  std::cout << "arc version: " << arc::Version() << std::endl;
  try {
    /* code */
    StartEventLoop(TestConn());
    // StartEventLoop(MultipleTimers(std::stoi(std::string(argv[1]))));
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
  }

  // test111();
}