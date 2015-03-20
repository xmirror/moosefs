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

#ifndef _NEGENTRYCACHE_H_
#define _NEGENTRYCACHE_H_

void negentry_cache_remove(uint32_t inode,uint8_t nleng,const uint8_t *name);
void negentry_cache_insert(uint32_t inode,uint8_t nleng,const uint8_t *name);
uint8_t negentry_cache_search(uint32_t inode,uint8_t nleng,const uint8_t *name);
void negentry_cache_clear(void);
void negentry_cache_init(double to);
void negentry_cache_term(void);

#endif
