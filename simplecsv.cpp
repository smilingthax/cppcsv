#include "simplecsv.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>

namespace SimpleCSV {

const std::string Row::del;

Table empty_table;
const Row Table::empty_row(empty_table,-1);
const Value Table::empty_value(empty_row,-1,std::string());

// {{{ Value
Value::Value(const Row &parent,int cidx,const std::string &value)
  : parent(parent),
    cidx(cidx),
    value(value)
{
}

int Value::asInt() const
{
  return atoi(value.c_str());  // TODO? error handling
}

const char *Value::asCString() const
{
  return value.c_str();
}

const std::string &Value::asString() const
{
  return value;
}
// }}}

// {{{ Row
Row::Row(Table &parent,int ridx)
  : parent(parent),ridx(ridx)
{
}

Row::~Row()
{
  for (std::map<unsigned int,Value *>::iterator it=columns.begin(),end=columns.end();it!=end;++it) {
    delete it->second;
  }
}

// TODO
const Value &Row::operator[](const char *key) const
{
  int cidx=parent.find_column(key);
  if (cidx==-1) { // TODO?! throw   (or return none; [static const Value none;] ?)
    printf("Key \"%s\" not found\n",key);
  }
  return operator[](cidx);
}

const Value &Row::operator[](int cidx) const // {{{
{
  if ( (cidx<0)||(cidx>=size()) ) {
    return parent.empty_value;
  }
  std::map<unsigned int,Value *>::const_iterator it=columns.find(cidx);
  if (it==columns.end()) {
    return parent.empty_value;
  }
  assert(it->second);
  return *it->second;
}
// }}}

int Row::size() const
{
  if (!columns.empty()) {  // columns.(r)begin()!=columns.(r)end()
    return columns.rbegin()->first+1; // last existing key +1
    //    "columns.back().first+1"
  }
  return 0;
//  return parent.columnnames.size();
}

void Row::set(int cidx,const std::string &value)
{
  if (&value==&del) {
    columns.erase(cidx);
  }
  Value *val=new Value(*this,cidx,value);
  columns.insert(std::make_pair(cidx,val));
}

void Row::dump() const // {{{
{
  printf("[%d]:",ridx);

//  for (std::map<unsigned int,Value *>::iterator it=columns.begin(),end=columns.end();it!=end;++it) {
  const int clen=size();
  for (int iA=0;iA<clen;iA++) {
    printf("%s;",operator[](iA).asCString());
  }
  printf("\n");
}
// }}}

void Row::write(csv_builder &out) const // {{{
{
  out.begin_row();
  const int clen=size();
  for (int iA=0;iA<clen;iA++) {
    const Value &val=operator[](iA);
    if (&val==&parent.empty_value) {
      out.cell(NULL,0);
    } else {
      const std::string &str=val.asString(); 
      out.cell(str.data(),str.size());
    }
  }
  out.end_row();
}
// }}}

// }}}

// {{{ Table
Table::~Table()
{
  const int len=rows.size();
  for (int iA=0;iA<len;iA++) {
    delete rows[iA];
  }
}

const Row &Table::operator[](int ridx) const
{
  if ( (ridx<0)||(ridx>=(int)size()) ) {
    return empty_row;
  }
  return *rows[ridx];
}

int Table::size() const
{
  return rows.size();
}

void Table::dump() const // {{{
{
  const int clen=size();
  for (int iA=0;iA<clen;iA++) {
    printf("%s;",columnnames[iA]->c_str());
  }
  printf("\n---\n");

  const int len=size();
  for (int iA=0;iA<len;iA++) {
    (operator[])(iA).dump();
//    rows[iA]->dump();
  }
}
// }}}

int Table::find_column(const std::string &name) const // {{{
{
  std::multimap<std::string,int>::const_iterator it=rev_column.find(name);
  if (it==rev_column.end()) {
    return -1;
  }
  return it->second; // only first matching element
}
// }}}

// TODO? allow rows[]==NULL  for empty_row  (created by after-the-end insertRow)

Row &Table::IBuild::newRow(Table &csv) // {{{
{
  csv.rows.push_back(new Row(csv,csv.rows.size()));
  return *csv.rows.back();
}
// }}}

Row &Table::IBuild::insertRow(Table &csv,int at_ridx) // {{{
{
  if (at_ridx<0) {
    throw std::invalid_argument("bad ridx");
  }
  if (at_ridx<(int)csv.rows.size()) { // insert
    Row *ret=new Row(csv,at_ridx);
/* replace
    delete csv.rows[at_ridx];
    csv.rows[at_ridx]=ret;
*/
    std::vector<Row *>::iterator it=csv.rows.begin();
    std::advance(it,at_ridx);
    try {
      it=++csv.rows.insert(it,ret);
    } catch (...) {
      delete ret;
      throw;
    }

    // fix Row::ridx 
    std::vector<Row *>::iterator end=csv.rows.end();
    for (;it!=end;++it) {
      (*it)->ridx++;
    }
    return *ret;
  } else { // append
    csv.rows.reserve(at_ridx+1);
    for (int iA=csv.rows.size();iA<=at_ridx;iA++) {
      csv.rows.push_back(new Row(csv,iA));
    }
    return *csv.rows.back();
  }
}
// }}}

void Table::IBuild::deleteRow(Table &csv,int ridx) // {{{
{
  if ( (ridx<0)||(ridx>=(int)csv.rows.size()) ) {
    return; // no-op   (TODO?)
  }
  delete csv.rows[ridx];
  std::vector<Row *>::iterator it=csv.rows.begin();
  std::advance(it,ridx);
  it=csv.rows.erase(it);

  // fix Row::ridx
  std::vector<Row *>::iterator end=csv.rows.end();
  for (;it!=end;++it) {
    (*it)->ridx--;
  }
}
// }}}

// TODO? throw instead at duplicate?
void Table::IBuild::setHeader(Table &csv,const std::vector<std::string>& names) // {{{
{
  csv.columnnames.clear();
  csv.rev_column.clear();

  const int len=names.size();
  csv.columnnames.reserve(len);
  for (int iA=0;iA<len;iA++) {
    std::multimap<std::string,int>::iterator it=csv.rev_column.insert(std::make_pair(names[iA],iA));
    csv.columnnames.push_back(&it->first);
  }
}
// }}}

void Table::write(csv_builder &out,bool with_header) const // {{{
{
  if (with_header) {
    out.begin_row();
    const int clen=size();
    for (int iA=0;iA<clen;iA++) {
      out.cell(columnnames[iA]->c_str(),
               columnnames[iA]->size());
    }
    out.end_row();
  }

  const int len=size();
  for (int iA=0;iA<len;iA++) {
    (operator[])(iA).write(out);
  }
}
// }}}

// }}}


builder::builder(Table &result,bool first_is_header) // {{{
  : result(result),
    row(NULL),
    cidx(-1),
    as_header(first_is_header)
{
}
// }}}

void builder::begin_row() // {{{
{
  if (as_header) {
    row=NULL;
    return;
  }
  row=&Table::IBuild::newRow(result);
  cidx=0;
}
// }}}

void builder::cell(const char *buf,int len) // {{{
{
  if (as_header) {
//    header.emplace_back(buf,len);
    header.push_back(std::string(buf,len));
    return;
  }
  assert(row);
  row->set(cidx++,std::string(buf,len));
}
// }}}

void builder::end_row() // {{{
{
  if (as_header) {
    Table::IBuild::setHeader(result,header);
    header.clear();
    as_header=false;
  }
}
// }}}

} // namespace SimpleCSV

