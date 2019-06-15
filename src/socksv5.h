/* $ @socksv5.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_SOCKSV5_H_
#define	_SOCKSV5_H_

#include <string>

#include "user.h"
#include "socks.h"

#define SOCKSV5_VER '\x05'
#define SOCKSV5_AUTHVER '\x01'

#define SOCKSV5_METHOD_NOAUTH '\x00'
#define SOCKSV5_METHOD_GSSAPI '\x01'
#define SOCKSV5_METHOD_USRPASS '\x02'
#define SOCKSV5_METHOD_UNACCEPT '\xff'

#define SOCKSV5_CMD_CONNECT '\x01'
#define SOCKSV5_CMD_BIND '\x02'
#define SOCKSV5_CMD_UDP '\x03'
#define SOCKSV5_CMD_QUERY '\x80'

#define SOCKSV5_ATYP_IPV4 '\x01'
#define SOCKSV5_ATYP_DOMAINNAME '\x03'
#define SOCKSV5_ATYP_IPV6 '\x04'

#define SOCKSV5_REP_ERROR '\xff'
#define SOCKSV5_REP_SUCCESS '\x00'
#define SOCKSV5_REP_SRVFAILURE '\x01'
#define SOCKSV5_REP_NOTALLOWED '\x02'
#define SOCKSV5_REP_NETUNREACH '\x03'
#define SOCKSV5_REP_HOSTUNREACH '\x04'
#define SOCKSV5_REP_REFUSED '\x05'
#define SOCKSV5_REP_TTLEXPI '\x06'
#define SOCKSV5_REP_CMDUNSUPPORTED '\x07'
#define SOCKSV5_REP_ADDRUNSUPPORTED '\x08'

#define SOCKSV5_SUCCESS SOCKSV5_REP_SUCCESS
#define SOCKSV5_ERROR SOCKSV5_REP_ERROR

#define STAGE_INIT 0
#define STAGE_AUTH 1
#define STAGE_REQU 2
#define STAGE_FINISH 3

namespace std {
  bool    assem_socksv5reply(Socks& srv, const string& hostip, int port, Buffer& rep);
  ssize_t send_status(User* user, int soc, char status, pthread_t id, const Buffer& rep);
};

#endif	/* _SOCKSV5_H_ */
