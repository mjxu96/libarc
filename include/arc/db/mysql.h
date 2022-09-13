/*
 * File: mysql.h
 * Project: libarc
 * File Created: Tuesday, 13th September 2022 9:15:50 pm
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

#include <mariadb/mysql.h>

#include "sql.h"

namespace arc {
namespace db {

class MySQLResult;
class MySQLConnection;

class MySQLResultRow {
 public:
  MySQLResultRow() = default;
  ~MySQLResultRow() = default;

  const bool IsEmpty() const { return !row_; }

  const std::string_view operator[](int i) const { return row_[i]; }

 private:
  friend class MySQLResult;
  ::MYSQL_ROW row_;
};

class MySQLResult {
 public:
  MySQLResult(MySQLConnection* conn, ::MYSQL_RES* res)
      : conn_(conn), res_(res) {}
  ~MySQLResult() {
    if (res_) {
      mysql_free_result(res_);
      res_ = nullptr;
    }
  }
  MySQLResult(const MySQLResult&) = delete;
  MySQLResult(MySQLResult&& other) {
    if (res_) {
      mysql_free_result(res_);
    }
    res_ = other.res_;
    conn_ = other.conn_;
    other.res_ = nullptr;
  }

  const MySQLResult& operator=(const MySQLResult&) = delete;
  MySQLResult& operator=(MySQLResult&& other) {
    if (res_) {
      mysql_free_result(res_);
    }
    res_ = other.res_;
    conn_ = other.conn_;
    other.res_ = nullptr;
    return *this;
  }

  const unsigned int GetColumnCount() const;

  coro::Task<MySQLResultRow> FetchNextRow();

 private:
  MySQLConnection* conn_;
  ::MYSQL_RES* res_{nullptr};
};

class MySQLConnection : public SQLConnection<MySQLResult> {
 public:
  MySQLConnection();
  ~MySQLConnection();

  virtual coro::Task<void> Connect(const std::string& host, std::uint16_t port,
                                   const std::string& user,
                                   const std::string& pwd,
                                   const std::string& db = "") override;
  virtual coro::Task<MySQLResult> Run(const std::string& command) override;
  virtual coro::Task<void> Close() override;
  void CheckError();

 private:
  friend class MySQLResult;
  bool ReadyFunctor() { return false; }
  bool ResumeFunctor() { return false; }
  coro::Task<int> WaitForMySQL(int status);
  ::MYSQL mysql_;
};

}  // namespace db
}  // namespace arc
