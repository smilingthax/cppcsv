#ifndef _SIMPLECSV_H
#define _SIMPLECSV_H

#include <map>
#include <vector>
#include "nocase.h"
#include "csvbase.h"

namespace SimpleCSV {

class Table;
class Row;
class Value {
public:
  int asInt() const;
  const char *asCString() const;
  const std::string &asString() const;

private:
  friend class Table;
  friend class Row;
  Value(const Row &parent,int cidx,const std::string &value);

  Value(const Value&); // = delete
  Value &operator=(const Value &);
private:
  // parent, cidx  are currently unused
  const Row &parent;
  int cidx;

  std::string value;
};

class Row {
  Row(const Row&); // = delete
  Row &operator=(const Row &);
public:
  ~Row();

  const Value &operator[](const char *key) const;
  const Value &operator[](int cidx) const;

  int size() const;

  void dump() const;
public:
  // special case: &value==&del
  void set(int cidx,const std::string &value);  // non-const !

  static const std::string del; // sentinel
private:
  friend class Table;
  Row(Table &parent,int ridx);
  void write(csv_builder &out) const;
private:
  Table &parent;
  int ridx;

  std::map<unsigned int,Value *> columns;  // TODO: not by *? (need move?)
};

class Table {
  Table(const Table&); // = delete
  Table &operator=(const Table &);
public:
  Table() {} // = default
  ~Table();

  const Row &operator[](int ridx) const;
  int size() const;

  void dump() const;
public:
  class IBuild {
  public:
    static Row &newRow(Table &csv);
    static Row &insertRow(Table &csv,int at_ridx); // 0<at_ridx<=size();   // replaces! (TODO? real insert?)
    static void deleteRow(Table &csv,int ridx);  // (moves elements!)

    static void setHeader(Table &csv,const std::vector<std::string>& names);
  };
  friend class IBuild;

  void write(csv_builder &out,bool with_header=false) const; // TODO header_if_not_empty?
private:
  friend class Row;
  int find_column(const std::string &name) const;

  static const Row empty_row;      // sentinel
  static const Value empty_value;  // sentinel
private:
  std::multimap<std::string,int,lt_nocase_str> rev_column;
  std::vector<const std::string *> columnnames;
  std::vector<Row *> rows;  // CPP11:  std::unique_ptr!   OR  by-value(Row moveable)?
};

class builder : public csv_builder {
public:
  builder(Table &result,bool first_is_header=false);
  void begin_row();// override;
  void cell(const char *buf,int len);// override;
  void end_row();// override;
private:
  Table &result;
  Row *row;
  int cidx;
  bool as_header;
  std::vector<std::string> header;
};

} // namespace SimpleCSV

#endif
