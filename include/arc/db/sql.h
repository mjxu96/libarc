/*
 * File: sql.h
 * Project: libarc
 * File Created: Tuesday, 13th September 2022 9:02:19 pm
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

#include <arc/coro/task.h>

namespace arc {
namespace db {

enum SQLType {
  TYPE_MYSQL = 0U,
};

class SQLResultRow {
 public:
  SQLResultRow() = default;
  virtual ~SQLResultRow() = default;
  virtual bool IsEmpty() = 0;
};

class SQLResult {
 public:
  SQLResult() = default;
  virtual ~SQLResult() = default;
  virtual coro::Task<void> FetchNextRow(
      std::shared_ptr<SQLResultRow> row_ptr) = 0;
  virtual int GetColumnCount() = 0;
};

template <typename SQLResultType>
class SQLConnection {
 public:
  SQLConnection(SQLType sql_type) : sql_type_(sql_type) {}
  virtual ~SQLConnection() = default;

  virtual coro::Task<void> Connect(const std::string& host, std::uint16_t port,
                                   const std::string& user,
                                   const std::string& pwd,
                                   const std::string& db) = 0;
  virtual coro::Task<SQLResultType> Run(const std::string& command) = 0;
  virtual coro::Task<void> Close() = 0;

 protected:
  SQLType sql_type_;
};

}  // namespace db
}  // namespace arc
