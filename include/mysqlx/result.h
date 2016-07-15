/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * The MySQL Connector/C++ is licensed under the terms of the GPLv2
 * <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>, like most
 * MySQL Connectors. There are special exceptions to the terms and
 * conditions of the GPLv2 as it is applied to this software, see the
 * FLOSS License Exception
 * <http://www.mysql.com/about/legal/licensing/foss-exception.html>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef MYSQLX_RESULT_H
#define MYSQLX_RESULT_H

/**
  @file
  Classes used to access query and command execution results.
*/


#include "common.h"
#include "document.h"
#include "collations.h"

#include <memory>


namespace cdk {

  class Reply;

}  // cdk


namespace mysqlx {

using std::ostream;

class NodeSession;
class Schema;
class Collection;
class Result;
class Row;
class RowResult;
class SqlResult;
class DbDoc;
class DocResult;

template <class Res> class Executable;


/*
  Iterator class for RowResult and DocResult
*/

namespace internal {

  class BaseResult;

/*
  List_initializer object can be used to initialize a container of
  arbitrary type U with list of items taken from source object.

  It is assumed that the source object type Source defines iterator
  type and that std::begin/end() return iterators to the beginning
  and end of the sequence. The container type U is assumed to have
  a constructor from begin/end iterator.

  List_iterator defines begin/end() methods, so it is possible to
  iterate over the sequence without storing it in any container.
*/

template <typename Source>
class List_initializer
{
protected:

  Source &m_src;

  List_initializer(Source &src)
    : m_src(src)
  {}


public:

  typedef typename Source::iterator iterator;

  template <typename U>
  operator U()
  {
    return U(std::begin(m_src), std::end(m_src));
  }

  iterator begin()
  {
    return std::begin(m_src);
  }

  iterator end() const
  {
    return std::end(m_src);
  }

  friend RowResult;
  friend DocResult;
  friend BaseResult;
};


/*
  Iterator template.

  It defines an STL input iterator which is implemented using an
  implementation object of some type Impl. It is assumed that Impl
  has the following methods:

   void iterator_start() - puts iterator in "before begin" positon;
   bool iterator_next() - moves iterator to next position, returns
                          false if it was not possible;
   Value_type iterator_get() - gets current value.
*/

template<typename Value_type, typename Impl>
struct iterator
  : std::iterator < std::input_iterator_tag, Value_type>
{
  Impl *m_impl = NULL;
  bool m_at_end = false;

  iterator(Impl& impl)
    : m_impl(&impl)
  {
    m_impl->iterator_start();
    m_at_end = !m_impl->iterator_next();
  }

  iterator()
    : m_at_end(true)
  {}

  bool operator !=(const iterator &other) const
  {
    /*
      Compares only if both iterators are at the end
      of the sequence.
    */
    return !(m_at_end && other.m_at_end);
  }

  iterator<Value_type, Impl>& operator++()
  {
    if (m_impl && !m_at_end)
      m_at_end = !m_impl->iterator_next();
    return *this;
  }

  Value_type operator*() const
  {
    if (!m_impl || m_at_end)
      THROW("Attempt to dereference null iterator");
    return m_impl->iterator_get();
  }

};


} // internal


/*
  @todo Add diagnostics information (warnings)
*/

namespace internal {

  class BaseResult;

} // internal


class Warning : public internal::Printable
{
public:

  enum Level { ERROR, WARNING, INFO };

private:

  Level    m_level;
  uint16_t m_code;
  string   m_msg;

  Warning(Level level, uint16_t code, const string &msg)
    : m_level(level), m_code(code), m_msg(msg)
  {}

  void print(std::ostream&) const;

public:

  Level getLevel() const
  {
    return m_level;
  }

  uint16_t getCode() const
  {
    return m_code;
  }

  const string& getMessage() const
  {
    return m_msg;
  }

  struct Access;
  friend Access;
};

inline
void Warning::print(std::ostream &out) const
{
  switch (getLevel())
  {
  case ERROR: out << "Error"; break;
  case WARNING: out << "Warning"; break;
  case INFO: out << "Info"; break;
  }

  if (getCode())
    out << " " << getCode();

  out << ": " << getMessage();
}


namespace internal {

  class BaseResult : nocopy
  {
    class Impl;
    Impl  *m_impl = NULL;
    bool m_owns_impl = false;
    row_count_t  m_pos = 0;

    BaseResult(cdk::Reply*);
    BaseResult(cdk::Reply*, const std::vector<GUID>&);

  protected:

    BaseResult()
    {}

    BaseResult& operator=(BaseResult &&other)
    {
      init(std::move(other));
      return *this;
    }

    void init(BaseResult&&);

    const Impl& get_impl() const;
    Impl& get_impl()
    {
      return const_cast<Impl&>(
        const_cast<const BaseResult*>(this)->get_impl()
      );
    }

  public:

    typedef internal::iterator<Warning, BaseResult> iterator;

    BaseResult(BaseResult &&other) { init(std::move(other)); }
    virtual ~BaseResult();


    unsigned getWarningCount() const;
    internal::List_initializer<BaseResult> getWarnings()
    {
      return List_initializer<BaseResult>(*this);
    };

    Warning getWarning(unsigned);

    iterator begin()
    {
      return iterator(*this);
    }

    iterator end()
    {
      return iterator();
    }

  private:

    // warning iterator implementation

    unsigned m_wpos;
    bool   m_at_begin;

    void iterator_start()
    {
      m_wpos = 0;
      m_at_begin = true;
    }

    bool iterator_next()
    {
      if (!m_at_begin)
        m_wpos++;
      m_at_begin = false;
      return m_wpos < getWarningCount();
    }

    Warning iterator_get()
    {
      return getWarning(m_wpos);
    }

  public:

    friend mysqlx::NodeSession;
    friend mysqlx::Result;
    friend mysqlx::RowResult;
    friend mysqlx::SqlResult;
    friend mysqlx::DocResult;
    friend iterator;

    struct Access;
    friend Access;
  };

  inline
  void BaseResult::init(BaseResult &&init_)
  {
    m_pos = 0;
    m_impl = init_.m_impl;
    if (!init_.m_owns_impl)
      m_owns_impl = false;
    else
    {
      m_owns_impl = true;
      init_.m_owns_impl = false;
    }
  }

}

/**
  Represents result of an operation that does not return data.

  Generic result which can be returned by operations which only
  modify data.

  `Result` instance can store result of executing an operation:

  ~~~~~~
  Result res = operation.execute();
  ~~~~~~

  Storing another result in `Result` instance will overwrite
  previous result.

  @todo Implement other methods for getting information about
  the result specified by DevAPI.
*/

class Result : public internal::BaseResult
{
public:

  Result() = default;

  Result(Result &&other)
  {
    *this = std::move(other);
  }

  Result& operator=(Result &&other)
  {
    init(std::move(other));
    return *this;
  }

  /**
    Get the count of affected items from manipulation statements.
  */

  uint64_t getAffectedItemsCount() const;

  /**
    Get the auto-increment value if one was generated by a table insert
    statement.
  */

  uint64_t getAutoIncrementValue() const;

  /**
    Return id of the document added to a collection.
  */

  const GUID& getDocumentId() const;

  /**
    Return list of ids of documents added to a collection on a chain add() call.
  */

  internal::List_init<GUID> getDocumentIds() const;

private:

  Result(BaseResult &&other)
  {
    init(std::move(other));
  }

  friend Executable<Result>;
};



// Row based results
// -----------------


// RowResult column meta-data
// --------------------------

/**
List of types defined by DevAPI. For each type TTT in this list
there is corresponding enumeration value Type::TTT.

Note: The class name declared for each type is ignored for now
- it is meant for future extensions.
*/

#define TYPE_LIST(X) \
  X(BIT,        BlobType)     \
  X(TINYINT,    IntegerType)  \
  X(SMALLINT,   IntegerType)  \
  X(MEDIUMINT,  IntegerType)  \
  X(INT,        IntegerType)  \
  X(BIGINT,     IntegerType)  \
  X(FLOAT,      NumericType)  \
  X(DECIMAL,    NumericType)  \
  X(DOUBLE,     NumericType)  \
  X(JSON,       Type)         \
  X(STRING,     StringType)   \
  X(BYTES,      BlobType)     \
  X(TIME,       Type)         \
  X(DATE,       Type)         \
  X(DATETIME,   Type)         \
  X(TIMESTAMP,  Type)         \
  X(SET,        StringType)   \
  X(ENUM,       StringType)   \
  X(GEOMETRY,   Type)         \

#undef TYPE_ENUM
#define TYPE_ENUM(T,X) T,

enum class Type : unsigned short
{
  TYPE_LIST(TYPE_ENUM)
};


#define TYPE_NAME(T,X) case Type::T: return #T;

inline
const char* typeName(Type t)
{
  switch (t)
  {
    TYPE_LIST(TYPE_NAME)
  default:
    THROW("Unkonwn type");
  }
}

inline
std::ostream& operator<<(std::ostream &out, Type t)
{
  return out << typeName(t);
}


/**
Class providing meta-data for a single result column.
*/

class Column : public internal::Printable
{
public:

  string getSchemaName()  const;
  string getTableName()   const;
  string getTableLabel()  const;
  string getColumnName()  const;
  string getColumnLabel() const;

  Type getType()   const;

  unsigned long getLength() const;
  unsigned short getFractionalDigits() const;
  bool isNumberSigned() const;

  CharacterSet getCharacterSet() const;
  std::string getCharacterSetName() const
  {
    return characterSetName(getCharacterSet());
  }

  const CollationInfo& getCollation() const;
  std::string getCollationName() const
  {
    return getCollation().getName();
  }

  bool isPadded() const;

private:

  class Impl;
  std::shared_ptr<Impl> m_impl;
  virtual void print(std::ostream&) const;

public:

  friend Impl;

  struct Access;
  friend Access;
};


class RowResult;

/**
  Represents a single row from a result that contains rows.

  Such a row consists of a number of fields, each storing single
  value. The number of fields and types of values stored in each
  field are described by `RowResult` instance that produced this
  row.

  Values of fields can be accessed with `get()` method or using
  `row[pos]` expression. Fields are identified by 0-based position.
  It is also possible to get raw bytes representing value of a
  given field with `getBytes()` method.

  @sa `Value` class.
  @todo Support for iterating over row fields with range-for loop.
*/

class Row
{
  class Impl;
  std::shared_ptr<Impl>  m_impl;

  Impl& get_impl()
  { return const_cast<Impl&>(const_cast<const Row*>(this)->get_impl()); }
  const Impl& get_impl() const;

  Row(std::shared_ptr<Impl> &&impl) : m_impl(std::move(impl))
  {}

  void set_values(col_count_t pos, const Value &val)
  {
    set(pos, val);
  }

  template<typename... Types>
  void set_values(col_count_t pos, const Value &val, Types... rest)
  {
    set(pos, val);
    set_values(pos + 1, rest...);
  }

public:

  Row() {}

  template<typename... Types>
  Row(const Value &val, Types... vals)
  {
    set_values(0, val, vals...);
  }

  virtual ~Row() {}

  col_count_t colCount() const;


  /**
    Get raw bytes representing value of row field at position `pos`.

    @returns null bytes range if given field is NULL.
    @throws out_of_range if given row was not fetched from server.
  */

  bytes getBytes(col_count_t pos) const;


  /**
    Get reference to row field at position `pos`.

    @throws out_of_range if given field does not exist in the row.
  */

  Value& get(col_count_t pos);


  /**
    Set value of row field at position `pos`.

    Creates new field if it does not exist.

    @returns Reference to the field that was set.
  */

  Value& set(col_count_t pos, const Value&);

  /**
    Get const reference to row field at position `pos`.

    This is const version of method `get()`.

    @throws out_of_range if given field does not exist in the row.
  */

  const Value& operator[](col_count_t pos) const
  {
    return const_cast<Row*>(this)->get(pos);
  }


  /**
    Get modifiable reference to row field at position `pos`.

    The field is created if it does not exist. In this case
    the initial value of the field is NULL.
  */

  Value& operator[](col_count_t pos)
  {
    try {
      return get(pos);
    }
    catch (const out_of_range&)
    {
      return set(pos, Value());
    }
  }

  /// Check if this row contains fields or is null.
  bool isNull() const { return NULL == m_impl; }
  operator bool() const { return !isNull(); }

  void clear();

  friend RowResult;
};


/**
  %Result of an operation that returns rows.
*/

class RowResult : public internal::BaseResult
{
  // Column meta-data access

  struct Columns_src
  {
    const RowResult &m_res;

    Columns_src(const RowResult &res)
      : m_res(res)
    {}

    typedef internal::iterator<Column, Columns_src> iterator;

    iterator begin()
    {
      return iterator(*this);
    }

    iterator end()
    {
      return iterator();
    }

    // iterator implementation

    col_count_t m_pos;
    bool m_at_begin;

    void iterator_start()
    {
      m_pos = 0;
      m_at_begin = true;
    }

    bool iterator_next()
    {
      if (!m_at_begin)
        m_pos++;
      m_at_begin = false;
      return m_pos < m_res.getColumnCount();
    }

    Column iterator_get()
    {
      return m_res.getColumn(m_pos);
    }
  };

  struct Columns
    : public internal::List_initializer<Columns_src>
  {
    Columns_src m_src;

    Columns(const RowResult &res)
      : List_initializer(m_src)
      , m_src(res)
    {}

    /*
      Note: Without this empty destructor code crashes on
      Solaris but works fine on all other platforms. The
      crash is like if the m_src object gets destroyed too
      early.
    */

    ~Columns()
    {}

    Column operator[](col_count_t pos) const
    {
      return m_src.m_res.getColumn(pos);
    }
  };


  std::forward_list<Row> m_row_cache;
  uint64_t m_row_cache_size = 0;
  bool m_cache = false;


public:

  typedef internal::iterator<Row, RowResult> iterator;

  RowResult()
  {}

  virtual ~RowResult() {}

  /*
    Note: Even though we have RowResult(BaseResult&&) constructor below,
    we still need move-ctor for such copy-initialization to work:

      RowResult res= coll...execute();

    This copy-initialization works as follows
    (see http://en.cppreference.com/w/cpp/language/copy_initialization):

    1. A temporary prvalue of type RowResult is created by type-conversion
       of the Result prvalue coll...execute(). Constructor RowResult(Result&&)
       is calld to do the conversion.

    2. Now res is direct-initialized
       (http://en.cppreference.com/w/cpp/language/direct_initialization)
       from the prvalue produced in step 1.

    Since RowResult has disabled copy constructor, a move constructor is
    required for direct-initialization in step 2. Even though move-constructor
    is actually not called (because of copy-elision), it must be declared
    in the RowResult class. We also define it for the case that copy-elision
    was not applied.
  */

  RowResult(RowResult &&other)
    : BaseResult(std::move(static_cast<BaseResult&>(other)))
  {}

  RowResult& operator=(RowResult &&init_)
  {
    BaseResult::operator=(std::move(init_));
    return *this;
  }

  /// Retrun number of fields in each row.

  col_count_t getColumnCount() const;

  /// Return Column instance describing given result column.

  const Column& getColumn(col_count_t pos) const;

  /**
    Return meta-data for all result columns. The returned data
    can be stored in any STL container which can store Column
    objects.
  */

  Columns getColumns() const
  {
    try {
      return Columns(*this);
    }
    CATCH_AND_WRAP
  }

  /**
    Return current row and move to the next one in the sequence.

    If there are no more rows in this result, returns NULL.
  */

  Row fetchOne();

  /**
    Return all remaining rows

    Rows that have been fetched using fetchOne() will not be available when
    calling fetchAll()
   */


public:

  internal::List_initializer<RowResult> fetchAll()
  {
    return internal::List_initializer<RowResult>(*this);
  }

  /**
     Returns number of rows available on RowResult to be fetched
   */

  uint64_t count();


  /**
   Iterate over Rows.

   Rows that have been fetched using iterator will not be available when
   calling fetchOne() or fetchAll()
  */

  iterator begin()
  {
    return iterator(*this);
  }

  iterator end() const
  {
    return iterator();
  }

protected:

  void check_result() const;

private:

  RowResult(BaseResult &&init_)
    : BaseResult(std::move(init_))
  {}

  // iterator implementation

  Row m_cur_row;

  void iterator_start() {}

  bool iterator_next()
  {
    m_cur_row = fetchOne();
    return !m_cur_row.isNull();
  }

  Row iterator_get()
  {
    return m_cur_row;
  }

  friend Executable<RowResult>;
  friend SqlResult;
  friend DocResult;
  friend iterator;
};


/**
  %Result of SQL query or command.

  Such result can contain one or more results returned from a
  single command or query. When created, SqlResult instance gives
  access to the first result. Method `nextResult()` moves to the next
  result if there is any.

  @todo implement `nextResult()` and other methods specified by DevAPI.
*/

class SqlResult : public RowResult
{
public:

  SqlResult(SqlResult &&other)
    : RowResult(std::move(static_cast<RowResult&>(other)))
  {}


  SqlResult& operator=(SqlResult &&init_)
  {
    RowResult::operator=(std::move(init_));
    return *this;
  }


  /**
    Tels if current result contains rows.

    If this is the case, rows can be accessed using `RowResult` interface.
    Otherwise calling `RowResult` methods throws an error.
  */

  bool hasData() const;


  /**
    Move to next result if there is one.

    Returns true if next result is available, false if there are no more
    results in the reply. Calling `nextResult()` discards the current result
    and all the data it contains (if any).
  */

  bool nextResult();

private:

  SqlResult(BaseResult &&init_)
    : RowResult(std::move(init_))
  {}

  friend Executable<SqlResult>;
};


// Document based results
// ----------------------


/**
  %Result of an operation that returns documents.
*/

class DocResult // : public internal::BaseResult
{
  class Impl;
  Impl *m_doc_impl = NULL;

  void check_result() const;

public:

  typedef internal::iterator<DbDoc, DocResult> iterator;

  DocResult()
  {}

  DocResult(DocResult &&other)
  {
    *this = std::move(other);
  }

  virtual ~DocResult();

  void operator=(DocResult &&other);

  /**
    Return current document and move to the next one in the sequence.

    If there are no more documents in this result returns null document.
  */

  DbDoc fetchOne();

  /**
    Return all remaining documents.

    Documents that have been fetched using fetchOne() will not be available when
    calling fetchAll()
   */

public:

  internal::List_initializer<DocResult> fetchAll()
  {
    return internal::List_initializer<DocResult>(*this);
  }

  /**
     Returns number of documents available on DocResult to be fetched.
   */

  uint64_t count();

  /**
   Iterate over Documents.

   Documents that have been fetched using iterator will not be available when
   calling fetchOne() or fetchAll()
  */

  iterator begin()
  {
    return iterator(*this);
  }

  iterator end() const
  {
    return iterator();
  }


private:

  DocResult(internal::BaseResult&&);

  // iterator implementation

  DbDoc m_cur_doc;

  void iterator_start() {}

  bool iterator_next()
  {
    m_cur_doc = fetchOne();
    return !m_cur_doc.isNull();
  }

  DbDoc iterator_get()
  {
    return m_cur_doc;
  }

  friend Impl;
  friend DbDoc;
  friend Executable<DocResult>;
  friend iterator;
};


}  // mysqlx

#endif
