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

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "clocks.h"

#include "mfstest.h"

void universal_usleep(uint64_t usec) {
	struct timeval tv;
	tv.tv_sec = usec/1000000;
	tv.tv_usec = usec%1000000;
	select(0, NULL, NULL, NULL, &tv);
}

int main(void) {
	double st,en;
	uint64_t stusec,enusec;
	uint64_t stnsec,ennsec;

	if (strcmp(monotonic_method(),"time")==0) {
		printf("testing classic 'time(NULL)' clock doesn't make sense\n");
		return 77;
	}
	mfstest_init();

	mfstest_start(monotonic_clocks);
	printf("used method: %s\n",monotonic_method());
	st = monotonic_seconds();
	stusec = monotonic_useconds();
	stnsec = monotonic_nseconds();
	universal_usleep(10000);
	en = monotonic_seconds();
	enusec = monotonic_useconds();
	ennsec = monotonic_nseconds();
	en -= st;
	enusec -= stusec;
	ennsec -= stnsec;
	printf("second: %.6lf ; %"PRIu64" ; %"PRIu64"\n",en,enusec,ennsec);

	mfstest_assert_double_ge(en,0.01);
	mfstest_assert_uint64_ge(enusec,10000);
	mfstest_assert_uint64_ge(ennsec,10000000);
	mfstest_assert_double_lt(en,0.012);
	mfstest_assert_uint64_lt(enusec,12000);
	mfstest_assert_uint64_lt(ennsec,12000000);

	mfstest_end();
	mfstest_return();
}
