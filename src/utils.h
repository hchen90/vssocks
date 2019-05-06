/* $ @utils.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * This file is autogenated by perl script. DO NOT edit it.
 * */
#ifndef	_UTILS_H_
#define	_UTILS_H_

#include <vector>
#include <string>
#include <ostream>

#include "buffer.h"

#define MAX(x00, x01) (x00 > x01 ? x00 : x01)
#define MIN(x00, x01) (x00 < x01 ? x00 : x01)

using namespace std;

namespace utils {
  char*   skpstr(char* buf, int ch, size_t len, int where = 0);
  char*   srpstr(char* buf, int ch, size_t* len, int where = 3);
  size_t  tokstr(const char* buf, size_t len, vector<string>& tt);
  char*   genramstr(char* buf, size_t len);
  size_t  atocap(const char* buf, size_t len);
  bool    gethostnameport(const string& host, string& hostip, int& port);
};

namespace log {
  void dump(string& str, const void* ptr, size_t len);
  void dump(const void* ptr, size_t len);
  void echo(const string& msg);
  void info(const string& msg);
  void warn(const string& msg);
  void erro(const string& msg, bool raw = false);
  void color(bool cr);
};

#endif	/* _UTILS_H_ */
