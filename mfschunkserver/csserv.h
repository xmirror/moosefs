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

#ifndef _CSSERV_H_
#define _CSSERV_H_

#include <inttypes.h>

void csserv_stats(uint64_t *bin,uint64_t *bout);
// void csserv_cstocs_connected(void *e,void *cptr);
// void csserv_cstocs_gotstatus(void *e,uint64_t chunkid,uint32_t writeid,uint8_t s);
// void csserv_cstocs_disconnected(void *e);
uint32_t csserv_getlistenip();
uint16_t csserv_getlistenport();
int csserv_init(void);

#endif
