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

#ifndef _CSCOMM_H_
#define _CSCOMM_H_

int cs_readblock(int fd,uint64_t chunkid,uint32_t version,uint32_t offset,uint32_t size,uint8_t *buff);
/*
int cs_writestatus(int fd,uint64_t chunkid,uint32_t writeid);
int cs_writeinit(int fd,const uint8_t *chain,uint32_t chainsize,uint64_t chunkid,uint32_t version);
int cs_writeblock(int fd,uint64_t chunkid,uint32_t writeid,uint16_t blockno,uint16_t offset,uint32_t size,uint8_t *buff);
*/

#endif
