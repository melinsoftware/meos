#pragma once

#include <string>
#include <map>
#include <vector>
#include "mysql/mysql.h"

using std::string;
using std::map;

namespace sqlwrapper {

  class ConnectionWrapper;
  class QueryWrapper;
  class ResultWrapper;
  class ResultBase;
  
  class CellWrapper {
    char *data;
    unsigned int length;
    CellWrapper(char *data, unsigned int length) : data(data), length(length) {}
    friend class RowWrapper;
  public:
    const char *c_str() const;
    string get_string() const;
    operator string() const;
    operator int() const;
    operator unsigned int() const;
    operator bool() const;
    int64_t longlong() const;
    uint64_t ulonglong() const;
    void storeBlob(std::vector<uint8_t>& d) const;
    bool is_null() const;
  };

  class RowWrapper {
    MYSQL_ROW row;
    ResultBase *res;
    RowWrapper(ResultBase *res, MYSQL_ROW row);
    friend class ResultWrapper;
    friend class ResUseWrapper;
  public:
    RowWrapper() : row(nullptr), res(nullptr) {}
    CellWrapper at(int col) const;
    int size() const;
    operator bool() const;
    CellWrapper operator[](const char *col) const;
    CellWrapper operator[](int ix) const;
    const char *raw_string(int ix) const;
  };

  class ResultBase {
  protected:
    ConnectionWrapper *con;
    MYSQL_RES *result;
    map<string, int> name2Col;
    ResultBase(ConnectionWrapper *con, MYSQL_RES *result);
    friend class QueryWrapper;
    friend class RowWrapper;
  public:
    ResultBase(ResultBase &&r);
    const ResultBase &operator=(ResultBase &&);
    ResultBase(const ResultBase& r) = delete;
    const ResultBase& operator=(const ResultBase&) = delete;

    virtual ~ResultBase();
    int field_num(const string &field);
  };


  class ResultWrapper : public ResultBase {
    ResultWrapper(ConnectionWrapper &con, MYSQL_RES *result);
    friend class QueryWrapper;
    friend class RowWrapper;
  public:
    ResultWrapper() : ResultBase(nullptr, nullptr) {}
    ResultWrapper(ResultWrapper &&r) : ResultBase(std::move(r)) {}
    ResultWrapper(const ResultWrapper& r) = delete;
    const ResultBase &operator=(ResultWrapper &&r) {
      ResultBase::operator=(std::move(r)); 
      return *this;
    }
    const ResultBase& operator=(const ResultBase& r) = delete;
    
    //ResultWrapper(ResultWrapper &r);
    //~ResultWrapper();
    //const ResultWrapper &operator=(ResultWrapper &);

    int num_rows() const;
    RowWrapper at(int row);
    operator bool() const;
    bool empty() const;    
  };

  class ResUseWrapper : public ResultBase {
    ResUseWrapper(ConnectionWrapper& con, MYSQL_RES* result);
    friend class QueryWrapper;
    friend class RowWrapper;
  public:
    ResUseWrapper(ResUseWrapper&& r) : ResultBase(std::move(r)) {}
    const ResUseWrapper& operator=(ResUseWrapper&& r) {
      ResultBase::operator=(std::move(r));
      return *this;
    }
    ResUseWrapper(const ResUseWrapper& r) = delete;
    const ResUseWrapper& operator=(const ResUseWrapper& r) = delete;

    operator bool() const;
    RowWrapper fetch_row();
  };

  class ResNSel {
  public:
    ResNSel(int insert_id, int rows) : insert_id(insert_id), rows(rows) {}
    const int insert_id;
    const int rows;
    operator bool() const;
  };

 
  class Exception {
    string w;
  public:
    Exception(const char *w);
    const char *what() const;
  };

  class BadFieldName : public Exception {
    string fieldName;
  public:
    BadFieldName(const string &n) : Exception("Bad field name"), fieldName(n) {}
    const string &name() const { return fieldName; }
  };

  class EndOfResults : public Exception {
  public:
    EndOfResults() : Exception("End of results") {}
  };

  enum QT {
    quote
  };

  class QueryWrapper {
    ConnectionWrapper &con;
    string sql;
    bool qnext = false;
    QueryWrapper(ConnectionWrapper &con);
  public:

    ~QueryWrapper();
    ResultWrapper store(const string &q);
    ResultWrapper store();

    ResUseWrapper use(const string &q);

    void reset();
    ResNSel execute();
    ResNSel execute(const string &sql);
    void exec(const string &sql);

    const string& str() const;
    QueryWrapper & operator<<(const string &arg);
    QueryWrapper & operator<<(const char *arg);
    QueryWrapper & operator<<(int arg);
    QueryWrapper & operator<<(uint32_t arg);
    QueryWrapper & operator<<(int64_t arg);
    QueryWrapper & operator<<(QT arg);

    friend class ConnectionWrapper;
  };

  class ConnectionWrapper {
  private:
    MYSQL *mysql = nullptr;
  protected:
    MYSQL *get() const;

    friend class QueryWrapper;
    bool unusedResult = false;
  public:
    ConnectionWrapper();
    virtual ~ConnectionWrapper();
    void close();
    bool select_db(const string &db);
    void create_db(const string &db);
    void drop_db(const string &db);
    string server_info() const;
    bool connected() const;
    void connect(const string &unused, const string &server, const string &user,
                 const string &pwd, int port);
    QueryWrapper query();
  };
  
}
