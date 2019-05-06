/* $ @proxy.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * This file is autogenated by perl script. DO NOT edit it.
 * */
#ifndef	_PROXY_H_
#define	_PROXY_H_

#include <string>

#include "config.h"
#include "user.h"
#include "thread.h"
#include "conf.h"
#include "server.h"
#include "client.h"

#ifndef TCP_FASTOPEN
  #define TCP_FASTOPEN 23
#endif

#define INIT_USER   0x01
#define INIT_LOCAL  0x02
#define INIT_SERVER 0x04

using namespace std;

class Proxy : public Configuration {
public:
  Proxy(const string& conf);
  ~Proxy();
  
  void run(void);
  void stop(void);
protected:
  void set_userinfo(const string& usrnam, const string& usrnfo);
  void set_fastopen(bool fo);
private:
  static void cleanup_client(void* args);
  static void* start_client(void* args);
  
  void local_read(Client* srv, int fd1, int fd2);
  
  Server  local;
  User*   user;
  bool    okay;
  double  timeout;

  string  server_ip;
  int     server_port;

  Thread  threads;
};

#endif	/* _PROXY_H_ */
