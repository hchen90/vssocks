/* $ @user.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_USER_H_
#define	_USER_H_

#include "crypto.h"

#define USER_COMPRESS   0x01
#define USER_BANDWIDTH  0x02
#define USER_EXPIRATION 0x04
#define USER_PRIVATEKEY 0x08

class Buffer;

class User : public Crypto {
public:
  User(const string& name, const string& usrnfo);
  User(const User& user);

  bool encode(Buffer& res, const void* ptr, size_t len); // will remove it soon
  bool decode(Buffer& res, const void* ptr, size_t len); // wiil remove it soon

  bool get_info(int lev, char* buf, size_t len);

  const string& get_username(void) const;
  bool          get_compress(void) const;
  const string& get_ciphername(void) const;
  const string& get_keyfile(void) const;
  size_t        get_bandwidth(void) const;
  time_t        get_expiration(void) const;

  void set_compress(bool comp);
  void set_ciphername(const string& meth);
  void set_keyfile(const string& keyfile, bool pri);
  void set_bandwidth(size_t bawi);
  void set_expiration(time_t expi);

  bool valid(void);

private:
  string  username;
  string  userinfo;

  int     tags;
  bool    compress;
  string  ciphername;
  string  keyfile;
  size_t  bandwidth;
  time_t  expiration;
};

#endif	/* _USER_H_ */
