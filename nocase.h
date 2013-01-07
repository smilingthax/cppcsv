#ifndef _NOCASE_H
#define _NOCASE_H

#include <locale>
#include <string>

struct lt_nocase_str : public std::binary_function<std::string, std::string, bool>
{
  struct lt_char {
    const std::ctype<char>& ct;
    lt_char(const std::ctype<char>& c) : ct(c) {}
    bool operator()(char x, char y) const {
      return ct.toupper(x) < ct.toupper(y);
    }
  };
  std::locale loc;
  const std::ctype<char>& ct;

  lt_nocase_str(const std::locale& L = std::locale::classic()) : loc(L), ct(std::use_facet<std::ctype<char> >(loc)) {}
  
  bool operator()(const std::string& x, const std::string& y) const {
    return std::lexicographical_compare(x.begin(), x.end(),
                                        y.begin(), y.end(),
                                        lt_char(ct));
  }
};

#endif
