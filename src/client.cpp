/* ***
 * @ $client.cpp
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
#include "client.h"
#include "utils.h"

using namespace std;

bool Client::connect(const Server& srv)
{
  string hostip; int port;

  if (srv.gethostaddr(hostip, port)) {
    return connect(hostip, port);
  }

  return false;
}

bool Client::connect(const string& hostip, int port)
{
  if (hostip.empty()) return false;

  struct addrinfo* addr = NULL;

  if (! resolve(hostip.c_str(), port, &addr)) {
    if (  Socks::socket(addr->ai_family, SOCK_STREAM) != -1 && \
          Socks::connect(addr->ai_addr, addr->ai_addrlen) != -1 )
      return true;
    resolve(NULL, 0, &addr);
  }

  return false;
}

bool Client::connect(const struct sockaddr* addr, socklen_t addr_len)
{
  if (addr == NULL) return false;

  char hostip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)]; int port;

  if (! resolve(addr, hostip, port)) {
    return connect(hostip, port);
  }

  return false;
}

/*end*/
