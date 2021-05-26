/* ***
 * @ $proxy.c
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
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <iostream>

#include "proxy.h"
#include "utils.h"
#include "xstring.h"

using namespace std;
using namespace utils;

struct ThreadArgs {
  int     soc;
  Proxy*  pxy;
  bool    done;
  Client* srv;
  string  hostip;
  int     port;
  pthread_t id;
};

Proxy::Proxy(const string& conf) : Configuration(conf.c_str()), user(NULL), okay(false), timeout(20.), server_port(0)
{
  char status = 0;
  bool  fo = false;
  
  string local_ipp  = get("master", "local");
  string server_ipp = get("master", "server");
  string usrnam     = get("default", "user");
  string str        = get("master", "fastopen");

  if (! str.empty()) {
    fo = atoi(str.c_str()) > 0;
  }

  if (! usrnam.empty()) {
    string usrnfo = get("user", usrnam);
    if (! usrnfo.empty()) {
      set_userinfo(usrnam, usrnfo); // set username / userinfo
      status |= INIT_USER;
    }
  }

  string hostip; int port;

  if (! server_ipp.empty() && gethostnameport(server_ipp, hostip, port)) {
    xstring str = "Proxy:Proxy:remote server initialized on [";
    str.printf("%s:%u]", hostip.c_str(), (unsigned int) port);
    logutil::info(str);
    server_ip = hostip;
    server_port = port;
    status |= INIT_SERVER;
  } else {
    xstring str = "Proxy:Proxy:cannot connect to remote server [";
    str.printf("%s:%u]", hostip.c_str(), (unsigned int) port);
    logutil::erro(str);
  }

  if (! local_ipp.empty() && gethostnameport(local_ipp, hostip, port)) {
    if (local.setup(hostip, port)) {
      int num = 1;
      local.setsockopt(SOL_SOCKET, SO_REUSEADDR, &num, sizeof(num));
      local.setsockopt(SOL_SOCKET, SO_REUSEPORT, &num, sizeof(num));
      local.setsockopt(SOL_SOCKET, MSG_NOSIGNAL, &num, sizeof(num));
      status |= INIT_LOCAL;
    }
  }

  if (fo && status & INIT_LOCAL) {
    set_fastopen(fo);
  }

  if ((status & 0x07) == 0x07) {
    okay = true;
  }
}

Proxy::~Proxy()
{
  stop();

  if (user != NULL) delete user;
}

void Proxy::run(void)
{
  if (okay && user != NULL) {
    int soc = local.socket();

    fd_set fds;

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
          if (tags != NULL && tags->done) {
            threads.join_thread(pa.first);
            if (tags->srv != NULL)
              delete tags->srv;
            delete tags;
          }
        }
      }
      
      if (select(soc + 1, &fds, NULL, NULL, NULL) != -1) {
        char hostip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];
        int port, fd = local.accept(hostip, port);;

        if (fd < 0) continue;

        xstring msg = "Proxy:run:connection from [";
        msg += hostip;
        msg.printf(":%u]", (unsigned int) port);
        logutil::info(msg);

        ThreadArgs* tags = new ThreadArgs();

        if (tags != NULL) {
          tags->soc = fd;
          tags->pxy = this;
          tags->done = false;
          tags->srv = new Client();
          tags->hostip = hostip;
          tags->port = port;
          threads.create_thread(Proxy::start_client, tags, tags->id);
        }
      }
    }
  }

}

void Proxy::stop(void)
{
  if (okay) {
    threads.kill_threads();
    local.close();
    logutil::info("Proxy:stop:stop proxy");
  }
}

void Proxy::set_userinfo(const string& usrnam, const string& usrnfo)
{
  if (user != NULL) delete user;
  user = new User(usrnam, usrnfo);
}

void Proxy::set_fastopen(bool fo)
{
#ifndef __APPLE__
  int opt = 1;
#else
  int opt = 5;
#endif

  if (local.setsockopt(IPPROTO_TCP, TCP_FASTOPEN, &opt, sizeof(opt)) == -1) {
    logutil::erro("Proxy:set_fastopen");
  }
}

void Proxy::cleanup_client(void* args)
{
  if (args == NULL) return;

  ThreadArgs* tags = (ThreadArgs*) args;

  if (tags->pxy != NULL && tags->soc > 0) {
    tags->pxy->local.close(tags->soc);
  }

  tags->done = true;
}

void* Proxy::start_client(void* args)
{
  if (args == NULL) return args;

  ThreadArgs* tags = (ThreadArgs*) args;

  if (tags->srv == NULL) return args;
  
  pthread_cleanup_push(Proxy::cleanup_client, args);

  if (tags->srv->connect(tags->pxy->server_ip, tags->pxy->server_port)) {
    User* user = new User(*tags->pxy->user);
    if (user != NULL) {
      tags->pxy->local_read(user, tags->srv, tags->soc, tags->srv->socket(), tags->id);
      delete user;
    }
  } else {
    xstring msg = "Proxy:start_client:cannot connect to [";
    msg.printf("%s:%u]", tags->hostip.c_str(), tags->port);
    logutil::erro(msg);
  }

  tags->done = true;
  tags->pxy->local.close(tags->soc);
  tags->srv->close();
  
  pthread_cleanup_pop(0);

  return args;
}

void Proxy::local_read(User* user, Client* srv, int fd1, int fd2, pthread_t id) // fd1 - local client, fd2 - remote server
{
  fd_set fdsc, fds;

  FD_ZERO(&fdsc);
  FD_SET(fd1, &fdsc);
  FD_SET(fd2, &fdsc);

  Buffer res, dat;

  bool okay = true;

  while (okay) {
    res.reset();
    dat.reset();

    memcpy(&fds, &fdsc, sizeof(fds));

    size_t to = (size_t) timeout;
    struct timeval tm = {
      .tv_sec = (time_t) to,
      .tv_usec = (time_t) (timeout - to) * 1000
    };

    switch (select(MAX(fd1, fd2) + 1, &fds, NULL, NULL, &tm)) {
      case 0: // timeout
        okay = false;
        break;
      case -1: // error
        if (errno == EINTR) continue;
        break;
    }

    if (! okay) break;

    int     in  = FD_ISSET(fd1, &fds) ? fd1 : fd2;
    int     out = in == fd1 ? fd2 : fd1;
    char*   ptr = (char*) res.ptr();
    ssize_t len;

    if (in == fd2) {
      len = utils::recvall(res, in);
    } else {
      len = local.recv(in, res.ptr(), res.capacity());
    }

    if (len <= 0) break;

    if (user != NULL) {
      if (in == fd2) { // if data received from remote server
        if (user->decode(dat, ptr, len, id)) {
          ptr = (char*) dat.ptr(); len = dat.size();
        }
      }
      if (out == fd2) { // send data to remote server
        if (user->encode(dat, ptr, len, id)) {
          ptr = (char*) dat.ptr(); len = dat.size();
        }
      }
    }

    if (out == fd2) {
      utils::sendall(ptr, len, out);
    } else {
      local.send(out, ptr, len);
    }

    /*ssize_t sent = 0;

    while (sent < len) {
      to = local.send(out, ptr + sent, len - sent);
      if (to <= 0) return;
      sent += to;
    }*/
  }
}

/*end*/
