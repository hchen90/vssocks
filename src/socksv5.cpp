/* ***
 * @ $socksv5.cpp
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
#include "buffer.h"
#include "socksv5.h"
#include "utils.h"

ssize_t std::send_status(User* user, int soc, char status, pthread_t id)
{
  char buf[10] = {
    SOCKSV5_VER, status, 0, SOCKSV5_ATYP_IPV4, 0,0,0,0, 0,0
  };

  Buffer res;
  char* ptr = buf;
  size_t len = sizeof(buf);

  if (user != NULL) {
    if (user->encode(res, ptr, len, id)) {
      ptr = (char*) res.ptr();
      len = res.size();
    }
  }

  return utils::sendall(ptr, len, soc);
}

/*end*/
