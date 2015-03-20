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

#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>

#define CSDB_HASHSIZE 256
#define CSDB_HASH(ip,port) (((ip)*0x7b348943+(port))%(CSDB_HASHSIZE))

typedef struct _csdbentry {
	uint32_t ip;
	uint16_t port;
	uint32_t readopcnt;
	uint32_t writeopcnt;
	struct _csdbentry *next;
} csdbentry;

static csdbentry *csdbhtab[CSDB_HASHSIZE];
static pthread_mutex_t *csdblock;

void csdb_init(void) {
	uint32_t i;
	for (i=0 ; i<CSDB_HASHSIZE ; i++) {
		csdbhtab[i]=NULL;
	}
	csdblock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(csdblock,NULL);
}

void csdb_term(void) {
	uint32_t i;
	csdbentry *cs,*csn;

	pthread_mutex_destroy(csdblock);
	free(csdblock);
	for (i=0 ; i<CSDB_HASHSIZE ; i++) {
		for (cs = csdbhtab[i] ; cs ; cs = csn) {
			csn = cs->next;
			free(cs);
		}
	}
}

uint32_t csdb_getreadcnt(uint32_t ip,uint16_t port) {
	uint32_t hash = CSDB_HASH(ip,port);
	uint32_t result = 0;
	csdbentry *e;
	pthread_mutex_lock(csdblock);
	for (e=csdbhtab[hash] ; e ; e=e->next) {
		if (e->ip == ip && e->port == port) {
			result = e->readopcnt;
			break;
		}
	}
	pthread_mutex_unlock(csdblock);
	return result;
}

uint32_t csdb_getwritecnt(uint32_t ip,uint16_t port) {
	uint32_t hash = CSDB_HASH(ip,port);
	uint32_t result = 0;
	csdbentry *e;
	pthread_mutex_lock(csdblock);
	for (e=csdbhtab[hash] ; e ; e=e->next) {
		if (e->ip == ip && e->port == port) {
			result = e->writeopcnt;
			break;
		}
	}
	pthread_mutex_unlock(csdblock);
	return result;
}

uint32_t csdb_getopcnt(uint32_t ip,uint16_t port) {
	uint32_t hash = CSDB_HASH(ip,port);
	uint32_t result = 0;
	csdbentry *e;
	pthread_mutex_lock(csdblock);
	for (e=csdbhtab[hash] ; e ; e=e->next) {
		if (e->ip == ip && e->port == port) {
			result = e->readopcnt + e->writeopcnt;
			break;
		}
	}
	pthread_mutex_unlock(csdblock);
	return result;
}

void csdb_readinc(uint32_t ip,uint16_t port) {
	uint32_t hash = CSDB_HASH(ip,port);
	csdbentry *e;
	pthread_mutex_lock(csdblock);
	for (e=csdbhtab[hash] ; e ; e=e->next) {
		if (e->ip == ip && e->port == port) {
			e->readopcnt++;
			pthread_mutex_unlock(csdblock);
			return;
		}
	}
	e = malloc(sizeof(csdbentry));
	e->ip = ip;
	e->port = port;
	e->readopcnt = 1;
	e->writeopcnt = 0;
	e->next = csdbhtab[hash];
	csdbhtab[hash] = e;
	pthread_mutex_unlock(csdblock);
}

void csdb_readdec(uint32_t ip,uint16_t port) {
	uint32_t hash = CSDB_HASH(ip,port);
	csdbentry *e;
	pthread_mutex_lock(csdblock);
	for (e=csdbhtab[hash] ; e ; e=e->next) {
		if (e->ip == ip && e->port == port) {
			e->readopcnt--;
			pthread_mutex_unlock(csdblock);
			return;
		}
	}
	pthread_mutex_unlock(csdblock);
}

void csdb_writeinc(uint32_t ip,uint16_t port) {
	uint32_t hash = CSDB_HASH(ip,port);
	csdbentry *e;
	pthread_mutex_lock(csdblock);
	for (e=csdbhtab[hash] ; e ; e=e->next) {
		if (e->ip == ip && e->port == port) {
			e->writeopcnt++;
			pthread_mutex_unlock(csdblock);
			return;
		}
	}
	e = malloc(sizeof(csdbentry));
	e->ip = ip;
	e->port = port;
	e->readopcnt = 0;
	e->writeopcnt = 1;
	e->next = csdbhtab[hash];
	csdbhtab[hash] = e;
	pthread_mutex_unlock(csdblock);
}

void csdb_writedec(uint32_t ip,uint16_t port) {
	uint32_t hash = CSDB_HASH(ip,port);
	csdbentry *e;
	pthread_mutex_lock(csdblock);
	for (e=csdbhtab[hash] ; e ; e=e->next) {
		if (e->ip == ip && e->port == port) {
			e->writeopcnt--;
			pthread_mutex_unlock(csdblock);
			return;
		}
	}
	pthread_mutex_unlock(csdblock);
}
