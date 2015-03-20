/*
   Copyright Jakub Kruszona-Zawadzki, Core Technology Sp. z o.o.

   This file is part of MooseFS.

   READ THIS BEFORE INSTALLING THE SOFTWARE. BY INSTALLING,
   ACTIVATING OR USING THE SOFTWARE, YOU ARE AGREEING TO BE BOUND BY
   THE TERMS AND CONDITIONS OF MooseFS LICENSE AGREEMENT FOR
   VERSION 1.7 AND HIGHER IN A SEPARATE FILE. THIS SOFTWARE IS LICENSED AS
   THE PROPRIETARY SOFTWARE, NOT AS OPEN SOURCE ONE. YOU NOT ACQUIRE
   ANY OWNERSHIP RIGHT, TITLE OR INTEREST IN OR TO ANY INTELLECTUAL
   PROPERTY OR OTHER PROPRITARY RIGHTS.
 */

#ifndef _MASSERT_H_
#define _MASSERT_H_

#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>

#include "strerr.h"

#define massert(e,msg) ((e) ? (void)0 : (fprintf(stderr,"failed assertion '%s' : %s\n",#e,(msg)),syslog(LOG_ERR,"failed assertion '%s' : %s",#e,(msg)),abort()))
// #define passert(ptr) ((ptr!=NULL) ? (ptr!=(void*)(-1)) ? (void)0 :  : (fprintf(stderr,"out of memory: %s is NULL\n",#ptr),syslog(LOG_ERR,"out of memory: %s is NULL",#ptr),abort()))
#define sassert(e) ((e) ? (void)0 : (fprintf(stderr,"failed assertion '%s'\n",#e),syslog(LOG_ERR,"failed assertion '%s'",#e),abort()))
#define passert(ptr) if (ptr==NULL) { \
		fprintf(stderr,"out of memory: %s is NULL\n",#ptr); \
		syslog(LOG_ERR,"out of memory: %s is NULL",#ptr); \
		abort(); \
	} else if (ptr==((void*)(-1))) { \
		const char *_mfs_errorstring = strerr(errno); \
		syslog(LOG_ERR,"mmap error on %s, error: %s",#ptr,_mfs_errorstring); \
		fprintf(stderr,"mmap error on %s, error: %s\n",#ptr,_mfs_errorstring); \
		abort(); \
	}
#define eassert(e) if (!(e)) { \
		const char *_mfs_errorstring = strerr(errno); \
		syslog(LOG_ERR,"failed assertion '%s', error: %s",#e,_mfs_errorstring); \
		fprintf(stderr,"failed assertion '%s', error: %s\n",#e,_mfs_errorstring); \
		abort(); \
	}
#define zassert(e) if ((e)!=0) { \
		const char *_mfs_errorstring = strerr(errno); \
		syslog(LOG_ERR,"unexpected status, '%s' returned: %s",#e,_mfs_errorstring); \
		fprintf(stderr,"unexpected status, '%s' returned: %s\n",#e,_mfs_errorstring); \
		abort(); \
	}

#endif
