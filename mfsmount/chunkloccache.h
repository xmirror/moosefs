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

#ifndef _CHUNKLOC_CACHE_H_
#define _CHUNKLOC_CACHE_H_

#include <inttypes.h>

void chunkloc_cache_insert(uint32_t inode,uint32_t pos,uint64_t chunkid,uint32_t chunkversion,uint8_t csdatasize,const uint8_t *csdata);
int chunkloc_cache_search(uint32_t inode,uint32_t pos,uint64_t *chunkid,uint32_t *chunkversion,uint8_t *csdatasize,const uint8_t **csdata);
void chunkloc_cache_init(void);
void chunkloc_cache_term(void);

#endif
