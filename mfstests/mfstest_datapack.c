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

#include <stdio.h>
#include <inttypes.h>

#include "datapack.h"

#include "mfstest.h"

int main(void) {
	uint64_t buff[2];
	uint8_t *wp;
	const uint8_t *rp;
	uint32_t i;

	mfstest_init();

	wp = (uint8_t*)buff;
	for (i=0 ; i<16 ; i++) {
		wp[i] = ((15-i)*0x10)+i;
	}

	mfstest_start(getbit_uneven);
	rp = (uint8_t*)buff;
	mfstest_assert_uint8_eq(get8bit(&rp),0xF0);
	mfstest_assert_uint16_eq(get16bit(&rp),0xE1D2);
	mfstest_assert_uint32_eq(get32bit(&rp),0xC3B4A596);
	mfstest_assert_uint64_eq(get64bit(&rp),0x8778695A4B3C2D1E);
	mfstest_end();

	mfstest_start(getbit_even);
	rp = (uint8_t*)buff;
	mfstest_assert_uint64_eq(get64bit(&rp),0xF0E1D2C3B4A59687);
	mfstest_assert_uint32_eq(get32bit(&rp),0x78695A4B);
	mfstest_assert_uint16_eq(get16bit(&rp),0x3C2D);
	mfstest_assert_uint8_eq(get8bit(&rp),0x1E);
	mfstest_end();

	wp = (uint8_t*)buff;
	for (i=0; i<16 ; i++) {
		wp[i] = 0;
	}

	put8bit(&wp,0xF0);
	put16bit(&wp,0xE1D2);
	put32bit(&wp,0xC3B4A596);
	put64bit(&wp,0x8778695A4B3C2D1E);
	put8bit(&wp,0x0F);

	mfstest_start(putbit_uneven);
	rp = (uint8_t*)buff;
	for (i=0 ; i<16 ; i++) {
		mfstest_assert_uint8_eq(rp[i],((15-i)*0x10)+i);
	}
	mfstest_end();

	wp = (uint8_t*)buff;
	for (i=0; i<16 ; i++) {
		wp[i] = 0;
	}

	put64bit(&wp,0xF0E1D2C3B4A59687);
	put32bit(&wp,0x78695A4B);
	put16bit(&wp,0x3C2D);
	put8bit(&wp,0x1E);
	put8bit(&wp,0x0F);

	mfstest_start(putbit_even);
	rp = (uint8_t*)buff;
	for (i=0 ; i<16 ; i++) {
		mfstest_assert_uint8_eq(rp[i],((15-i)*0x10)+i);
	}
	mfstest_end();
	mfstest_return();
}

