/* ***
 * @ $conf.cpp
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

/* ***
 * configuration manager
 * ***/
#include <fstream>
#include <string>
#include <cstring>

#include "conf.h"

using namespace std;

Configuration::Configuration(const char* conf) 
{
  this->conf  = string(conf);
  
  open_conf(conf);
}

Configuration::~Configuration()
{
  if (! sc_map.empty()) {
    map<string, map<string, string>>::iterator it;

    for (it = sc_map.begin(); it != sc_map.end(); it++) {
      if (! it->second.empty()) {
        it->second.clear();
      }
    }

    sc_map.clear();
  }
}

void Configuration::open_conf(const char* conf)
{
  if (conf == NULL) {
    conf = (char*) this->conf.c_str();
  }
  if (conf != NULL) {
    ifstream fin(this->conf = conf, ios::in);

    if (! fin.bad()) {
      string sec = DEFSEC;

      // 1k bytes is sufficient for one line.
      char line[1024];

      while(fin.getline(line, sizeof(line))) {
        char* pl = srpstr(line);
        if (*pl != '#' && *pl != ';') {
          if (*pl == '[') {
            /*modify variable, `sec'*/
            char* start = pl + 1;

            for (pl++; *pl != '\0' && *pl != ']'; pl++);

            if (*pl == ']') {
              *pl = '\0';
            }

            sec = string(start);
          } else {
            /*append name-value pair to map on section, `sec'*/
            char* start1 = pl;

            for ( ; *pl != '\0' && *pl != '='; pl++);

            bool isequal = *pl == '=' ? true : false;

            if (*pl != '\0') pl++; // symbol: = , skip to next byte.
          
            *(pl - 1) = '\0'; // delete equal symbol.

            char* start2 = pl;

            if (isequal) {
              add(sec, srpstr(start1), srpstr(start2));
            }
          }
        }
      }
      
      fin.close();
    }
  }
}

void Configuration::write_conf(const char* conf)
{
  if (conf == NULL) {
    conf = (char*) this->conf.c_str();;
  }
  if (conf != NULL) {
    ofstream fout(this->conf = conf, ios::out);
    
    if (! fout.bad()) {
      map<string, map<string, string>>::iterator itsc;

      for (itsc = sc_map.begin(); itsc != sc_map.end(); itsc++) {
        map<string, string>::iterator itnv;

        // [xxx]
        fout << "[" << itsc->first << "]" << endl;

        // xxx = xxx
        for (itnv = itsc->second.begin(); itnv != itsc->second.end(); itnv++) {
          fout << itnv->first << "=" << itnv->second << endl;
        }
      }

      fout.close();
    }
  }
}

map<string, string> Configuration::get(const string& sec)
{
  string secs = sec;

  if (secs.empty()) {
    secs = DEFSEC;
  }

  map<string, map<string, string>>::iterator itsc = sc_map.find(secs);

  if (itsc != sc_map.end()) {
    return itsc->second;
  }

  map<string, string> res;
  res.clear();

  return res;
}

string Configuration::get(const string& sec, const string& name)
{
  map<string, string> msec = get(sec);

  if (msec.size() > 0) {
    map<string, string>::iterator itnv = msec.find(name);

    if (itnv != msec.end()) {
      return itnv->second;
    }
  }
  
  return string("");
}

int Configuration::set(const string& sec, const string& name, const string& val)
{
  string secs = sec;

  if (secs.empty()) {
    secs = DEFSEC;
  }

  map<string, map<string, string>>::iterator itsc = sc_map.find(secs);

  if (itsc != sc_map.end()) {
    map<string, string>::iterator itnv = itsc->second.find(name);

    if (itnv != itsc->second.end()) {
      if (! itnv->second.empty()) {
        itnv->second.clear();
      }
      itnv->second = val;

      return 0;
    }
  }

  return 0;
}

void Configuration::add(const string& sec, const string& name, const string& val)
{
  string secs = sec;

  if (secs.empty()) {
    secs = DEFSEC;
  }

  map<string, map<string, string>>::iterator itsc = sc_map.find(secs);
  
  map<string, string> nv_map;

  bool isempty;

  if (itsc != sc_map.end()) {
    nv_map = itsc->second;
    isempty = false;
  } else {
    isempty = true;
  }

  nv_map.insert(pair<string, string>(name, val));

  if (isempty) { /*make sure this pair dont exists in sc_map before insert it, otherwise, it will gets fatal*/
    sc_map.insert(pair<string, map<string, string>>(secs, nv_map));
  } else {
    sc_map[secs].swap(nv_map);
  }
}

char* Configuration::srpstr(char* buf)
{
  size_t sz = strlen(buf);

  return utils::srpstr(buf, ' ', &sz);
}

/*end*/
