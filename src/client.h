/* $ @server.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "socks.h"
#include "server.h"

class Client : public Socks {
public:
  bool connect(const Server& srv);
  bool connect(const string& hostip, int port);
  bool connect(const struct sockaddr* addr, socklen_t addr_len);
};

#endif /* _CLIENT_H_ */
