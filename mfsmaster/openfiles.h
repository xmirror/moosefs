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

#ifndef _OPENEDFILES_H_
#define _OPENEDFILES_H_

#include <inttypes.h>
#include "bio.h"

void of_openfile(uint32_t sessionid,uint32_t inode);
void of_sync(uint32_t sessionid,uint32_t *inode,uint32_t inodecnt);
void of_sessionremoved(uint32_t sessionid);
uint8_t of_isfileopened(uint32_t inode);
uint32_t of_noofopenedfiles(uint32_t sessionid);

int of_mr_acquire(uint32_t sessionid,uint32_t inode);
int of_mr_release(uint32_t sessionid,uint32_t inode);

uint8_t of_store(bio *fd);
int of_load(bio *fd,uint8_t mver);
void of_cleanup(void);
int of_init(void);

#endif
