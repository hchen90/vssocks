/* $ @conf.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * This file is autogenated by perl script. DO NOT edit it.
 * */
#ifndef	_CONF_H_
#define	_CONF_H_

#include <map>
#include <string>

#include "utils.h"

#define DEFSEC "global"

using namespace std;
using namespace utils;

class Configuration {
public:
  Configuration(const char* conf);
  ~Configuration();

  void open_conf(const char* conf = NULL);
  void write_conf(const char* conf = NULL);
  string get(const string& sec, const string& name);
  map<string, string> get(const string& sec);
  int set(const string& sec, const string& name, const string& val);
  void add(const string& sec, const string& name, const string& val);
protected:
  char* srpstr(char* buf);
private:
  string conf;
  map<string, map<string, string>> sc_map;
};

#endif	/* _CONF_H_ */
