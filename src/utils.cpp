/* ***
 * @ $utils.c
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
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#include "config.h"
#include "utils.h"
#include "xstring.h"

/*namespace: utils*/

char* utils::skpstr(char* buf, int ch, size_t len, int where)
{
  char* p;
  
  if (! where) p = buf;
  else p = buf + len - 1;

  if (buf == NULL) return buf;

  if (! where) for ( ; p < buf + len && *p == ch; p++) ;
  else for ( ; p >= buf && *p == ch; p--) ;

  return p;
}

char* utils::srpstr(char* buf, int ch, size_t* len, int where)
{
  char* a = buf;
  
  if (where & 1) {
    if ((a = skpstr(buf, ch, *len, 1) + 1) > buf) { /*from end: |... <---|*/
      *len = a - buf;
      *a = '\0';
      a = buf;
    }
  }

  if (where & 2) {
    if ((a = skpstr(buf, ch, *len)) < buf + *len) { /*from front: |---> ...|*/
      *len -= a - buf;
    }
  }

  return a;
}

size_t utils::tokstr(const char* buf, size_t len, vector<string>& tt)
{
  size_t i, j; 
  char* p = new char[len + 1];

  if (p == NULL) return 0;

  memcpy(p, buf, len);
  *(p + len) = 0;

  for (i = 0, j = 0; i <= len; i++) {
    if (*(p + i) == ':' || *(p + i) == '\0') { /*seperator or EOF*/
      *(p + i) = 0;

      size_t l = strlen(p + j);
      char* s = srpstr((char*) (p + j), ' ', &l, 3);
      
      tt.push_back(s);
      
      j = i + 1;
    }
  }

  delete[] p;

  return tt.size();
}

char* utils::genramstr(char* buf, size_t len)
{
  size_t i;

  srandom(time(NULL));

  for (i = 0; i < len - 1; i++) {
    unsigned char rad = random() * (10 + 26 * 2) / RAND_MAX;
    if (rad < 10) {
      buf[i] = '0' + rad;
    } else if (rad >= 10 && rad < 36) {
      buf[i] = 'A' + rad - 10;
    } else {
      buf[i] = 'a' + rad - 36;
    }
  }

  if (i < len) {
    buf[i] = 0;
  }

  return buf;
}

size_t utils::atocap(const char* buf, size_t len)
{
  if (buf == NULL) return 0;

  char* ptr = new char[len + 1];
  char ch = buf[len - 1];
  size_t n = 1;

  if (ptr == NULL) return 0;

  memcpy(ptr, buf, len);
  ptr[len] = 0;

  if (ch > '9') {
    ch &= 0xdf;
    ptr[len - 1] = 0;
  }

  if (ch == 'K') {
    n = 1024;
  } else if (ch == 'M') {
    n = 1024 * 1024;
  } else if (ch == 'G') {
    n = 1024 * 1024 * 1024;
  }

  size_t l = atol(ptr);

  delete[] ptr;

  if (l > 0) {
    return l * n;
  }

  return 0;
}

bool utils::gethostnameport(const string& host, string& hostip, int& port)
{
  if (! host.empty()) {
    size_t i, l = host.size();
    const char* p = host.c_str();

    for (i = 0; i < l; i++) {
      if (p[l - i - 1] >= '0' && p[l - i - 1] <= '9') {
        continue;
      } else if (p[l - i - 1] == ':') {
        bool ret = true;
        if (i < l) port = atoi(p + l - i);
        else ret = false;
        if (i > 1) hostip = string(p, l - i - 1);
        else ret = false;
        return ret;
      } else break;
    }

    hostip = host;
    
    return true;
  }

  return false;
}

ssize_t utils::recvall(Buffer& res, int soc)
{
  if (soc == -1) return -1;

  short len = 0;

  // block length (size: 2 bytes)
  ssize_t rcved = recv(soc, (char*) &len, sizeof(len), 0);

  if (rcved == sizeof(len) && len > 0) {
    res.alloc(len);
    res.reset();

    // block data (size: variable)
    return recv(soc, res.ptr(), len, 0);
  }

  return -1;
}

ssize_t utils::sendall(const void* ptr, size_t len, int soc)
{
  if (soc == -1) return -1;

  short bs = (short) len;

  // block size
  if (send(soc, (char*) &bs, sizeof(bs), 0) > 0) {
    // block data
    return send(soc, ptr, len, 0);
  }
  
  return -1;
}

void utils::setnonblock(int soc, bool nb)
{
  if (soc > 0) {
    int flags = fcntl(soc, F_GETFL, 0);
    if (nb) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    fcntl(soc, F_SETFL, flags);
  }
}

/*namespace: log*/

void log::dump(string& str, const void* ptr, size_t len)
{
  size_t i, l;

  for (i = 0, l = 16; i < len; i += 16) {
    if (len - i < 16) {
      l = len - i;
      break;
    }

    unsigned char buf[16], txt[17];

    memcpy(buf, (char*) ((char*) ptr + i), sizeof(buf));
    
    char suf[128];

    size_t x;

    for (x = 0; x < 16; x++)
      if (buf[x] < 0x20 || buf[x] > 0x7e) txt[x] = '.';
      else txt[x] = buf[x];
    txt[16] = 0;

    snprintf(suf, sizeof(suf), "%02x %02x %02x %02x %02x %02x %02x %02x - %02x %02x %02x %02x %02x %02x %02x %02x ; %s", \
    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], \
    buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], txt);
    
    str += suf;
  }

  if (l < 16) {
    unsigned char* p = (unsigned char*) ((char*) ptr + i);
    char suf[128],* s = suf;
    int n, k = sizeof(suf), j;

    for (j = 0; j < 16; j++) {
      if (j == 8) {
        s[0] = '-'; s[1] = ' ';
        s += 2; k -= 2;
      }
      if (i + j < len) {
        n = snprintf(s, k, "%02x ", p[j]);
      } else {
        s[0] = s[1] = s[2] = ' ';
        n = 3;
      }
      if (n < 0) break;
      else {
        s += n;
        k -= n;
      }
    }

    char txt[17];

    for (j = 0; j < 16; j++)
      if (p[j] < 0x20 || p[j] > 0x7e) txt[j] = '.';
      else txt[j] = p[j];
    txt[16] = 0;

    snprintf(s, k, "; %s", txt);

    str += suf;
  }
}

void log::dump(const void* ptr, size_t len)
{
  string str = "= ";

  log::dump(str, ptr, len);

  log::echo(str);
}

pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

void log::echo(const string& msg)
{
  pthread_mutex_lock(&g_mutex);
  cout << "\r" << msg << endl;
  pthread_mutex_unlock(&g_mutex);
}

static bool en_cr = false;

void log::info(const string& msg)
{
  pthread_mutex_lock(&g_mutex);
  if (en_cr) cout << "\r- \033[32m" << msg << "\033[0m" << endl;
  else cout << "\r- " << msg << endl;
  pthread_mutex_unlock(&g_mutex);
}

void log::warn(const string& msg)
{
  pthread_mutex_lock(&g_mutex);
  if (en_cr) cout << "\r! \033[33m" << msg << "\033[0m" << endl;
  else cout << "\r! " << msg << endl;
  pthread_mutex_unlock(&g_mutex);
}

void log::erro(const string& msg, bool raw)
{
  pthread_mutex_lock(&g_mutex);
  if (raw) {
    if (en_cr) cerr << "\r* \033[31m" << msg << "\033[0m" << endl;
    else cerr << "\r* " << msg << endl;
  } else {
    int en = errno;

    if (en_cr) cerr << "\r* \033[31m" << msg << ":" << strerror(en) << " (" << en << ")" << "\033[0m" << endl;
    else cerr << "\r* " << msg << ":" << strerror(en) << " (" << en << ")" << endl;
  }
  pthread_mutex_unlock(&g_mutex);
}

void log::color(bool cr)
{
  en_cr = cr;
}

/*end*/
