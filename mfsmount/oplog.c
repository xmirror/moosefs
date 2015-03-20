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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>

#include "oplog.h"

#define OPBUFFSIZE 0x1000000
#define LINELENG 1000
#define MAXHISTORYSIZE 0xF00000

typedef struct _fhentry {
	unsigned long fh;
	uint64_t readpos;
	uint32_t refcount;
//	uint8_t dotsent;
	struct _fhentry *next;
} fhentry;

static unsigned long nextfh=1;
static fhentry *fhhead=NULL;

static uint8_t opbuff[OPBUFFSIZE];
static uint64_t writepos=0;
static uint8_t waiting=0;
static pthread_mutex_t opbufflock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t nodata = PTHREAD_COND_INITIALIZER;

static time_t convts=0;
static struct tm convtm;
static pthread_mutex_t timelock = PTHREAD_MUTEX_INITIALIZER;
//static pthread_mutex_t bufflock = PTHREAD_MUTEX_INITIALIZER;

static inline void oplog_put(uint8_t *buff,uint32_t leng) {
	uint32_t bpos;
	if (leng>OPBUFFSIZE) {	// just in case
		buff+=leng-OPBUFFSIZE;
		leng=OPBUFFSIZE;
	}
	pthread_mutex_lock(&opbufflock);
	bpos = writepos%OPBUFFSIZE;
	writepos+=leng;
	if (bpos+leng>OPBUFFSIZE) {
		memcpy(opbuff+bpos,buff,OPBUFFSIZE-bpos);
		buff+=OPBUFFSIZE-bpos;
		leng-=OPBUFFSIZE-bpos;
		bpos = 0;
	}
	memcpy(opbuff+bpos,buff,leng);
	if (waiting) {
		pthread_cond_broadcast(&nodata);
		waiting=0;
	}
	pthread_mutex_unlock(&opbufflock);
}

void oplog_printf(const struct fuse_ctx *ctx,const char *format,...) {
	va_list ap;
	char buff[LINELENG];
	uint32_t leng;
	struct timeval tv;
	struct tm ltime;

	pthread_mutex_lock(&timelock);
	gettimeofday(&tv,NULL);
	if (convts/900!=tv.tv_sec/900) {
		convts=tv.tv_sec/900;
		convts*=900;
		localtime_r(&convts,&convtm);
	}
	ltime = convtm;
	leng = tv.tv_sec - convts;
	ltime.tm_sec += leng%60;
	ltime.tm_min += leng/60;
	pthread_mutex_unlock(&timelock);
//	pthread_mutex_lock(&bufflock);
	leng = snprintf(buff,LINELENG,"%02u.%02u %02u:%02u:%02u.%06u: uid:%u gid:%u pid:%u cmd:",ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec,(unsigned)(tv.tv_usec),(unsigned)(ctx->uid),(unsigned)(ctx->gid),(unsigned)(ctx->pid));
	if (leng<LINELENG) {
		va_start(ap,format);
		leng += vsnprintf(buff+leng,LINELENG-leng,format,ap);
		va_end(ap);
	}
	if (leng>=LINELENG) {
		leng=LINELENG-1;
	}
	buff[leng++]='\n';
	oplog_put((uint8_t*)buff,leng);
//	pthread_mutex_unlock(&bufflock);
}


unsigned long oplog_newhandle(int hflag) {
	fhentry *fhptr;
	uint32_t bpos;

	pthread_mutex_lock(&opbufflock);
	fhptr = malloc(sizeof(fhentry));
	fhptr->fh = nextfh++;
	fhptr->refcount = 1;
//	fhptr->dotsent = 0;
	if (hflag) {
		if (writepos<MAXHISTORYSIZE) {
			fhptr->readpos = 0;
		} else {
			fhptr->readpos = writepos - MAXHISTORYSIZE;
			bpos = fhptr->readpos%OPBUFFSIZE;
			while (fhptr->readpos < writepos) {
				if (opbuff[bpos]=='\n') {
					break;
				}
				bpos++;
				bpos%=OPBUFFSIZE;
				fhptr->readpos++;
			}
			if (fhptr->readpos<writepos) {
				fhptr->readpos++;
			}
		}
	} else {
		fhptr->readpos = writepos;
	}
	fhptr->next = fhhead;
	fhhead = fhptr;
	pthread_mutex_unlock(&opbufflock);
	return fhptr->fh;
}

void oplog_releasehandle(unsigned long fh) {
	fhentry **fhpptr,*fhptr;
	pthread_mutex_lock(&opbufflock);
	fhpptr = &fhhead;
	while ((fhptr = *fhpptr)) {
		if (fhptr->fh==fh) {
			fhptr->refcount--;
			if (fhptr->refcount==0) {
				*fhpptr = fhptr->next;
				free(fhptr);
			} else {
				fhpptr = &(fhptr->next);
			}
		} else {
			fhpptr = &(fhptr->next);
		}
	}
	pthread_mutex_unlock(&opbufflock);
}

void oplog_getdata(unsigned long fh,uint8_t **buff,uint32_t *leng,uint32_t maxleng) {
	fhentry *fhptr;
	uint32_t bpos;
	struct timeval tv;
	struct timespec ts;

	pthread_mutex_lock(&opbufflock);
	for (fhptr=fhhead ; fhptr && fhptr->fh != fh ; fhptr=fhptr->next ) {
	}
	if (fhptr==NULL) {
		*buff = NULL;
		*leng = 0;
		return;
	}
	fhptr->refcount++;
	while (fhptr->readpos>=writepos) {
		gettimeofday(&tv,NULL);
		ts.tv_sec = tv.tv_sec+1;
		ts.tv_nsec = tv.tv_usec*1000;
		waiting=1;
		if (pthread_cond_timedwait(&nodata,&opbufflock,&ts)==ETIMEDOUT) {
//			fhptr->dotsent=1;
			*buff = (uint8_t*)"#\n";
			*leng = 2;
			return;
		}
	}
//	if (fhptr->dotsent) {
//		fhptr->dotsent=0;
//		*buff = (uint8_t*)"\n";
//		*leng = 1;
//		return;
//	}
	bpos = fhptr->readpos%OPBUFFSIZE;
	*leng = (writepos-(fhptr->readpos));
	*buff = opbuff+bpos;
	if ((*leng)>(OPBUFFSIZE-bpos)) {
		(*leng) = (OPBUFFSIZE-bpos);
	}
	if ((*leng)>maxleng) {
		(*leng) = maxleng;
	}
	fhptr->readpos+=(*leng);
}

void oplog_releasedata(unsigned long fh) {
	fhentry **fhpptr,*fhptr;
	fhpptr = &fhhead;
	while ((fhptr = *fhpptr)) {
		if (fhptr->fh==fh) {
			fhptr->refcount--;
			if (fhptr->refcount==0) {
				*fhpptr = fhptr->next;
				free(fhptr);
			} else {
				fhpptr = &(fhptr->next);
			}
		} else {
			fhpptr = &(fhptr->next);
		}
	}
	pthread_mutex_unlock(&opbufflock);
}
