/* ***
 * @ $server.cpp
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
#include "config.h"
#include "server.h"
#include "utils.h"
#include "xstring.h"

using namespace std;
using namespace utils;

/*class Server*/
Server::Server() : port(0) {}

bool Server::setup(const string& hostip, int port)
{
  struct addrinfo* addr = NULL;
  int num = 0;

  if (! resolve(hostip.c_str(), port, &addr)) {
    struct addrinfo* pt;

    for (pt = addr; pt != NULL; pt = pt->ai_next) {
      num |= setup(pt->ai_addr, pt->ai_addrlen) ? 1 : 0;
    }

    resolve(NULL, 0, &addr); // release info
  }

  return num > 0 ? true : false;
}

bool Server::setup(const struct sockaddr* addr, socklen_t addr_len)
{
  if (addr == NULL) return false;

  char hostip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)]; int port;

  if (resolve((struct sockaddr*) addr, hostip, port)) return false;

  if (socket(addr->sa_family, SOCK_STREAM) == -1) {
    xstring msg = "Server:setup:socket() on [";
    msg.printf("%s:%u]", hostip, (unsigned int) port);
    log::erro(msg);
  } else if (bind(hostip, port) == -1) {
    xstring msg = "Server:setup:bind() on [";
    msg.printf("%s:%u", hostip, (unsigned int) port);
    log::erro(msg);
  } else if (listen() == -1) {
    xstring msg = "Server:setup:listen() on [";
    msg.printf("%s:%u]", hostip, (unsigned int) port);
    log::erro(msg);
  } else {
    xstring msg = "Server:setup:server started on [";
    msg.printf("%s:%u]", hostip, (unsigned int) port);
    log::info(msg);
    this->hostip = hostip;
    this->port = port;
    return true;
  }

  return false;
}

bool Server::gethostaddr(string& hostip, int& port) const
{
  if (! this->hostip.empty()) {
    hostip = this->hostip;
    port = this->port;
    return true;
  } else {
    return false;
  }
}

/*end*/
