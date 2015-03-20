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

#ifndef _WRITEDATA_H_
#define _WRITEDATA_H_

#include <inttypes.h>

void write_data_init(uint32_t cachesize,uint32_t retries);
void write_data_term(void);
void* write_data_new(uint32_t inode);
int write_data_end(void *vid);
int write_data_flush(void *vid);
uint64_t write_data_getmaxfleng(uint32_t inode);
int write_data_flush_inode(uint32_t inode);
int write_data(void *vid,uint64_t offset,uint32_t size,const uint8_t *buff);
// uint64_t write_data_get_maxfleng(uint32_t inode);

#endif
