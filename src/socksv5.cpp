/* ***
 * @ $socksv5.cpp
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

#include "buffer.h"
#include "socksv5.h"
#include "utils.h"
#include "socks.h"

#define STATUS_IPV4_LENGTH 10
#define STATUS_IPV6_LENGTH 22
#define STATUS_INDEX 1

bool std::assem_socksv5reply(Socks& srv, const string& ipp, Buffer& rep)
{
  bool ret = false;
  string hostip; int port;

  if (utils::gethostnameport(ipp, hostip, port)) {
    struct addrinfo* an = NULL;
    if (! srv.resolve(hostip.c_str(), port, &an)) {
      if (an->ai_family == AF_INET && rep.alloc(STATUS_IPV4_LENGTH)) { // IPv4
        struct sockaddr_in* sin = (struct sockaddr_in*) an->ai_addr;
        if (sin != NULL) {
          unsigned char* p = (unsigned char*) rep.ptr();
          p[0] = SOCKSV5_VER;
          p[1] = p[2] = 0;
          p[3] = SOCKSV5_ATYP_IPV4;
          *(unsigned int*) (p + 4) = *(unsigned int*) &sin->sin_addr.s_addr;
          *(unsigned short*) (p + 8) = sin->sin_port;
          rep.resize(STATUS_IPV4_LENGTH);
          ret = true;
        }
      } else if (an->ai_family == AF_INET6 && rep.alloc(STATUS_IPV6_LENGTH)) { // IPv6
        struct sockaddr_in6* sin6 = (struct sockaddr_in6*) an->ai_addr;
        if (sin6 != NULL) {
          unsigned char* p = (unsigned char*) rep.ptr();
          p[0] = SOCKSV5_VER;
          p[1] = p[2] = 0;
          p[3] = SOCKSV5_ATYP_IPV6;
          memcpy(p + 4, (char*) &sin6->sin6_addr, 16);
          *(unsigned short*) (p + 20) = sin6->sin6_port;
          rep.resize(STATUS_IPV6_LENGTH);
          ret = true;
        }
      } else { // hostname
        unsigned char  l = hostip.size();
        if (rep.alloc(l + 7)) {
          unsigned char* p = (unsigned char*) rep.ptr();
          p[0] = SOCKSV5_VER;
          p[1] = p[2] = 0;
          p[3] = SOCKSV5_ATYP_DOMAINNAME;
          p[4] = l;
          memcpy(p + 5, hostip.c_str(), l);
          *(unsigned short*) (p + l + 5) = port;
          rep.resize(l + 7);
          ret = true;
        }
      }
      srv.resolve(NULL, 0, &an);
    }
  }

  return ret;
}

ssize_t std::send_status(User* user, int soc, char status, pthread_t id, const Buffer& rep)
{
  Buffer  reply = rep;

  reply[STATUS_INDEX] = status;

  Buffer  res;
  char*   ptr = (char*) reply.ptr();;
  size_t  len = reply.size();

  if (user != NULL) {
    if (user->encode(res, ptr, len, id)) {
      ptr = (char*) res.ptr();
      len = res.size();
    }
  }

  return utils::sendall(ptr, len, soc);
}

/*end*/
