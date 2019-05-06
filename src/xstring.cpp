/* ***
 * @ $xstring.cpp
 * 
 * Copyright (C) 2018 Hsiang Chen
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * ***/
#include <stdarg.h>

#include "config.h"
#include "xstring.h"

using namespace std;

xstring::xstring() : string() {}
xstring::xstring(const xstring& s) : string(s) {}
xstring::xstring(size_t l, const char& ch) : string(l, ch) {}
xstring::xstring(const char* str) : string(str) {}
xstring::xstring(const char* str, size_t l) : string(str, l) {}
xstring::xstring(const xstring& s, size_t index, size_t l) : string(s, index, l) {}
xstring::xstring(iterator start, iterator end) : string(start, end) {}

int xstring::printf(const char* fmt, ...)
{
  va_list ap;
  char buf[BUFF_SIZE];
  int ret;

  va_start(ap, fmt);
  if ((ret = vsnprintf(buf, sizeof(buf), fmt, ap)) > 0) {
    append(buf);
  }
  va_end(ap);
  return ret;
}

int xstring::scanf(const char* fmt, ...)
{
  va_list ap;
  int ret;

  va_start(ap, fmt);
  ret = vsscanf(c_str(), fmt, ap);
  va_end(ap);
  return ret;
}

/*end*/
