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

#ifndef _READDATA_H_
#define _READDATA_H_

#include <sys/uio.h>
#include <inttypes.h>

void read_data_init (uint64_t readaheadsize,uint32_t readaheadleng,uint32_t readaheadtrigger,uint32_t retries);
void read_data_term(void);
int read_data(void *vid, uint64_t offset, uint32_t *size, void **rhead,struct iovec **iov,uint32_t *iovcnt);
void read_data_free_buff(void *vid,void *vrhead,struct iovec *iov);
void read_inode_dirty_region(uint32_t inode,uint64_t offset,uint32_t size,const char *buff);
void read_inode_set_length(uint32_t inode,uint64_t newlength,uint8_t active);
void* read_data_new(uint32_t inode);
void read_data_end(void *vid);

#endif
