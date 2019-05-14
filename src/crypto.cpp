/* ***
 * @ $crypto.cpp
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
#include <cstring>

#include "config.h"
#include "crypto.h"

Crypto::Crypto() : ctx(NULL), ci(NULL), iv_len(0), okay(false), pri_rsa(NULL), pub_rsa(NULL)
{
  ctx = EVP_CIPHER_CTX_new();
}

Crypto::Crypto(const Crypto& co) : Crypto()
{
  set_ciphername(co.ciphername);
  set_password(co.password);
  set_keyfile(co.keyfile[0], true);
  set_keyfile(co.keyfile[1], false);
}

Crypto::~Crypto()
{
  if (ctx != NULL) EVP_CIPHER_CTX_free(ctx);
  if (pri_rsa != NULL) RSA_free(pri_rsa);
  if (pub_rsa != NULL) RSA_free(pub_rsa); 
}

bool Crypto::encode(Buffer& res, const void* ptr, size_t len, bool enc)
{
  if (ptr != NULL && len > 0 && ci != NULL && res.alloc(len + 2)) {
    unsigned char* p = (unsigned char*) res.ptr(),* x = (unsigned char*) ptr;
    int n = 0, m = MIN(BUFF_SIZE, len);
    ssize_t l = len, k = 0;

    if (EVP_CipherInit_ex(ctx, ci, NULL, key, iv, enc)) {
      while (l > 0) {
        if (l < m) m = l;
        if (EVP_CipherUpdate(ctx, p, &n, x, m)) {
          x += m;
          p += n;
          l -= m;
          k += n;
        } else {
          break;
        }
      }

      if (EVP_CipherFinal(ctx, p, &n)) {
        k += n;
      }

      res.resize(k);

      return k > 0;
    }
  }

  return false;
}

bool Crypto::decode(Buffer& res, const void* ptr, size_t len)
{
  return encode(res, ptr, len, false);
}

bool Crypto::set_ciphername(const string& ciphername)
{
  if (ciphername == "rsa") {
    okay = true;
    return true;
  } else {
    this->ciphername = ciphername;
    this->ci = EVP_get_cipherbyname(ciphername.c_str());

    unsigned short len = 16;

    if (ciphername == "chacha20-ietf") {
      len = 12;
    } else if (ciphername == "bf-cfb" || ciphername == "chacha20" || ciphername == "salsa20") {
      len = 8;
    }

    okay = true;

    this->iv_len = len;

    return this->ci != NULL;
  }
}

bool Crypto::set_password(const string& password, unsigned char* key, unsigned char* iv, unsigned short* iv_len)
{
  if (okay && this->ci != NULL) {
    this->password = password;

    int ret = EVP_BytesToKey(this->ci, EVP_md5(), NULL, (unsigned const char*) password.c_str(), password.size(), 1, this->key, this->iv);
    
    if (key != NULL) {
      memcpy(key, this->key, sizeof(this->key));
    }

    if (iv != NULL) {
      memcpy(iv, this->iv, sizeof(this->iv));
    }

    if (iv_len != NULL) {
      *iv_len = this->iv_len;
    }

    return ret > 0;
  }

  return false;
}

bool Crypto::lzss_encode(Buffer& res, const void* ptr, size_t len)
{
  size_t size = 0;

  if (ptr == NULL) return false;

  unsigned char* head,* tail,* src,* dst;

  if (! res.alloc(len * 3)) return false;

  dst = (unsigned char*) res.ptr();

  if (dst == NULL) return false;

  src = (unsigned char*) ptr;

  tail = src;

  unsigned int nc = 1, control_count = 0;

  /*  *********************************
   *  src|00-----------------------15|
   *       |
   *     control_bit_flags
   *  ********************************* */

  dst[0] = (len >> 24) & 0xff;
  dst[1] = (len >> 16) & 0xff;
  dst[2] = (len >>  8) & 0xff;
  dst[3] = (len >>  0) & 0xff;

  size += 4;

  unsigned char* dm = dst + 4;
  unsigned char buf[2 * 8 + 2]; /*2 * 8 + 1 byte for flags, total 17 bytes is required, on more byte for alignment*/

  buf[0] = 0;

  for ( ; ; ) {
    head = tail - WINDOWSIZE;

    if (head < src) head = src;

    if (control_count >= 8) {
      memcpy(dm, buf, nc);
      buf[0] = 0;
      dm += nc;
      size += nc;
      nc = 1;
      control_count = 1;
    } else {
      control_count++;
    }

    if (tail > src + len - 1) break;

    if (head < tail) {
      pair<unsigned char*, size_t> pt = lzss_search_maxpat(head, tail, src + len);

      if (pt.second > 2) { /*only pattern's length more than 2 bytes is worth to save*/
        buf[0]   <<= 1;
        buf[0]    |= 1;
        buf[nc++] = (unsigned char) (( (tail - pt.first) >> LENGTHBITS) & 0xff);
        buf[nc++] = (unsigned char) ((((tail - pt.first) << LENGTHBITS) + pt.second - 2) & 0xff);
        tail      += pt.second;
      } else {
        buf[0]   <<= 1;
        buf[nc++] = *tail;
        tail++;
      }
    } else {
      buf[0]   <<= 1;
      buf[nc++] = *tail;
      tail++;
    }
  }

  for ( ; control_count <= 8; control_count++) buf[0] <<= 1;
  memcpy(dm, buf, nc);
  size += nc;

  res.resize(size);

  return true;
}

bool Crypto::lzss_decode(Buffer& res, const void* ptr, size_t len)
{
  size_t size = 0;

  if (ptr == NULL) return false;
  
  unsigned char* tail,* src,* dst;

  src = (unsigned char*) ptr;

  size = (src[0] << 24 & 0xff000000) + (src[1] << 16 & 0xff0000) + (src[2] << 8 & 0xff00) + (src[3] << 0 & 0xff);

  if (! res.alloc(size)) return false;

  dst = (unsigned char*) res.ptr();
  
  tail = dst;
  src += 4;

  unsigned char control_flags = src[0];
  unsigned int  control_count = 0;

  src++;

  for ( ; ; ) {
    if (src >= ((unsigned char*) ptr + len)) break;

    if (control_count >= 8) {
      control_flags = src[0];
      src++;
      control_count = 1;
    } else {
      control_count++;
    }

    if (control_flags >> 7) { /*compressed tag*/
      unsigned int offset = (src[0] << LENGTHBITS & 0xfff0) + (src[1] >> LENGTHBITS & 0x0f);
      unsigned int length = (src[1] & 0x0f) + 2, i;

      for (i = 0; i < length; i++) {
        *(tail + i) = *(unsigned char*) (tail - offset + i);
      }

      tail += length;
      src += 2;
      control_flags <<= 1;
    } else { /*transfer literal byte*/
      *tail = *src;
      tail++;
      src++;
      control_flags <<= 1;
    }
  }

  res.resize(size);

  return true;
}

bool Crypto::set_keyfile(const string& keyfile, bool prikey)
{
  if (keyfile.empty()) return false;

  if (prikey) this->keyfile[0] = keyfile;
  else this->keyfile[1] = keyfile;

  BIO* bio;

  if (prikey) {
    if ((bio = BIO_new(BIO_s_file())) != NULL) {
      BIO_read_filename(bio, keyfile.c_str());
      if (pri_rsa != NULL) RSA_free(pri_rsa);
      pri_rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);
      BIO_free_all(bio);
    }
  } else {
    if ((bio = BIO_new(BIO_s_file())) != NULL) {
      BIO_read_filename(bio, keyfile.c_str());
      if (pub_rsa != NULL) RSA_free(pub_rsa);
      pub_rsa = PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);
      BIO_free_all(bio);
    }
  }

  if ((prikey && pri_rsa == NULL) || (! prikey && pub_rsa == NULL)) {
    string str = "Crypto:set_keyfile(";

    str += keyfile;
    str += "):";

    char  buf[BUFF_SIZE];
    
    ERR_load_crypto_strings();
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    ERR_free_strings();

    str += buf;

    log::erro(str, true);

    return okay = false;
  }

  return okay = true;
}

bool Crypto::rsa_encode(Buffer& res, const void* ptr, size_t len, bool pri, bool enc)
{
  ssize_t         rsalen = 0;
  unsigned char*  rsaptr = NULL;
  bool            okay = false;

  if (ptr == NULL) return false;

  RSA* rsa = pri ? pri_rsa : pub_rsa;

  if (rsa != NULL) {
    ssize_t rsasiz = RSA_size(rsa);
    ssize_t count, remain, length;

    if (enc) {
      count = len / (rsasiz - 11);
      remain = len % (rsasiz - 11);
    } else {
      count = len / rsasiz;
      remain = len % rsasiz;
    }

    length = (count + (remain > 0 ? 1 : 0)) * rsasiz;

    if (res.alloc(length)) {
      unsigned char* dst = rsaptr = (unsigned char*) res.ptr(),* src = (unsigned char*) ptr;
      ssize_t srcl;

      if (enc) {
        srcl = count > 0 ? (rsasiz - 11) : remain;
      } else {
        srcl = rsasiz;
      }

      do {
        if (enc) {
          if (pri) {
            length = RSA_private_encrypt(srcl, src, dst, rsa, RSA_PKCS1_PADDING);
          } else {
            length = RSA_public_encrypt(srcl, src, dst, rsa, RSA_PKCS1_PADDING);
          }
        } else {
          if (pri) {
            length = RSA_private_decrypt(srcl, src, dst, rsa, RSA_PKCS1_PADDING);
          } else {
            length = RSA_public_decrypt(srcl, src, dst, rsa, RSA_PKCS1_PADDING);
          }
        }

        if (length > 0) {
          rsalen += length;
          okay = true;
        } else {
          break;
        }

        count--;
        src += srcl;

        if (enc) {
          srcl = count > 0 ? (rsasiz - 11) : remain;
        } else {
          srcl = rsasiz;
        }

        dst += length;
      } while (count >= 0);
    }
  }

  res.resize(rsalen);

  return okay;
}

bool Crypto::rsa_decode(Buffer& res, const void* ptr, size_t len, bool pri)
{
  return rsa_encode(res, ptr, len, pri, false);
}

/* search pattern
 * head|-------pt<---|tail------...---|end<EOF>
 * */
pair<unsigned char*, size_t> Crypto::lzss_search_maxpat(unsigned char* head, unsigned char* tail, unsigned char* end)
{
  unsigned char* ptr = NULL;
  unsigned char* pt = tail - 1;
  size_t len = 0, i;

  if (pt < head) {
    pt = head;
  }

  for ( ; pt >= head; pt--) {
    if (*pt == *tail) {
      /*why (1 << LENGTHBITS) + 1 rather than (1 << LENGTH) - 1?
        because pattern length no less than 2 bytes, reserve 2 bytes for storing pattern length*/
      for (i = 0; i < (1 << LENGTHBITS) + 1 && (pt + i) > head && (pt + i) < tail && (tail + i) < end && pt[i] == tail[i]; i++);
      if (i > len) {
        len = i;
        ptr = pt;
      }
    }
  }

  return make_pair(ptr, len);
}

bool Crypto::valid(void)
{
  return okay;
}

/*end*/
