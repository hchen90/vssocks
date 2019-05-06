/* $ @xstring.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_XSTRING_H_
#define	_XSTRING_H_

#include <string>

namespace std {
  class xstring : public string {
  public:
    xstring();
    xstring(const xstring& s);
    xstring(size_t l, const char& ch);
    xstring(const char* str);
    xstring(const char* str, size_t l);
    xstring(const xstring& s, size_t index, size_t l);
    xstring(iterator start, iterator end);

    int printf(const char* fmt, ...);
    int scanf(const char* fmt, ...);
  };
};

#endif	/* _XSTRING_H_ */
