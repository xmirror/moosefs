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

#ifndef _CHANGELOG_H_
#define _CHANGELOG_H_

#include <inttypes.h>

uint32_t changelog_get_old_changes(uint64_t version,void (*sendfn)(void *,uint64_t,uint8_t *,uint32_t),void *userdata,uint32_t limit);
uint64_t changelog_get_minversion(void);

void changelog_rotate(void);
void changelog_mr(uint64_t version,const char *data);

#ifdef __printflike
void changelog(const char *format,...) __printflike(1, 2);
#else
void changelog(const char *format,...);
#endif
char* changelog_escape_name(uint32_t nleng,const uint8_t *name);
int changelog_init(void);

uint64_t changelog_findfirstversion(const char *fname);
uint64_t changelog_findlastversion(const char *fname);
int changelog_checkname(const char *fname);

#endif
