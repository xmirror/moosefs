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

#ifndef _XATTR_H_
#define _XATTR_H_

#include <inttypes.h>
#include "bio.h"

int xattr_namecheck(uint8_t anleng,const uint8_t *attrname);
void xattr_removeinode(uint32_t inode);
uint8_t xattr_setattr(uint32_t inode,uint8_t anleng,const uint8_t *attrname,uint32_t avleng,const uint8_t *attrvalue,uint8_t mode);
uint8_t xattr_getattr(uint32_t inode,uint8_t anleng,const uint8_t *attrname,uint32_t *avleng,uint8_t **attrvalue);
uint8_t xattr_listattr_leng(uint32_t inode,void **xanode,uint32_t *xasize);
void xattr_listattr_data(void *xanode,uint8_t *xabuff);
uint8_t xattr_copy(uint32_t srcinode,uint32_t dstinode);
void xattr_cleanup(void);
uint8_t xattr_store(bio *fd);
int xattr_load(bio *fd,uint8_t mver,int ignoreflag);
int xattr_init(void);

#endif
