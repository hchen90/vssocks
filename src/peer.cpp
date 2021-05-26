/* ***
 * @ $peer.cpp
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
#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>

#include <cstring>
#include <iostream>

#include "config.h"
#include "utils.h"
#include "peer.h"
#include "xstring.h"

using namespace utils;

struct ThreadArgs {
  int     which;
  Peer*   peer;
  int     soc;
  bool    done;
  struct  {
    struct sockaddr addr;
    socklen_t addr_len;
  } cli;
  Socks*  target;
  User*   user;
  pthread_t id;
};

Peer::Peer(const string& conf) : Configuration(conf.c_str()), defuser(NULL), okay(false), timeout(20.), status(INIT_UNDEF), mutex_okay(-1)
{
  mutex_okay = pthread_mutex_init(&mutex, NULL);

  bool fo = false; string str;

  if (! (str = get("master", "fastopen")).empty()) {
    fo = atoi(str.c_str()) > 0;
  }

  if (! (str = get("master", "timeout")).empty()) {
    timeout = atof(str.c_str());
  }

  string sip = get("master", "server");
  string rip = get("master", "remote");
  string nip = get("master", "next");

  string def_user = get("default", "user");

  if (! sip.empty() && ! def_user.empty()) {
    // initialize all users
    map<string, string> usrs = get("user");
    map<string, string>::iterator it, end;

    for (it = usrs.begin(), end = usrs.end(); it != end; it++) {
      // update user list
      update_userlist(it->first, it->second);
    }

    set_default(def_user);
    status |= INIT_USERS;
  }
  
  string  hostip;
  int     port;

  // server endpoint
  if (status & INIT_USERS && ! sip.empty() && gethostnameport(sip, hostip, port)) {
    if (server_srv.setup(hostip, port) && assem_socksv5reply(server_srv, hostip, port, server_rep)) {
      int num = 1;
      server_srv.setsockopt(SOL_SOCKET, SO_REUSEADDR, &num, sizeof(num));
      server_srv.setsockopt(SOL_SOCKET, SO_REUSEPORT, &num, sizeof(num));
      server_srv.setsockopt(SOL_SOCKET, MSG_NOSIGNAL, &num, sizeof(num));
      xstring msg = "Peer:Peer:server initialized on [";
      msg.printf("%s:%u]", hostip.c_str(), port);
      logutil::info(msg);
      server_ipp.first = hostip;
      server_ipp.second = port;
      set_fastopen(fo);
      status |= INIT_SERVER;
    }
  }

  // remote server (local) for binding socket
  if (status & INIT_SERVER && ! rip.empty() && gethostnameport(rip, hostip, port)) {
    xstring msg = "Peer:Peer:remote server address set to [";
    msg.printf("%s:%u]", hostip.c_str(), port);
    logutil::info(msg);
    remote_ipp.first = hostip;
    remote_ipp.second = port;
    status |= INIT_REMOTE;
  }

  // next server for quering request
  if (status & INIT_SERVER && ! nip.empty() && gethostnameport(nip, hostip, port)) {
    if (nexsrv_cli.connect(hostip, port)) {
      xstring msg = "Peer:Peer:next server initialized on [";
      msg.printf("%s:%u]", hostip.c_str(), port);
      logutil::info(msg);
      nexsrv_ipp.first = hostip;
      nexsrv_ipp.second = port;
      status |= INIT_NEXSRV;
    }
  }

  if (status & INIT_USERS && status & INIT_SERVER) {
    logutil::info("Peer:Peer:peer initialized");
    okay = true;
  }
}

Peer::~Peer()
{
  if (okay) {
    stop();

    if (user_map.size() > 0) { // free users list
      map<string, User*>::iterator it, end;

      for (it = user_map.begin(), end = user_map.end(); it != end; it++) {
        delete it->second;
      }
    }
  }

  if (! mutex_okay) {
    pthread_mutex_destroy(&mutex);
  }
}

void Peer::run(void)
{
  if (! okay) return;

  ThreadArgs args = {
    .which = 0,
    .peer = this
  };

  // start thread for server_srv
  if (threads.create_thread(Peer::start, &args, args.id)) {
    logutil::erro("Peer:run:create_thread");
  }

  enter_loop();

  // wait to child threads
  threads.kill_threads();
}

void Peer::stop(void)
{
  if (! okay) return;

  threads.kill_threads();

  server_srv.close();
  //remote_srv.close();
  nexsrv_cli.close();

  logutil::info("Peer:stop:stop peer");
}

void Peer::update_userlist(const string& usrnam, const string& usrnfo)
{
  if (! usrnam.empty() && ! usrnfo.empty()) {
    User* usr = new User(usrnam, usrnfo);

    if (usr != NULL) { // insert user to list
      user_map.insert(make_pair(usrnam, usr));
    }
  }
}

bool Peer::get_userinfo(size_t ix, int lev, char* buf, size_t len)
{
  map<string, User*>::iterator it, end; size_t i;

  for (i = 0, it = user_map.begin(), end = user_map.end(); it != end && i <= ix; it++, i++) {
    if (i == ix) {
      return it->second->get_info(lev, buf, len);
    }
  }

  return false;
}

size_t Peer::get_usercount(void)
{
  return user_map.size();
}

void Peer::set_default(const string& usrnam)
{
  map<string, User*>::iterator it = user_map.find(usrnam);

  if (it != user_map.end()) {
    defuser = it->second; // default user
  }
}

void Peer::set_fastopen(bool fo)
{
  if (fo) {
#ifndef __APPLE__
    int opt = 1;
#else
    int opt = 5;
#endif
    
    if (status & INIT_SERVER && server_srv.setsockopt(IPPROTO_TCP, TCP_FASTOPEN, &opt, sizeof(opt)) == -1) {
      logutil::warn("Peer:server_srv.set_fastopen()");
    }
    
    /*if (status & INIT_REMOTE && remote_srv.setsockopt(IPPROTO_TCP, TCP_FASTOPEN, &opt, sizeof(opt)) == -1) {
      logutil::warn("Peer:remote_srv.set_fastopen()");
    }*/
  }
}

// thread routine for server_srv / recver_srv
void* Peer::start(void* args)
{
  ThreadArgs* pags = (ThreadArgs*) args;

  if (pags == NULL) return args;

  if (pags->which == 0) { // server_srv
#ifdef DEBUG
    logutil::info("server_srv started");
#endif
    Thread threads;
    fd_set fds;
    int soc = pags->peer->server_srv.socket();

    FD_ZERO(&fds);
    FD_SET(soc, &fds);

    while (true) {
      size_t i, count = threads.threads_count();

      for (i = 0; i < count; i++) {
        pair<pthread_t, void*> pa = threads.thread_get(i);
	bool first_b = false;
#ifdef __linux__
	first_b = pa.first > 0;
#else
	first_b = pa.first != NULL;
#endif
        if (first_b && pa.second != NULL) {
          ThreadArgs* tags = (ThreadArgs*) pa.second;
          if (tags->done) {
            threads.join_thread(pa.first);
            if (tags->target != NULL)
              delete tags->target;
            if (tags->user != NULL)
              delete tags->user;
            delete tags;
          }
        }
      }

      if (select(soc + 1, &fds, NULL, NULL, NULL) != -1) {
        ThreadArgs* tags = new ThreadArgs();

        if (tags != NULL) {
          tags->which = pags->which;
          tags->peer = pags->peer;
          tags->cli.addr_len = sizeof(tags->cli.addr);
          tags->soc = pags->peer->server_srv.accept(&tags->cli.addr, &tags->cli.addr_len); // accept incomings
          if (tags->soc == -1) continue;
          else {
            struct linger lgr = { .l_onoff = 1, .l_linger = 5 };
            tags->peer->server_srv.setsockopt(tags->soc, SOL_SOCKET, SO_LINGER, &lgr, sizeof(lgr));
          }
          tags->done = false;
          tags->target = NULL;
          tags->user = new User(*tags->peer->defuser);
          threads.create_thread(Peer::start_client, tags, tags->id);
        }
      }
    }
  }

  return args;
}

void* Peer::start_client(void* args)
{
  ThreadArgs* tags = (ThreadArgs*) args;

  if (tags == NULL) return args;

  pthread_cleanup_push(Peer::cleanup_client, &tags);

  char    ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 1];
  int     port;
  xstring from_ipp;

  if (! tags->peer->server_srv.resolve(&tags->cli.addr, ip, port)) {
    ip[sizeof(ip) - 1] = 0;
    from_ipp = ip;
    from_ipp.printf(":%u", (unsigned int) port);
  } else {
    logutil::erro("Peer:start_client:resolve()");
    return args;
  }

  User*   user = tags->user;
  Buffer  res, txt;
  ssize_t len;
  char*   ptr;
  short   stage = STAGE_INIT;
  int     ret;

  if (! res.alloc(BUFF_SIZE * 3)) {
    logutil::erro("Peer:start_client:alloc()");
    return args;
  }

  while ((len = utils::recvall(res, tags->soc)) > 0) {
    if (user != NULL && user->decode(txt, res.ptr(), len, tags->id)) {
      ptr = (char*) txt.ptr(); len = txt.size();
    } else {
      ptr = (char*) res.ptr();
    }
    switch (stage) {
      case STAGE_INIT:
        ret = tags->peer->stage_init(ptr, len, user, tags->soc, tags->id);
        if (ret == SOCKSV5_METHOD_NOAUTH) stage = STAGE_REQU;
        else if (ret == SOCKSV5_METHOD_USRPASS) stage = STAGE_AUTH;
        else stage = STAGE_FINISH; // only support NOAUTH and USRPASS
        break;
      case STAGE_AUTH:
        ret = tags->peer->stage_auth(ptr, len, user, tags->soc, tags->id); // authentication
        if (ret == SOCKSV5_SUCCESS) stage = STAGE_REQU;
        stage = STAGE_FINISH;
        break;
      case STAGE_REQU:
        ret = tags->peer->stage_requ(ptr, len, user, tags->soc, tags->target, from_ipp, tags->id); // request
        if (ret == SOCKSV5_CMD_CONNECT) {
          tags->peer->stage_strm_connect(user, tags->soc, tags->target, tags->id);
        } else if (ret == SOCKSV5_CMD_BIND) {
          tags->peer->stage_strm_bind(user, tags->soc, tags->target, tags->id);
        } else if (ret == SOCKSV5_CMD_UDP) {
          tags->peer->stage_strm_udp(user, tags->soc, tags->target, tags->id);
        }
        stage = STAGE_FINISH;
        break;
    }
    if (stage == STAGE_FINISH) break;
  }

  if (tags->target != NULL)
    tags->target->close(); // close remote target host's connection
  tags->peer->server_srv.close(tags->soc); // close client's connection
  tags->done = true;

  pthread_cleanup_pop(0);

  return args;
}

void Peer::cleanup_client(void* args)
{
  ThreadArgs* tags = (ThreadArgs*) args;
  if (tags != NULL) {
    if (tags->target != NULL) tags->target->close();
    tags->peer->server_srv.close(tags->soc); tags->done = true;
  }
}

int Peer::stage_init(char* ptr, size_t len, User* user, int soc, pthread_t id)
{
  /* format: [VERSION][LAUTH][AUTH] ~ [1][1][1-255] */
  int     rev = 0;

  if (len < 2) return -1;

  if (ptr[0] == SOCKSV5_VER) {
    short i, nms = ptr[1];
    char buf[2] = { SOCKSV5_VER };
    /* format: [VERSION][AUTH] */

    for (i = 0; i < nms && (size_t) (i + 2) < len; i++) {
      if (*(char*) (ptr + 2 + i) == SOCKSV5_METHOD_USRPASS) { // use username / password
        rev = buf[1] = SOCKSV5_METHOD_USRPASS;
        break;
      } else if (*(char*) (ptr + 2 + i) == SOCKSV5_METHOD_NOAUTH) { // no authentication
        rev = buf[1] = SOCKSV5_METHOD_NOAUTH;
        break;
      }
    }

    Buffer  xtx;
    char*   p = buf;
    size_t  l = sizeof(buf);

    if (user != NULL && user->encode(xtx, p, l, id)) {
      p = (char*) xtx.ptr(); l = xtx.size();
    }
    
    utils::sendall(p, l, soc);
  }

  return rev;
}

int Peer::stage_auth(char* ptr, size_t len, User* user, int soc, pthread_t id)
{
  /* format:
   * [VERSION][LNAME][NAME][LPASSWD][PASSWD] ~ [1][1][1-255][1][1-255]
   * */
  int     rev = SOCKSV5_ERROR;
  Buffer  res;

  if (len < 2) return SOCKSV5_ERROR;

  // only verify username / password while this socket not in in-core saved list
  if (ptr[0] == SOCKSV5_AUTHVER) {
    short nml = ptr[1];

    if ((size_t) (nml + 1) > len) return SOCKSV5_ERROR;
    
    short pwl = ptr[2 + nml];
    
    if ((size_t) (pwl + 2) > len) return SOCKSV5_ERROR;

    string nm = string(ptr + 2, nml);
    string pw = string(ptr + 3 + nml, pwl);

    map<string, User*>::iterator it = user_map.find(nm);

    if (it != user_map.end()) {
      bool matched = true;
      string upw = it->second->get_password(); // may be empty when using RSA encryption
      if (! upw.empty() && upw != pw) matched = false;
      if (matched) {
        rev = SOCKSV5_SUCCESS;
      } else {
        logutil::warn("Peer:server_read_stage_auth:authentication failed");
      }
    } else {
      logutil::warn("Peer:server_read_stage_auth:user not found");
    }

    char buf[2] = { SOCKSV5_AUTHVER };

    if (rev == SOCKSV5_SUCCESS) {
      buf[1] = SOCKSV5_SUCCESS;
    } else {
      buf[1] = SOCKSV5_ERROR;
    }

    Buffer  xtx;
    char*   p = buf;
    size_t  l = sizeof(buf);

    if (user != NULL && user->encode(xtx, p, l, id)) {
      p = (char*) xtx.ptr(); l = xtx.size();
    }

    utils::sendall(p, l, soc);
  }

  return rev;
}

int Peer::stage_requ(char* ptr, size_t len, User* user, int soc, Socks*& target, const string& from_ipp, pthread_t id)
{
  /* format:
   * [VERSION][CMD][RSV][ATYP][BIND.ADDR][BIND.PORT] ~ [1][1][1][1][1-256][2]
   * */
  int     rev = SOCKSV5_ERROR;
  Buffer  res;
  
  if (ptr[0] == SOCKSV5_VER && ! ptr[2]) {
    char& cmd = ptr[1];
    char& aty = ptr[3];
    char ip[16];
    string hostip;
    int port = 0;

    if (aty == SOCKSV5_ATYP_IPV4) {
      memcpy(ip, ptr + 4, 4);
      port = ntohs(*(short*) (ptr + 8));
    } else if (aty == SOCKSV5_ATYP_IPV6) {
      memcpy(ip, ptr + 4, 16);
      port = ntohs(*(short*) (ptr + 20));
    } else if (aty == SOCKSV5_ATYP_DOMAINNAME) {
      hostip = string(ptr + 5, ptr[4]);
      port = ntohs(*(short*) (ptr + 5 + ptr[4]));
    }

    char ips[MAX(INET6_ADDRSTRLEN, INET_ADDRSTRLEN) + 1];

    if (aty == SOCKSV5_ATYP_IPV4 || aty == SOCKSV5_ATYP_IPV6) {
      if (inet_ntop(aty == SOCKSV5_ATYP_IPV4 ? AF_INET : AF_INET6, ip, ips, sizeof(ips)) != NULL) {
        ips[(aty == SOCKSV5_ATYP_IPV4 ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN)] = 0;
        hostip = ips;
      }
    }

    /* hostip: target host IP address
     * port: target host port number
     * */
    xstring msg = "Peer:stage_requ";

    if (cmd == SOCKSV5_CMD_CONNECT || cmd == SOCKSV5_CMD_BIND) { // connect or bind
      Client* host = new Client();
      if (host != NULL) {
        if (host->connect(hostip, port)) {
          target = host; rev = cmd;
          if (cmd == SOCKSV5_CMD_CONNECT)
            send_status(user, soc, SOCKSV5_REP_SUCCESS, id, server_rep);
          msg.printf(":[%s] connected to [%s:%u]", from_ipp.c_str(), hostip.c_str(), (unsigned int) port);
        } else {
          delete host;
          if (cmd == SOCKSV5_CMD_CONNECT)
            send_status(user, soc, SOCKSV5_REP_ERROR, id, server_rep);
          msg.printf(":[%s] cannot connect to [%s:%u]", from_ipp.c_str(), hostip.c_str(), (unsigned int) port);
        }
      }
    } else if (cmd == SOCKSV5_CMD_UDP) { // UDP associate
      rev = cmd;
      send_status(user, soc, SOCKSV5_REP_SUCCESS, id, server_rep);
    } else { // unknown command
      rev = SOCKSV5_ERROR;
    }

    if (rev != SOCKSV5_ERROR) {
      logutil::info(msg);
    }
  }

  return rev; 
}

// soc - user's client side, target - remote target host
int Peer::stage_strm_connect(User* user, int soc, Socks* target, pthread_t id)
{
  Buffer  res, dat;
  int     fd = target->socket();

  fd_set  fdsc, fds;

  FD_ZERO(&fdsc);
  FD_SET(fd, &fdsc);
  FD_SET(soc, &fdsc);

  while (true) {
    res.reset();
    dat.reset();

    memcpy(&fds, &fdsc, sizeof(fds));

    size_t to = (size_t) timeout;
    struct timeval tmv = {
      .tv_sec   = (time_t) to,
      .tv_usec  = 0 //(time_t) (timeout - to) * 1000
    };

    switch (select(MAX(fd, soc) + 1, &fds, NULL, NULL, &tmv)) {
      case 0: // timeout
        send_status(user, soc, SOCKSV5_REP_TTLEXPI, id, server_rep);
        return 1;
      case -1: // error
        if (errno == EINTR) continue;
        return 2;
    }

    int     in  = FD_ISSET(fd, &fds) ? fd : soc;
    int     out = in == fd ? soc : fd;
    ssize_t len;

    if (in == soc) {
      len = utils::recvall(res, in);
    } else {
      len = server_srv.recv(in, res.ptr(), res.capacity());
    }

    if (len <= 0) return 2;

    char*   ptr = (char*) res.ptr();

    if (user != NULL) {
      if (in == soc) { // from user's client
        if (user->decode(dat, ptr, len, id)) {
          ptr = (char*) dat.ptr(); len = dat.size();
        }
      } else if (out == soc) { // data need to encrypted before send it out
        if (user->encode(dat, ptr, len, id)) {
          ptr = (char*) dat.ptr(); len = dat.size();
        }
      }
    }

    if (len <= 0) return 2;

    if (out == soc) {
      if (utils::sendall(ptr, len, out) <= 0) return 2;
    } else {
      if (server_srv.send(out, ptr, len) <= 0) return 2;
    }
  }
  
  return 0;
}

int Peer::stage_strm_bind(User* user, int soc, Socks* target, pthread_t id)
{
  Server remote;
  Client* host = (Client*) target;

  int port = get_port(remote_ipp.second);

  if (port < 0) return -1;

  if (remote.setup(remote_ipp.first, port)) {
    Buffer remote_rep, res, dat;
    ssize_t size = 0;

    // first reply (`port' is awaiting for incoming connection)
    if (assem_socksv5reply(server_srv, remote_ipp.first, port, remote_rep)) {
      size = send_status(user, soc, SOCKSV5_REP_SUCCESS, id, remote_rep);
    }

    if (size > 0) {
      fd_set fds;
      bool okay = true;
      int fd = remote.socket();

      FD_ZERO(&fds);
      FD_SET(fd, &fds);

      struct timeval tmv = {
        .tv_sec = (time_t) timeout,
        .tv_usec = 0
      };

      while (okay) {
        switch (select(fd + 1, &fds, NULL, NULL, &tmv)) {
          case 0: // timeout
          case -1: // error
            okay = false;
            break;
        }

        if (! okay) break;

        int cd = remote.accept(NULL, 0);
        if (cd > 0) {
          // second reply (`hostip' and `port' are the address of target host on below)
          string hostip; int prt;
          if (host->gethostaddr(hostip, prt) && assem_socksv5reply(server_srv, hostip, prt, remote_rep)) {
            okay = send_status(user, soc, SOCKSV5_REP_SUCCESS, id, remote_rep) > 0;
          } else {
            send_status(user, soc, SOCKSV5_REP_ERROR, id, remote_rep); okay = false;
          }

          fd_set rfds1, rfds2;
          int hs = host->socket();

          FD_ZERO(&rfds1);
          FD_SET(cd, &rfds1); // SOCKS5 client side
          FD_SET(hs, &rfds1); // target host side

          while (okay) {
            memcpy(&rfds2, &rfds1, sizeof(rfds1));

            switch (select(MAX(cd, hs) + 1, &rfds2, NULL, NULL, &tmv)) {
              case 0: // timeout
              case -1: // error
                okay = false;
                break;
            }

            if (! okay) break;

            int in = FD_ISSET(cd, &rfds2) ? cd : hs;
            int out = in == cd ? hs : cd;
            ssize_t len;

            if (in == cd) {
              len = utils::recvall(res, in);
            } else {
              len = server_srv.recv(in, res.ptr(), res.capacity());
            }

            if (len <= 0) {
              okay = false;
              break;
            }
            
            char* ptr = (char*) res.ptr();

            if (user != NULL) {
              if (in == cd) {
                if (user->decode(dat, ptr, len, id)) {
                  ptr = (char*) dat.ptr(); len = dat.size();
                }
              } else if (out == cd) {
                if (user->encode(dat, ptr, len, id)) {
                }
              }
            }

            if (len <= 0) {
              okay = false;
              break;
            }

            if (out == cd) {
              if (utils::sendall(ptr, len, out) <= 0) okay = false;
            } else {
              if (server_srv.send(out, ptr, len) <= 0) okay = false;
            }
          }
        }
      }
    }
  }

  del_port(port);
  return 0;
}

int Peer::stage_strm_udp(User* user, int soc, Socks* target, pthread_t id)
{
  return 0;
}


/////////////////////////////

int Peer::get_port(int port)
{
  pthread_mutex_lock(&mutex);

  while (port > 0) {
    if (port_set.find(port) == port_set.end()) { // if not existed
      port_set.insert(port);
      return port;
    } else {
      port++;
    }
  }

  pthread_mutex_unlock(&mutex);

  return 0;
}

void Peer::del_port(int port)
{
  pthread_mutex_lock(&mutex);
  port_set.erase(port);
  pthread_mutex_unlock(&mutex);
}

/////////////////////////////
void Peer::enter_loop(void)
{
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(0, &fds);

  while (true) {
    if (select(1, &fds, NULL, NULL, NULL) != -1) {
      string command;
      cin >> command;

      if (! command.empty()) {
        if (command == "quit" || command == "exit") {
          // exit loop
          return;
        } else if (command == "list" || command == "status") {
          // list all users or their status
          char buf[BUFF_SIZE];
          size_t i, count = get_usercount();

          for (i = 0; i < count; i++) {
            if (get_userinfo(i, command == "list" ? 0 : 1, buf, sizeof(buf))) {
              cout << "\r]  " << buf << endl;
            }
          }
        } else if (command == "version") {
          // package version
          cout << "\r]  " << PACKAGE_NAME << " version " << PACKAGE_VERSION << endl;
        } else {
          cout << "\r] Avaliable command: list status quit exit version" << endl;
        }
      }

      cout << "\r] ";
      cout.flush();
    }
  }
}

/*end*/
