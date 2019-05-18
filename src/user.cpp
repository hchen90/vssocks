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

User::User(const string& usrnam, const string& usrnfo) : tags(0), compress(false)
{
  if (! usrnam.empty() && ! usrnfo.empty()) {
    username = usrnam;
    userinfo = usrnfo;

    vector<string> tok;

    if (tokstr(usrnfo.c_str(), usrnfo.size(), tok) >= 3) {
      tags = atoi(tok[0].c_str()); // tags
      ciphername  = tok[1]; // cipher name
      string& pass = tok[2]; // keyfile or password

      if (! ciphername.empty()) {
        set_ciphername(ciphername); // set ciphername
      }

      if (ciphername == "rsa") {
        set_keyfile(pass); // set keyfile
      } else {
        set_password(pass); // set password
      }

      if (tags & USER_COMPRESS) { // enable compress
        set_compress(true);
      }
    }
  }
}

User::User(const User& user) : User(user.username, user.userinfo) {}

bool User::encode(Buffer& res, const void* ptr, size_t len, pthread_t id)
{
#ifdef DEBUG
  xstring msg;
  msg.printf("[%08x] User::encode(ptr = %p, len = %u):", id, ptr, len);
  log::dump(msg, ptr, 16);
  log::info(msg);
#endif
  bool ret = true;

  Buffer lzss;

  if (tags & USER_COMPRESS) { // compress data if it's enabled
    if (Crypto::lzss_encode(lzss, ptr, len)) {
      ptr = lzss.ptr(); len = lzss.size();
    } else {
      ret = false;
    }
  }

  if (! ret) return false;

  res.reset();

  if (ciphername == "rsa") {
    return Crypto::rsa_encode(res, ptr, len, tags & USER_PRIKEY); // encrypt data with RSA
  } else {
    return Crypto::encode(res, ptr, len); // encrypt data
  }
}

bool User::decode(Buffer& res, const void* ptr, size_t len, pthread_t id)
{
#ifdef DEBUG
  xstring msg;
  msg.printf("[%08x] User::decode(ptr = %p, len = %u):", id, ptr, len);
  log::dump(msg, ptr, 16);
  log::info(msg);
#endif
  bool ret = false;

  res.reset();

  // decrypt data
  if (ciphername == "rsa") {
    ret = Crypto::rsa_decode(res, ptr, len, tags & USER_PRIKEY);
  } else {
    ret = Crypto::decode(res, ptr, len);
  }

  if (ret && tags & USER_COMPRESS) {
    Buffer lzss;

    // decompress data
    if (Crypto::lzss_decode(lzss, res.ptr(), res.size())) {
      res = lzss;
    } else {
      ret = false;
    }
  }

  return ret;
}

bool User::get_info(int lev, char* buf, size_t len)
{
  if (buf != NULL) {
    if (lev) {
      return snprintf(buf, len, "%s:%s - [%s]", username.c_str(), ciphername.c_str(), valid() ? "okay" : "fatal") > 0;
    } else {
      return snprintf(buf, len, "%s:%s", username.c_str(), ciphername.c_str()) > 0;
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

const string& User::get_password(void) const
{
  return password;
}

const string& User::get_keyfile(void) const
{
  return keyfile;
}

void User::set_compress(bool comp)
{
  compress = comp;
}

void User::set_ciphername(const string& ciphername)
{
  Crypto::set_ciphername(this->ciphername = ciphername);
}

void User::set_password(const string& password)
{
  Crypto::set_password(this->password = password);
}

void User::set_keyfile(const string& keyfile)
{
  Crypto::set_keyfile(this->keyfile = keyfile, tags & USER_PRIKEY);
}

// end
