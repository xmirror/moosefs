/*
   Copyright Jakub Kruszona-Zawadzki, Core Technology Sp. z o.o.

   This file is part of MooseFS.

   READ THIS BEFORE INSTALLING THE SOFTWARE. BY INSTALLING,
   ACTIVATING OR USING THE SOFTWARE, YOU ARE AGREEING TO BE BOUND BY
   THE TERMS AND CONDITIONS OF MooseFS LICENSE AGREEMENT FOR
   VERSION 1.7 AND HIGHER IN A SEPARATE FILE. THIS SOFTWARE IS LICENSED AS
   THE PROPRIETARY SOFTWARE. YOU NOT ACQUIRE
   ANY OWNERSHIP RIGHT, TITLE OR INTEREST IN OR TO ANY INTELLECTUAL
   PROPERTY OR OTHER PROPRIETARY RIGHTS.
 */

#ifndef _SLOGGER_H_
#define _SLOGGER_H_

#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>

#include "strerr.h"

#define mfs_syslog(priority,msg) {\
	syslog((priority),"%s",(msg)); \
	fprintf(stderr,"%s\n",(msg)); \
}

#define mfs_arg_syslog(priority,format, ...) {\
	syslog((priority),(format), __VA_ARGS__); \
	fprintf(stderr,format "\n", __VA_ARGS__); \
}

#define mfs_errlog(priority,msg) {\
	const char *_mfs_errstring = strerr(errno); \
	syslog((priority),"%s: %s", (msg) , _mfs_errstring); \
	fprintf(stderr,"%s: %s\n", (msg), _mfs_errstring); \
}

#define mfs_arg_errlog(priority,format, ...) {\
	const char *_mfs_errstring = strerr(errno); \
	syslog((priority),format ": %s", __VA_ARGS__ , _mfs_errstring); \
	fprintf(stderr,format ": %s\n", __VA_ARGS__ , _mfs_errstring); \
}

#define mfs_errlog_silent(priority,msg) syslog((priority),"%s: %s", msg, strerr(errno));
#define mfs_arg_errlog_silent(priority,format, ...) syslog((priority),format ": %s", __VA_ARGS__ , strerr(errno));

#endif
