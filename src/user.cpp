/* ***
 * @ $user.cpp
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
#include <vector>

#include "config.h"
#include "user.h"
#include "utils.h"

#include "xstring.h"

using namespace std;
using namespace utils;

User::User(const string& usrnam, const string& usrnfo) : tags(0), compress(false), bandwidth(0), expiration(0)
{
  if (! usrnam.empty() && ! usrnfo.empty()) {
    username = usrnam;
    userinfo = usrnfo;

    vector<string> tok;

    if (tokstr(usrnfo.c_str(), usrnfo.size(), tok) == 5) {
      tags = atoi(tok[0].c_str()); // tags
      ciphername  = tok[1]; // cipher name
      string& pass = tok[2]; // keyfile or password
      string& banw = tok[3]; // bandwidth
      string& expi = tok[4]; // expiration

      if (! ciphername.empty()) {
        set_ciphername(ciphername); // set ciphername
      }

      if (ciphername == "rsa") {
        set_keyfile(pass, tags & USER_PRIVATEKEY); // set keyfile
      } else {
        set_password(pass); // set password
      }

      if (tags & USER_COMPRESS) { // enable compress
        set_compress(true);
      }

      if (tags & USER_BANDWIDTH && ! banw.empty()) { // enable bandwidth
        set_bandwidth(atocap(banw.c_str(), banw.size()));
      }

      if (tags & USER_EXPIRATION && ! expi.empty()) { // enable expiration
        set_expiration((time_t) atol(expi.c_str()));
      }
    }
  }
}

User::User(const User& user) : User(user.username, user.userinfo) {}

bool User::encode(Buffer& res, const void* ptr, size_t len, pthread_t id)
{
  res.reset();
  //res.app(ptr, len); return true;
  //xstring str;
  //str.printf("[%08x] \033[36mencode - ptr:%p, len:%u\033[0m", id, ptr, len);
  //log::dump(str, ptr, 16);
  //res.app(ptr, len); return true;
  /*res.reset(); res.app(ptr, len);
  res.alloc(len); char* p = (char*) res.ptr();
  for (size_t i = 0; i < len; i++) p[i] = (*(char*) ((char*) ptr + i)) ^ 0x66;
  res.resize(len);*/
  //xstring xs;
  //xs.printf("\033[;1mEN\033[0m:\033[43mlen:%4u\033[0m", len);
  //log::dump(xs, ptr, 16);
  bool rev = Crypto::encode(res, ptr, len);
  //xs.printf("\033[44mres.size():%4u\033[0m", res.size());
  //log::dump(xs, res.ptr(), 16);
  //log::info(xs);
  //str.printf("\033[36msize:%u\033[0m", res.size());
  //log::echo(str);
  return rev;
}

bool User::decode(Buffer& res, const void* ptr, size_t len, pthread_t id)
{
  res.reset();
  //res.app(ptr, len); return true;
  /*res.reset(); res.app(ptr, len);
  res.alloc(len); char* p = (char*) res.ptr();
  for (size_t i = 0; i < len; i++) p[i] = (*(char*) ((char*) ptr + i)) ^ 0x66;
  res.resize(len);*/
  //xstring xs;
  //xs.printf("\033[;1mDE\033[0m:\033[45mlen:%4u\033[0m", len);
  //log::dump(xs, ptr, 16);
  bool rev = Crypto::decode(res, ptr, len);
  //xs.printf("\033[46mres.size():%4u\033[0m", res.size());
  //log::dump(xs, res.ptr(), 16);
  //log::info(xs);
  /*xstring str;
  str.printf("[%08x] \033[35mdecode - len: %u - ptr:%p, size:%u\033[0m", id, len, res.ptr(), res.size());
  log::dump(str, res.ptr(), 16);
  log::echo(str);*/

  return rev;
}

bool User::get_info(int lev, char* buf, size_t len)
{
  if (buf != NULL) {
    if (lev) {
      return snprintf(buf, len, "%s:%s:%ld:%ld - [%s]", username.c_str(), ciphername.c_str(), bandwidth, expiration, valid() ? "okay" : "fatal") > 0;
    } else {
      return snprintf(buf, len, "%s:%s:%ld:%ld", username.c_str(), ciphername.c_str(), bandwidth, expiration) > 0;
    }
  }

  return false;
}

const string& User::get_username(void) const
{
  return username;
}

bool User::get_compress(void) const
{
  return compress;
}

const string& User::get_ciphername(void) const
{
  return ciphername;
}

const string& User::get_keyfile(void) const
{
  return keyfile;
}

size_t User::get_bandwidth(void) const
{
  return bandwidth;
}

time_t User::get_expiration(void) const
{
  return expiration;
}

void User::set_compress(bool comp)
{
  compress = comp;
}

void User::set_ciphername(const string& meth)
{
  Crypto::set_ciphername(ciphername = meth);
}

void User::set_keyfile(const string& keyfile, bool pri)
{
  Crypto::set_keyfile(this->keyfile = keyfile, pri);
}

void User::set_bandwidth(size_t bawi)
{
  bandwidth = bawi;
}

void User::set_expiration(time_t expi)
{
  expiration = expi;
}

bool User::valid(void)
{
  return Crypto::valid();
}

// end
