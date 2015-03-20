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

#ifndef _CSDB_H_
#define _CSDB_H_

#include <inttypes.h>

void csdb_init(void);
void csdb_term(void);
uint32_t csdb_getreadcnt(uint32_t ip,uint16_t port);
uint32_t csdb_getwritecnt(uint32_t ip,uint16_t port);
uint32_t csdb_getopcnt(uint32_t ip,uint16_t port);
void csdb_readinc(uint32_t ip,uint16_t port);
void csdb_readdec(uint32_t ip,uint16_t port);
void csdb_writeinc(uint32_t ip,uint16_t port);
void csdb_writedec(uint32_t ip,uint16_t port);

#endif
