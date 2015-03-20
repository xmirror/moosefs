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

#ifndef _POSIXACL_H_
#define _POSIXACL_H_

uint16_t posix_acl_getmode(uint32_t inode);
void posix_acl_setmode(uint32_t inode,uint16_t mode);
int posix_acl_perm(uint32_t inode,uint32_t auid,uint32_t agids,uint32_t *agid,uint32_t fuid,uint32_t fgid,uint16_t modemask);
void posix_acl_remove(uint32_t inode,uint8_t acltype);
uint8_t posix_acl_copydefaults(uint32_t parent,uint32_t inode,uint8_t directory,uint16_t mode);
int posix_acl_set(uint32_t inode,uint8_t acltype,uint16_t userperm,uint16_t groupperm,uint16_t otherperm,uint16_t mask,uint16_t namedusers,uint16_t namedgroups,const uint8_t *aclblob);
int32_t posix_acl_get_blobsize(uint32_t inode,uint8_t acltype,void **aclnode);
void posix_acl_get_data(void *aclnode,uint16_t *userperm,uint16_t *groupperm,uint16_t *otherperm,uint16_t *mask,uint16_t *namedusers,uint16_t *namedgroups,uint8_t *aclblob);
void posix_acl_cleanup(void);
uint8_t posix_acl_store(bio *fd);
int posix_acl_load(bio *fd,uint8_t mver,int ignoreflag);
int posix_acl_init(void);

#endif
