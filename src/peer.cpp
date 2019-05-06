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
};

Peer::Peer(const string& conf) : Configuration(conf.c_str()), defuser(NULL), okay(false), timeout(20.), status(INIT_UNDEF)
{
  bool fo = false; string str;

  if (! (str = get("master", "fastopen")).empty()) {
    fo = atoi(str.c_str()) > 0;
  }

  if (! (str = get("master", "timeout")).empty()) {
    timeout = atof(str.c_str());
  }

  // target_ipp = get(...) // for connecting
  remote_ipp = get("master", "remote"); // for binding
  server_ipp = get("master", "server");
  sender_ipp = get("master", "sender");
  recver_ipp = get("master", "recver");

  string def_user = get("default", "user");

  if (! server_ipp.empty() && ! def_user.empty()) {
    // initialize all users
    map<string, string> usrs = get("user");
    map<string, string>::iterator it, end;

    for (it = usrs.begin(), end = usrs.end(); it != end; it++) {
      // update user list
      update_userlist(it->first, it->second);
    }

    set_default(def_user);
    set_fastopen(fo);
    status |= INIT_USERS;
  }
  
  string hostip;
  int port;

  if (gethostnameport(server_ipp, hostip, port)) {
    // server endpoint
    if (server_srv.setup(hostip, port)) {
      int num = 1;
      server_srv.setsockopt(SOL_SOCKET, SO_REUSEADDR, &num, sizeof(num));
      server_srv.setsockopt(SOL_SOCKET, SO_REUSEPORT, &num, sizeof(num));
      server_srv.setsockopt(SOL_SOCKET, MSG_NOSIGNAL, &num, sizeof(num));
      okay = true;
      status |= INIT_SERVER;
    }
  }

  // sender client
  if (okay && ! sender_ipp.empty() && gethostnameport(sender_ipp, hostip, port)) {
    if (sender_cli.connect(hostip, port)) {
      xstring msg = "Peer:Peer:sender client started on [";
      msg.printf("%s:%u]", hostip.c_str(), (unsigned int) port);
      log::info(msg);
      status |= INIT_SENDER;
    }
  }

  // recver server endpoint
  if (okay && ! recver_ipp.empty() && gethostnameport(recver_ipp, hostip, port)) {
    if (recver_srv.setup(hostip, port)) {
      xstring msg = "Peer:Peer:recver server started on [";
      msg.printf("%s:%u]", hostip.c_str(), (unsigned int) port);
      log::info(msg);
      status |= INIT_RECVER;
      int num = 1;
      recver_srv.setsockopt(SOL_SOCKET, SO_REUSEADDR, &num, sizeof(num));
      recver_srv.setsockopt(SOL_SOCKET, SO_REUSEPORT, &num, sizeof(num));
      recver_srv.setsockopt(SOL_SOCKET, MSG_NOSIGNAL, &num, sizeof(num));
    }
  }

  // remote server (local) for binding socket
  if (okay && ! remote_ipp.empty() && gethostnameport(remote_ipp, hostip, port)) {
    if (remote_srv.setup(hostip, port)) {
      xstring msg = "Peer:Peer:remote server started on [";
      msg.printf("%s:%u]", hostip.c_str(), port);
      log::info(msg);
      status |= INIT_REMOTE;
      int num = 1;
      server_srv.setsockopt(SOL_SOCKET, SO_REUSEADDR, &num, sizeof(num));
      server_srv.setsockopt(SOL_SOCKET, SO_REUSEPORT, &num, sizeof(num));
      server_srv.setsockopt(SOL_SOCKET, MSG_NOSIGNAL, &num, sizeof(num));
    }
  }

  if (okay) {
    log::info("Peer:Peer:peer initialized");
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
}

void Peer::run(void)
{
  if (! okay) return;
  if (defuser == NULL) return;

  ThreadArgs args = {
    .which = 0,
    .peer = this
  };

  // start thread for server_srv
  if (threads.create_thread(Peer::start, &args)) {
    log::erro("Peer:run:create_thread(0)");
  }

  ThreadArgs argz = {
    .which = 1,
    .peer = this
  };

  // start thread for recver_srv
  if (threads.create_thread(Peer::start, &argz)) {
    log::erro("Peer:run:create_thread(1)");
  }

  enter_loop();

  // wait to child threads
  threads.kill_threads();
}

void Peer::stop(void)
{
  if (! okay) return;

  threads.kill_threads();

  if (status & INIT_SERVER) server_srv.close();
  if (status & INIT_REMOTE) remote_srv.close();
  if (status & INIT_RECVER) recver_srv.close();
  if (status & INIT_SENDER) sender_cli.close();

  log::info("Peer:stop:stop peer");
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
      log::warn("Peer:server_srv.set_fastopen()");
    }
    
    if (status & INIT_REMOTE && recver_srv.setsockopt(IPPROTO_TCP, TCP_FASTOPEN, &opt, sizeof(opt)) == -1) {
      log::warn("Peer:remote_srv.set_fastopen()");
    }

    if (status & INIT_RECVER && recver_srv.setsockopt(IPPROTO_TCP, TCP_FASTOPEN, &opt, sizeof(opt)) == -1) {
      log::warn("Peer:recver_srv.set_fastopen()");
    }
  }
}

// thread routine for server_srv / recver_srv
void* Peer::start(void* args)
{
  ThreadArgs* pags = (ThreadArgs*) args;

  if (pags == NULL) return args;

  if (pags->which == 0) { // server_srv
#ifdef DEBUG
    log::info("server_srv");
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
        if (pa.first > 0 && pa.second != NULL) {
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
          threads.create_thread(Peer::start_client, tags);
        }
      }
    }
  } else { // recver_srv
#ifdef DEBUG
    log::info("recver_srv");
#endif
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
    log::erro("Peer:start_client:resolve()");
    return args;
  }

  User*   user = tags->user;
  Buffer  res, txt;
  ssize_t len;
  char*   ptr;
  short   stage = STAGE_INIT;
  int     ret;

  if (! res.alloc(BUFF_SIZE * 3)) {
    log::erro("Peer:start_client:alloc()");
    return args;
  }

  while ((len = tags->peer->server_srv.recv(tags->soc, res.ptr(), res.capacity())) > 0) {
    if (user != NULL && user->decode(txt, res.ptr(), len)) {
      ptr = (char*) txt.ptr(); len = txt.size();
    } else {
      ptr = (char*) res.ptr();
    }
    switch (stage) {
      case STAGE_INIT:
        ret = tags->peer->stage_init(ptr, len, user, tags->soc);
        if (ret == SOCKSV5_METHOD_NOAUTH) stage = STAGE_REQU;
        else if (ret == SOCKSV5_METHOD_USRPASS) stage = STAGE_AUTH;
        else stage = STAGE_FINISH; // only support NOAUTH and USRPASS
        break;
      case STAGE_AUTH:
        ret = tags->peer->stage_auth(ptr, len, user, tags->soc); // authentication
        if (ret == SOCKSV5_SUCCESS) stage = STAGE_REQU;
        stage = STAGE_FINISH;
        break;
      case STAGE_REQU:
        ret = tags->peer->stage_requ(ptr, len, user, tags->soc, tags->target, from_ipp); // request
        if (ret == SOCKSV5_CMD_CONNECT) {
          tags->peer->stage_strm_connect(user, tags->soc, tags->target);
        } else if (ret == SOCKSV5_CMD_BIND) {
          tags->peer->stage_strm_bind(user, tags->soc, tags->target);
        } else if (ret == SOCKSV5_CMD_UDP) {
          tags->peer->stage_strm_udp(user, tags->soc, tags->target);
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

int Peer::stage_init(char* ptr, size_t len, User* user, int soc)
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

    if (user != NULL && user->encode(xtx, p, l)) {
      p = (char*) xtx.ptr(); l = xtx.size();
    }
    
    server_srv.send(soc, p, l);
  }

  return rev;
}

int Peer::stage_auth(char* ptr, size_t len, User* user, int soc)
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
        log::warn("Peer:server_read_stage_auth:authentication failed");
      }
    } else {
      log::warn("Peer:server_read_stage_auth:user not found");
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

    if (user != NULL && user->encode(xtx, p, l)) {
      p = (char*) xtx.ptr(); l = xtx.size();
    }

    server_srv.send(soc, p, l);
  }

  return rev;
}

int Peer::stage_requ(char* ptr, size_t len, User* user, int soc, Socks*& target, const string& from_ipp)
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
    if (cmd == SOCKSV5_CMD_CONNECT) { // connect
      xstring msg = "Peer:stage_requ";
      Client* cli = new Client();
      if (cli != NULL) {
        if (cli->connect(hostip, port)) {
          target = cli; rev = cmd;
          send_status(user, soc, SOCKSV5_REP_SUCCESS);
          msg.printf(":[%s] connected to [%s:%u]", from_ipp.c_str(), hostip.c_str(), (unsigned int) port);
        } else {
          delete cli;
          send_status(user, soc, SOCKSV5_REP_ERROR);
          msg.printf(":[%s] cannot connect to [%s:%u]", from_ipp.c_str(), hostip.c_str(), (unsigned int) port);
        }
      }
      log::info(msg);
    } else if (cmd == SOCKSV5_CMD_BIND) { // bind
      rev = cmd;
      target = new Server();
    } else if (cmd == SOCKSV5_CMD_UDP) { // UDP associate
      rev = cmd;
    } else { // unknown command
      rev = SOCKSV5_ERROR;
    }
  }

  return rev; 
}

// soc - user's client side, target - remote target host
int Peer::stage_strm_connect(User* user, int soc, Socks* target)
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
        send_status(user, soc, SOCKSV5_REP_TTLEXPI);
        return 1;
      case -1: // error
        if (errno == EINTR) continue;
        return 2;
    }

    int     in  = FD_ISSET(fd, &fds) ? fd : soc;
    int     out = in == fd ? soc : fd;
    ssize_t len = server_srv.recv(in, res.ptr(), res.capacity());
    char*   ptr = (char*) res.ptr();

    if (len <= 0) return 2;

    if (user != NULL) {
      if (in == soc) { // from user's client
        if (user->decode(dat, ptr, len)) {
          ptr = (char*) dat.ptr(); len = dat.size();
        }
      } else if (out == soc) { // data need to encrypted before send it out
        if (user->encode(dat, ptr, len)) {
          ptr = (char*) dat.ptr(); len = dat.size();
        }
      }
    }

    if (len <= 0) return 2;

    server_srv.send(out, ptr, len);
    /*
    ssize_t sent = 0;

    while (sent < len) {
      to = server_srv.send(out, ptr + sent, len - sent);
      if (to <= 0) break;
      sent += to;
    }*/

    // if (sent == 0) return 2;
  }
  
  return 0;
}

int Peer::stage_strm_bind(User* user, int soc, Socks* target)
{
  return 0;
}

int Peer::stage_strm_udp(User* user, int soc, Socks* target)
{
  return 0;
}

bool Peer::recver_read(int soc)
{
  //if (latest_clean) { // this is first time
  //  latest_clean = false;
  //}

  return false;
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
