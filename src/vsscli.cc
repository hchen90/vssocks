/* ***
 * @ $vsscli.cpp
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
#include <iostream>

#include <unistd.h>
#include <signal.h>

#include "config.h"
#include "proxy.h"
#include "utils.h"

using namespace std;

void usage(void)
{
  cout  << PACKAGE_NAME << " version " << PACKAGE_VERSION << " (client)" << endl \
        << "Copyright (C) 2018 Hsiang Chen" << endl \
        << "This program may be freely redistributed under the terms of the GNU General Public License" << endl \
        << endl \
        << "Usage: " << PACKAGE_NAME << " [options]..." << endl \
        << "Options:" << endl \
        << "  -h      Display this help" << endl \
        << "  -v      Display program version" << endl \
        << "  -c      Set configuration file" << endl \
        << "  -C      Colorize output" << endl \
        << endl \
        << "Report bugs to <" << PACKAGE_BUGREPORT << ">" << endl;
}

Proxy* cli = NULL;

void sighder(int)
{
  if (cli != NULL) {
    cli->stop();
    delete cli;
    cli = NULL;
    exit(0);
  }
}

int main(int argc, char* argv[])
{
  int opt;
  string cfg;

  while ((opt = getopt(argc, argv, "hvc:")) != -1) {
    switch (opt) {
      case 'v':
        cout << PACKAGE_NAME << " version " << PACKAGE_VERSION << endl;
        return 0;
      case 'c':
        cfg = optarg;
        break;
      case 'C':
        log::color(true);
        break;
      case 'h':
        usage();
        return 0;
      default:
        usage();
        return -1;
    }
  }

  if (cfg.empty()) {
    usage();
  } else {
    signal(SIGINT,  sighder);
    signal(SIGPIPE, SIG_IGN);

    if ((cli = new Proxy(cfg.c_str())) != NULL) {
      cli->run();
      delete cli;
    }
  }

  return 0;
}
/*end*/
