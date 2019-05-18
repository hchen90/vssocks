/* $ @peer.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_PEER_H_
#define	_PEER_H_

#include <string>
#include <map>

#include "config.h"
#include "thread.h"
#include "user.h"
#include "conf.h"
#include "socksv5.h"
#include "server.h"
#include "client.h"

#ifndef TCP_FASTOPEN
  #define TCP_FASTOPEN 23
#endif

#define INIT_UNDEF  0x0000
#define INIT_USERS  0x0001
#define INIT_REMOTE 0x0002
#define INIT_SERVER 0x0004
#define INIT_NEXSRV 0x0008

class ThreadArgs;

class Peer : public Configuration {
public:
  Peer(const string& conf);
  ~Peer();

  void run(void);
  void stop(void);
protected:
  void    update_userlist(const string& usrnam, const string& usrnfo);
  bool    get_userinfo(size_t ix, int lev, char* buf, size_t len);
  size_t  get_usercount(void);

  void set_default(const string& usrnam);
  void set_fastopen(bool fo);
private:
  static void* start(void* args);
  static void* start_client(void* args);
  static void cleanup_client(void* args);

  void enter_loop(void);

  int stage_init(char* ptr, size_t len, User* user, int soc, pthread_t id);
  int stage_auth(char* ptr, size_t len, User* user, int soc, pthread_t id);
  int stage_requ(char* ptr, size_t len, User* user, int soc, Socks*& target, const string& from_ipp, pthread_t id);
  int stage_strm_connect(User* user, int soc, Socks* target, pthread_t id);
  int stage_strm_bind(User* user, int soc, Socks* target);
  int stage_strm_udp(User* user, int soc, Socks* target);

  /* Diagram:
   * [A:target] <---> [B:server] <---> [C:client]
   * */
  Server remote_srv; // listening on port waiting for target host incoming connection (endpoint:A -> B)
  Server server_srv; // listening for incoming client (endpoint:C -> B)
  Client nexsrv_cli;

  User* defuser;

  map<string, User*> user_map;

  bool    okay;
  double  timeout;
  short   status;

  string  remote_ipp;
  string  server_ipp;
  string  nexsrv_ipp;

  Thread  threads;
  Buffer  reply;
};

#endif	/* _PEER_H_ */
