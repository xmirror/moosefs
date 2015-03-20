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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>

static uint8_t i,j;
static uint8_t p[256];

int rnd_init(void) {
	uint8_t key[64],vkey[64];
	register uint8_t x;
	uint16_t l;

	srandom(time(NULL));
	for (l=0 ; l<64 ; l++) {
		key[l] = random();
		vkey[l] = random();
	}
	for (l=0 ; l<256 ; l++) {
		p[l]=l;
	}
	for (l=0 ; l<768 ; l++) {
		i = l&0xFF;
		x = j+p[i]+key[l%64];
		j = p[x];
		x = p[i];
		p[i] = p[j];
		p[j] = x;
	}
	for (l=0 ; l<768 ; l++) {
		i = l&0xFF;
		x = j+p[i]+vkey[l%64];
		j = p[x];
		x = p[i];
		p[i] = p[j];
		p[j] = x;
	}
	i = 0;
	return 0;
}

#define RND_RC4_STEP(result) { \
	register uint8_t x; \
	x = j+p[i]; \
	j = p[x]; \
	x = p[j]; \
	x = p[x]+1; \
	result = p[x]; \
	x = p[i]; \
	p[i] = p[j]; \
	p[j] = x; \
	i++; \
}

uint8_t rndu8() {
	uint8_t r;
	RND_RC4_STEP(r);
	return r;
}

uint32_t rndu32() {
	uint32_t res;
	uint8_t *r = (uint8_t*)&res;
	RND_RC4_STEP(r[0]);
	RND_RC4_STEP(r[1]);
	RND_RC4_STEP(r[2]);
	RND_RC4_STEP(r[3]);
	return res;
}

uint64_t rndu64() {
	uint64_t res;
	uint8_t *r = (uint8_t*)&res;
	RND_RC4_STEP(r[0]);
	RND_RC4_STEP(r[1]);
	RND_RC4_STEP(r[2]);
	RND_RC4_STEP(r[3]);
	RND_RC4_STEP(r[4]);
	RND_RC4_STEP(r[5]);
	RND_RC4_STEP(r[6]);
	RND_RC4_STEP(r[7]);
	return res;
}

uint64_t rndu64_ranged(uint64_t range) {
	uint64_t max,r;
	r = rndu64();
	if (range==0) {
		return r;
	}
	max = -(((uint64_t)-range) % range);
	if (max) {
		while (r>=max) {
			r = rndu64();
		}
	}
	return r%range;
}

uint32_t rndu32_ranged(uint32_t range) {
	uint32_t max,r;
	r = rndu32();
	if (range==0) {
		return r;
	}
	max = -(((uint32_t)-range) % range);
	if (max) {
		while (r>=max) {
			r = rndu32();
		}
	}
	return r%range;
}

/* test loop:
int main(int argc,char **argv) {
	uint32_t x,y,range;
	uint64_t i;
	double xmean=0.0,ymean=0.0;
	double delta;
	rnd_init();
	if (argc==2) {
		range = strtoul(argv[1],NULL,10);
	} else {
		range = 3000000000;
	}
	i = 1;
	while (i<100000000) {
		x = rndu32()%range;
		y = rndu32_ranged(range);
		delta = x-xmean;
		xmean += delta / i;
		delta = y-ymean;
		ymean += delta / i;
		i++;
		if ((i%1000000)==0) {
			printf("%.6lf : %.6lf\n",((range/2.0)-0.5-xmean)/range,((range/2.0)-0.5-ymean)/range);
		}
	}
	return 0;
}
*/
