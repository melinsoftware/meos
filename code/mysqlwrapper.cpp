/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2025 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License fro more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include "stdafx.h"

#include "mysql/mysql.h"
#include "mysqlwrapper.h"

using namespace std;
using namespace sqlwrapper;

const char *CellWrapper::c_str() const {
  return data;
}

string CellWrapper::get_string() const {
  if (data != nullptr)
    return data;
  return "";
}

CellWrapper::operator string() const {
  if (data != nullptr)
    return data;
  return "";
}
CellWrapper::operator int() const {
  if (data != nullptr)
    return atoi(data);
  return 0;
}
CellWrapper::operator unsigned int() const {
  char *out;
  if (data != nullptr)
    return strtoul(data, &out, 10);
  return 0;
} 

CellWrapper::operator bool() const {
  return int(*this) != 0;
}

int64_t CellWrapper::longlong() const {
  char *out;
  if (data != nullptr)
    return strtoll(data, &out, 10);
  return 0;
}

uint64_t CellWrapper::ulonglong() const {
  char* out;
  if (data != nullptr)
    return strtoull(data, &out, 10);
  return 0;
}

void CellWrapper::storeBlob(std::vector<uint8_t>& d) const {
  if (data != nullptr) {
    d.resize(length);
    memcpy(d.data(), data, length);
  }
}

bool CellWrapper::is_null() const {
  return data == nullptr;
}

CellWrapper RowWrapper::at(int col) const {
  if (res && row) {
    auto lengths = mysql_fetch_lengths(res->result);
    return CellWrapper(row[col], lengths[col]);
  }
  throw Exception("Invalid offset");
}

CellWrapper RowWrapper::operator[](const char *name) const {
  if (res) {
    int col = res->field_num(name);
    return at(col);
  }
  throw Exception("Invalid offset");
}

CellWrapper RowWrapper::operator[](int ix) const {
  if (res) {
    return at(ix);
  }
  throw Exception("Invalid offset");
}

RowWrapper::RowWrapper(ResultBase *res, MYSQL_ROW row) : res(res), row(row) {

}

int RowWrapper::size() const {
  if (res && res->result)
    return mysql_num_fields(res->result);
  return 0;
}

RowWrapper::operator bool() const {
  return row != nullptr;
}

const char *RowWrapper::raw_string(int ix) const {
  if (row)
    return row[ix];
  return nullptr;
}

ResultBase::ResultBase(ConnectionWrapper *con, MYSQL_RES * res) : con(con), result(res) {
}

ResultBase::ResultBase(ResultBase &&r) : con(r.con) {
  result = r.result;
  r.result = nullptr;
}

const ResultBase &ResultBase::operator=(ResultBase &&r) {
  con = r.con;
  if (result)
    mysql_free_result(result);
  name2Col.clear();
  result = r.result;
  r.result = nullptr;
  return *this;
}

ResultBase:: ~ResultBase() {
  if (result) {
    mysql_free_result(result);
    result = nullptr;
  }
}

int ResultBase::field_num(const string &fieldName) {
  if (name2Col.empty()) {
    //if (mysql_result_metadata()
    unsigned int num_fields;
    unsigned int i;
    MYSQL_FIELD *fields;
    num_fields = mysql_num_fields(result);
    fields = mysql_fetch_fields(result);
    for (i = 0; i < num_fields; i++)
      name2Col[fields[i].name] = i;
  }

  auto res = name2Col.find(fieldName);
  if (res == name2Col.end())
    throw BadFieldName(fieldName);

  return res->second;
}

ResultWrapper::ResultWrapper(ConnectionWrapper &con, MYSQL_RES * res) : ResultBase(&con, res) {
}

int ResultWrapper::num_rows() const {
  if (result)
    return (int)mysql_num_rows(result);
  return 0;
}

RowWrapper ResultWrapper::at(int row) {
  if (result && row>=0 && row < num_rows()) {
    mysql_data_seek(result, row);
    auto rd = mysql_fetch_row(result);
    if (rd) {
      return RowWrapper(this, rd);
    }
  }
  throw Exception("Invalid offset");
}

ResultWrapper::operator bool() const {
  return result != nullptr;
}

bool ResultWrapper::empty() const {
  return num_rows() == 0;
}

ResUseWrapper::ResUseWrapper(ConnectionWrapper &con, MYSQL_RES *res) : ResultBase(&con, res) {
}

ResUseWrapper::operator bool() const {
  return result != nullptr;
}

RowWrapper ResUseWrapper::fetch_row() {
  if (!result)
    throw Exception("Invalid result");
  auto row = mysql_fetch_row(result);
  if (row)
    return RowWrapper(this, row);
  else
    throw EndOfResults();
}

ResNSel::operator bool() const{
  return rows>=0;
};

Exception::Exception(const char *w) : w(w) {}

const char *Exception::what() const {
  return w.c_str();
}

QueryWrapper::QueryWrapper(ConnectionWrapper &con) : con(con) {
  sql.reserve(1024*4);
}
  
QueryWrapper::~QueryWrapper() {

}

ResultWrapper QueryWrapper::store(const string &q) {
  exec(q);
  auto result = mysql_store_result(con.get());
  con.unusedResult = false;
  if (result || mysql_field_count(con.get()) == 0) {
    return ResultWrapper(con, result);   
  }
  throw Exception(mysql_error(con.get()));
}

ResultWrapper QueryWrapper::store() {
  return store(sql);
}

ResUseWrapper QueryWrapper::use(const string &q) {
  exec(q);
  auto result = mysql_use_result(con.get());
  con.unusedResult = false;
  if (result || mysql_field_count(con.get()) == 0) {
    return ResUseWrapper(con, result);
  }
  throw Exception(mysql_error(con.get()));
}

void QueryWrapper::reset() {
  sql.clear();
}

ResNSel QueryWrapper::execute() {
  exec(sql);
  int id = (int) mysql_insert_id(con.get());
  int r = (int) mysql_affected_rows(con.get());
  return ResNSel(id, r);
}

ResNSel QueryWrapper::execute(const string &q) {
  exec(q);
  int id = (int)mysql_insert_id(con.get());
  int r = (int)mysql_affected_rows(con.get());
  return ResNSel(id, r);
}

void QueryWrapper::exec(const string &q) {
  auto c = con.get();
  if (con.unusedResult) {
    auto res = mysql_store_result(c);
    if (res)
      mysql_free_result(res);
  }
  if (mysql_real_query(c, q.c_str(), q.length()) != 0)
    throw Exception(mysql_error(c));

  con.unusedResult = true;

}

const string &QueryWrapper::str() const {
  return sql;
}

QueryWrapper & QueryWrapper::operator<<(const string &arg) {
  if (qnext) {
    int len = arg.size();
    int pt = sql.size();
    sql.resize(pt + len * 2);
    int realsize = mysql_real_escape_string(con.get(), &sql[pt], arg.c_str(), len);
    sql.resize(pt + realsize);
    sql += "'";
    qnext = false;
  }
  else {
    sql += arg;
  }
  return *this;
}
QueryWrapper & QueryWrapper::operator<<(const char *arg) {
  if (qnext) {
    int len = strlen(arg);
    int pt = sql.size();
    sql.resize(pt + len*2);
    int realsize = mysql_real_escape_string(con.get(), &sql[pt], arg, len);
    sql.resize(pt + realsize);
    sql += "'";
    qnext = false;
  }
  else {
    sql += arg;
  }
  return *this;
}
QueryWrapper & QueryWrapper::operator<<(int arg) {
  sql += to_string(arg);
  if (qnext) {
    sql += "'";
    qnext = false;
  }
  return *this;
}
QueryWrapper & QueryWrapper::operator<<(uint32_t arg) {
  sql += to_string(arg);
  if (qnext) {
    sql += "'";
    qnext = false;
  }
  return *this;
}
QueryWrapper & QueryWrapper::operator<<(int64_t arg) {
  sql += to_string(arg);
  if (qnext) {
    sql += "'";
    qnext = false;
  }
  return *this;
}
QueryWrapper & QueryWrapper::operator<<(QT arg) {
  sql += "'";
  qnext = true;
  return *this;
}

ConnectionWrapper::ConnectionWrapper() {  
}

ConnectionWrapper::~ConnectionWrapper() {
  close();
}

void ConnectionWrapper::close() {
  if (mysql != nullptr) {
    mysql_close(mysql);
    mysql = nullptr;
  }    
}

bool ConnectionWrapper::select_db(const string &db) {
  if (mysql_select_db(get(), db.c_str()) != 0)
    throw Exception(mysql_error(mysql));

  return true;
}

void ConnectionWrapper::create_db(const string &db) {
  string sql = "CREATE DATABASE " + db + "";
  if (mysql_query(get(), sql.c_str()) != 0)
    throw Exception(mysql_error(mysql));  
}

void ConnectionWrapper::drop_db(const string &db) {
  string sql = "DROP DATABASE " + db + "";
  if (mysql_query(get(), sql.c_str()) != 0)
    throw Exception(mysql_error(mysql));
}

MYSQL *ConnectionWrapper::get() const {
  if (!mysql)
    throw Exception("Not connected");
  return mysql;
}

string ConnectionWrapper::server_info() const {  
  const char *ptr =  mysql_get_server_info(get());
  if (ptr)
    return ptr;
  return "";
}

bool ConnectionWrapper::connected() const {
  return mysql != nullptr;
}

void ConnectionWrapper::connect(const string &unused,
                                const string &server,
                                const string &user,
                                const string &pwd,
                                int port) {
  close();
  mysql = mysql_init(nullptr);
  if (!mysql_real_connect(mysql,
      server.c_str(),
      user.c_str(),
      pwd.c_str(),
      nullptr,
      port,
      nullptr,
      0)) {
    string err = mysql_error(mysql);
    mysql_close(mysql);
    mysql = nullptr;
    throw Exception(err.c_str());
  }

  bool t = true;
  mysql_options(mysql, MYSQL_OPT_RECONNECT, &t);
}

QueryWrapper ConnectionWrapper::query() {
  return QueryWrapper(*this);
}
