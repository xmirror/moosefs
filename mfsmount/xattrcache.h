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

#ifndef _XATTRCACHE_H_
#define _XATTRCACHE_H_

void* xattr_cache_get(uint32_t node,uint32_t uid,uint32_t gid,uint32_t nleng,const uint8_t *name,const uint8_t **value,uint32_t *vleng,int *status);
void xattr_cache_set(uint32_t node,uint32_t uid,uint32_t gid,uint32_t nleng,const uint8_t *name,const uint8_t *value,uint32_t vleng,int status);
void xattr_cache_del(uint32_t node,uint32_t nleng,const uint8_t *name);
void xattr_cache_rel(void *vv);
void xattr_cache_init(double timeout);

#endif
