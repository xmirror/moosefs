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

#ifndef _DIRATTRCACHE_H_
#define _DIRATTRCACHE_H_

void* dcache_new(const struct fuse_ctx *ctx,uint32_t parent,const uint8_t *dbuff,uint32_t dsize);
void dcache_release(void *r);
uint8_t dcache_lookup(const struct fuse_ctx *ctx,uint32_t parent,uint8_t nleng,const uint8_t *name,uint32_t *inode,uint8_t attr[35]);
uint8_t dcache_getattr(const struct fuse_ctx *ctx,uint32_t inode,uint8_t attr[35]);
void dcache_setattr(uint32_t inode,const uint8_t attr[35]);
void dcache_invalidate(uint32_t inode);

#endif
