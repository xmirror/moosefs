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

#ifndef _MFSTEST_H_
#define _MFSTEST_H_

#include <inttypes.h>

#define STR_AUX(x) #x
#define STR(x) STR_AUX(x)

static uint32_t mfstest_passed;
static uint32_t mfstest_failed;
static uint8_t mfstest_result;

#define mfstest_assert(type,pri,arg1,op,arg2) { \
	type _arg1,_arg2; \
	_arg1 = (arg1); \
	_arg2 = (arg2); \
	if (_arg1 op _arg2) { \
		mfstest_passed++; \
	} else { \
		mfstest_failed++; \
		mfstest_result = 1; \
		printf("%s","Assertion '" STR(arg1) "" STR(op) "" STR(arg2) "' failed: \n"); \
		printf("'%s' == %" pri ", '%s' == %" pri "\n",STR(arg1),_arg1,STR(arg2),_arg2); \
	} \
}

#define mfstest_start(name) { \
	printf("%s","Starting test: " STR(name) "\n"); \
	mfstest_passed = mfstest_failed = 0; \
}

#define mfstest_end() printf("%u%%: Checks: %" PRIu32 ", Failures: %" PRIu32 "\n",100*mfstest_passed/(mfstest_passed+mfstest_failed),(mfstest_passed+mfstest_failed),mfstest_failed)

#define PRIDOUBLE ".10lf"

#define mfstest_assert_uint8(x,op,y) mfstest_assert(uint8_t,PRIu8,x,op,y)
#define mfstest_assert_uint16(x,op,y) mfstest_assert(uint16_t,PRIu16,x,op,y)
#define mfstest_assert_uint32(x,op,y) mfstest_assert(uint32_t,PRIu32,x,op,y)
#define mfstest_assert_uint64(x,op,y) mfstest_assert(uint64_t,PRIu64,x,op,y)
#define mfstest_assert_int8(x,op,y) mfstest_assert(int8_t,PRId8,x,op,y)
#define mfstest_assert_int16(x,op,y) mfstest_assert(int16_t,PRId16,x,op,y)
#define mfstest_assert_int32(x,op,y) mfstest_assert(int32_t,PRId32,x,op,y)
#define mfstest_assert_int64(x,op,y) mfstest_assert(int64_t,PRId64,x,op,y)
#define mfstest_assert_double(x,op,y) mfstest_assert(double,PRIDOUBLE,x,op,y)

#define mfstest_assert_uint8_eq(x,y) mfstest_assert_uint8(x,==,y)
#define mfstest_assert_uint16_eq(x,y) mfstest_assert_uint16(x,==,y)
#define mfstest_assert_uint32_eq(x,y) mfstest_assert_uint32(x,==,y)
#define mfstest_assert_uint64_eq(x,y) mfstest_assert_uint64(x,==,y)
#define mfstest_assert_int8_eq(x,y) mfstest_assert_int8(x,==,y)
#define mfstest_assert_int16_eq(x,y) mfstest_assert_int16(x,==,y)
#define mfstest_assert_int32_eq(x,y) mfstest_assert_int32(x,==,y)
#define mfstest_assert_int64_eq(x,y) mfstest_assert_int64(x,==,y)
#define mfstest_assert_double_eq(x,y) mfstest_assert_double(x,==,y)

#define mfstest_assert_uint8_ne(x,y) mfstest_assert_uint8(x,!=,y)
#define mfstest_assert_uint16_ne(x,y) mfstest_assert_uint16(x,!=,y)
#define mfstest_assert_uint32_ne(x,y) mfstest_assert_uint32(x,!=,y)
#define mfstest_assert_uint64_ne(x,y) mfstest_assert_uint64(x,!=,y)
#define mfstest_assert_int8_ne(x,y) mfstest_assert_int8(x,!=,y)
#define mfstest_assert_int16_ne(x,y) mfstest_assert_int16(x,!=,y)
#define mfstest_assert_int32_ne(x,y) mfstest_assert_int32(x,!=,y)
#define mfstest_assert_int64_ne(x,y) mfstest_assert_int64(x,!=,y)
#define mfstest_assert_double_ne(x,y) mfstest_assert_double(x,!=,y)

#define mfstest_assert_uint8_lt(x,y) mfstest_assert_uint8(x,<,y)
#define mfstest_assert_uint16_lt(x,y) mfstest_assert_uint16(x,<,y)
#define mfstest_assert_uint32_lt(x,y) mfstest_assert_uint32(x,<,y)
#define mfstest_assert_uint64_lt(x,y) mfstest_assert_uint64(x,<,y)
#define mfstest_assert_int8_lt(x,y) mfstest_assert_int8(x,<,y)
#define mfstest_assert_int16_lt(x,y) mfstest_assert_int16(x,<,y)
#define mfstest_assert_int32_lt(x,y) mfstest_assert_int32(x,<,y)
#define mfstest_assert_int64_lt(x,y) mfstest_assert_int64(x,<,y)
#define mfstest_assert_double_lt(x,y) mfstest_assert_double(x,<,y)

#define mfstest_assert_uint8_le(x,y) mfstest_assert_uint8(x,<=,y)
#define mfstest_assert_uint16_le(x,y) mfstest_assert_uint16(x,<=,y)
#define mfstest_assert_uint32_le(x,y) mfstest_assert_uint32(x,<=,y)
#define mfstest_assert_uint64_le(x,y) mfstest_assert_uint64(x,<=,y)
#define mfstest_assert_int8_le(x,y) mfstest_assert_int8(x,<=,y)
#define mfstest_assert_int16_le(x,y) mfstest_assert_int16(x,<=,y)
#define mfstest_assert_int32_le(x,y) mfstest_assert_int32(x,<=,y)
#define mfstest_assert_int64_le(x,y) mfstest_assert_int64(x,<=,y)
#define mfstest_assert_double_le(x,y) mfstest_assert_double(x,<=,y)

#define mfstest_assert_uint8_gt(x,y) mfstest_assert_uint8(x,>,y)
#define mfstest_assert_uint16_gt(x,y) mfstest_assert_uint16(x,>,y)
#define mfstest_assert_uint32_gt(x,y) mfstest_assert_uint32(x,>,y)
#define mfstest_assert_uint64_gt(x,y) mfstest_assert_uint64(x,>,y)
#define mfstest_assert_int8_gt(x,y) mfstest_assert_int8(x,>,y)
#define mfstest_assert_int16_gt(x,y) mfstest_assert_int16(x,>,y)
#define mfstest_assert_int32_gt(x,y) mfstest_assert_int32(x,>,y)
#define mfstest_assert_int64_gt(x,y) mfstest_assert_int64(x,>,y)
#define mfstest_assert_double_gt(x,y) mfstest_assert_double(x,>,y)

#define mfstest_assert_uint8_ge(x,y) mfstest_assert_uint8(x,>=,y)
#define mfstest_assert_uint16_ge(x,y) mfstest_assert_uint16(x,>=,y)
#define mfstest_assert_uint32_ge(x,y) mfstest_assert_uint32(x,>=,y)
#define mfstest_assert_uint64_ge(x,y) mfstest_assert_uint64(x,>=,y)
#define mfstest_assert_int8_ge(x,y) mfstest_assert_int8(x,>=,y)
#define mfstest_assert_int16_ge(x,y) mfstest_assert_int16(x,>=,y)
#define mfstest_assert_int32_ge(x,y) mfstest_assert_int32(x,>=,y)
#define mfstest_assert_int64_ge(x,y) mfstest_assert_int64(x,>=,y)
#define mfstest_assert_double_ge(x,y) mfstest_assert_double(x,>=,y)

#define mfstest_init() mfstest_result=0

#define mfstest_return() return mfstest_result

#endif
