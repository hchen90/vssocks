/* $ @crypto.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_CRYPTO_H_
#define	_CRYPTO_H_

#include <string>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/ossl_typ.h>
#include <openssl/err.h>

#include "utils.h"

using namespace std;

#define OFFSETBITS 12
#define LENGTHBITS 4
#define WINDOWSIZE (1 << OFFSETBITS)

class Crypto {
public:
  Crypto();
  Crypto(const Crypto& co);
  ~Crypto();

  bool encode(Buffer& res, const void* ptr, size_t len, bool enc = true);
  bool decode(Buffer& res, const void* ptr, size_t len);

  bool set_ciphername(const string& ciphername);
  bool set_password(const string& password, unsigned char* key = NULL, unsigned char* iv = NULL, unsigned short* iv_len = NULL);

  bool lzss_encode(Buffer& res, const void* ptr, size_t len);
  bool lzss_decode(Buffer& res, const void* ptr, size_t len);

  bool set_keyfile(const string& keyfile, bool prikey);

  bool rsa_encode(Buffer& res, const void* ptr, size_t len, bool pri, bool enc = true);
  bool rsa_decode(Buffer& res, const void* ptr, size_t len, bool pri);

  bool valid(void);
private:
  pair<unsigned char*, size_t> lzss_search_maxpat(unsigned char* head, unsigned char* tail, unsigned char* end);

  EVP_CIPHER_CTX* ctx;
  const EVP_CIPHER* ci;

  unsigned char key[64]; /*big key buffer is harmless*/
  unsigned char iv[16];
  unsigned short iv_len;

  bool okay;

  RSA* pri_rsa,* pub_rsa;

  string ciphername, password, keyfile[2];
};

#endif	/* _CRYPTO_H_ */
