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

#include <stdlib.h>
#include <inttypes.h>

typedef struct _shp {
	void *pointer;
	void (*freefn)(void*);
	uint32_t refcnt;
} shp;

void* shp_new(void *pointer,void (*freefn)(void*)) {
	shp *s;
	s = malloc(sizeof(shp));
	s->pointer = pointer;
	s->freefn = freefn;
	s->refcnt = 1;
	return s;
}

void* shp_get(void *vs) {
	shp *s = (shp*)vs;
	return s->pointer;
}

void shp_inc(void *vs) {
	shp *s = (shp*)vs;
	s->refcnt++;
}

void shp_dec(void *vs) {
	shp *s = (shp*)vs;
	if (s->refcnt>0) {
		s->refcnt--;
	}
	if (s->refcnt==0) {
		s->freefn(s->pointer);
		free(s);
	}
}
