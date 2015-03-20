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

#ifndef _REPLICATOR_H_
#define _REPLICATOR_H_

#include <inttypes.h>

void replicator_stats(uint64_t *bin,uint64_t *bout,uint32_t *repl);
/* srcs: srccnt * (chunkid:64 version:32 ip:32 port:16) */
uint8_t replicate(uint64_t chunkid,uint32_t version,uint8_t srccnt,const uint8_t *srcs);

#endif
