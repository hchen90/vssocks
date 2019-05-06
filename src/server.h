/* $ @server.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_SERVER_H_
#define	_SERVER_H_

#include <string>

#include "socks.h"

using namespace std;

class Server : public Socks {
public:
  Server();

  bool setup(const string& hostip, int port);
  bool setup(const struct sockaddr* addr, socklen_t addr_len);
  bool gethostaddr(string& hostip, int& port) const;
private:
  string hostip;
  int port;
};

#endif	/* _SERVER_H_ */
