/*
 * File: mysql.cc
 * Project: libarc
 * File Created: Tuesday, 13th September 2022 9:17:07 pm
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

#include <arc/coro/awaiter/io_awaiter.h>
#include <arc/db/mysql.h>
#include <arc/exception/db.h>

#include <iostream>
#include <mutex>

using namespace arc::db;

static std::mutex mysql_connection_lock;


  const unsigned int MySQLResult::GetColumnCount() const {
    if (res_) {
      return mysql_num_fields(res_);
    }
    return mysql_field_count(&(conn_->mysql_));
  }

  arc::coro::Task<MySQLResultRow> MySQLResult::FetchNextRow() {
    MySQLResultRow row;
    int status = mysql_fetch_row_start(&(row.row_), res_);
    while (status) {
      status = co_await conn_->WaitForMySQL(status);
      status = mysql_fetch_row_cont(&(row.row_), res_, status);
    }
    co_return row;
  }

MySQLConnection::MySQLConnection() : SQLConnection(SQLType::TYPE_MYSQL) {
  mysql_connection_lock.lock();
  mysql_init(&mysql_);
  mysql_connection_lock.unlock();
  mysql_options(&mysql_, MYSQL_OPT_NONBLOCK, 0);
}

MySQLConnection::~MySQLConnection() {
  mysql_connection_lock.lock();
  mysql_close(&mysql_);
  mysql_connection_lock.unlock();
}

arc::coro::Task<void> MySQLConnection::Connect(const std::string& host,
                                               std::uint16_t port,
                                               const std::string& user,
                                               const std::string& pwd,
                                               const std::string& db) {
  MYSQL* ret = nullptr;
  int status = 0;
  status = mysql_real_connect_start(
      &ret, &mysql_, host.c_str(), user.c_str(), pwd.c_str(),
      db.empty() ? nullptr : db.c_str(), port, nullptr, 0);
  while (status) {
    status = co_await WaitForMySQL(status);
    status = mysql_real_connect_cont(&ret, &mysql_, status);
  }

  if (!ret) {
    throw arc::exception::SQLException(&mysql_);
  }
}
arc::coro::Task<int> MySQLConnection::WaitForMySQL(int status) {
  int fd = mysql_get_socket(&mysql_);
  if (status & MYSQL_WAIT_READ) {
    co_await arc::coro::IOAwaiter(
        std::bind(&MySQLConnection::ReadyFunctor, this),
        std::bind(&MySQLConnection::ResumeFunctor, this), fd,
        arc::io::IOType::READ);
    co_await arc::coro::Yield();
    co_return MYSQL_WAIT_READ;
  } else if (status & MYSQL_WAIT_WRITE) {
    co_await arc::coro::IOAwaiter(
        std::bind(&MySQLConnection::ReadyFunctor, this),
        std::bind(&MySQLConnection::ResumeFunctor, this), fd,
        arc::io::IOType::WRITE);
    co_await arc::coro::Yield();
    co_return MYSQL_WAIT_WRITE;
  }
  co_return 0;
}

arc::coro::Task<void> MySQLConnection::Close() {
  int status = mysql_close_start(&mysql_);
  while (status) {
    status = co_await WaitForMySQL(status);
    status = mysql_close_cont(&mysql_, status);
  }
}

arc::coro::Task<MySQLResult> MySQLConnection::Run(const std::string& command) {
  int err = 0;
  int status =
      mysql_real_query_start(&err, &mysql_, command.c_str(), command.length());
  while (status) {
    status = co_await WaitForMySQL(status);
    status = mysql_real_query_cont(&err, &mysql_, status);
  }
  if (err) {
    throw arc::exception::SQLException(&mysql_);
  }
  co_return MySQLResult(this, mysql_use_result(&mysql_));
}

void MySQLConnection::CheckError() {
  if (mysql_errno(&mysql_)) {
    throw arc::exception::SQLException(&mysql_);
  }
}
