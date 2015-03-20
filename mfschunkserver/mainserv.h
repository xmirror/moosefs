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

#ifndef _MAINSERV_H_
#define _MAINSERV_H_

void mainserv_stats(uint64_t *bin,uint64_t *bout,uint32_t *hlopr,uint32_t *hlopw);
uint8_t mainserv_read(int sock,const uint8_t *packet,uint32_t length);
uint8_t mainserv_write(int sock,const uint8_t *packet,uint32_t length);
// void mainserv_serve(int sock);
int mainserv_init(void);

#endif
