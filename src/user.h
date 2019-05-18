/* $ @user.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_USER_H_
#define	_USER_H_

#include "crypto.h"

#define USER_COMPRESS 0x01
#define USER_PRIKEY   0x02

class Buffer;

class User : public Crypto {
public:
  User(const string& name, const string& usrnfo);
  User(const User& user);

  bool encode(Buffer& res, const void* ptr, size_t len, pthread_t id);
  bool decode(Buffer& res, const void* ptr, size_t len, pthread_t id);

  bool get_info(int lev, char* buf, size_t len);

  const string& get_username(void) const;
  bool          get_compress(void) const;
  const string& get_ciphername(void) const;
  const string& get_password(void) const;
  const string& get_keyfile(void) const;

  void set_compress(bool comp);
  void set_ciphername(const string& ciphername);
  void set_password(const string& password);
  void set_keyfile(const string& keyfile);
private:
  string  username;
  string  userinfo;

  int     tags;
  bool    compress;
  string  ciphername;
  string  password;
  string  keyfile;
};

#endif	/* _USER_H_ */
