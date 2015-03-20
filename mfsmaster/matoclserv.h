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

#ifndef _MATOCLSERV_H_
#define _MATOCLSERV_H_

#include <inttypes.h>

void matoclserv_stats(uint64_t stats[5]);
/*
void matoclserv_notify_attr(uint32_t dirinode,uint32_t inode,const uint8_t attr[35]);
void matoclserv_notify_link(uint32_t dirinode,uint8_t nleng,const uint8_t *name,uint32_t inode,const uint8_t attr[35],uint32_t ts);
void matoclserv_notify_unlink(uint32_t dirinode,uint8_t nleng,const uint8_t *name,uint32_t ts);
void matoclserv_notify_remove(uint32_t dirinode);
void matoclserv_notify_parent(uint32_t dirinode,uint32_t parent);
*/
void matoclserv_chunk_status(uint64_t chunkid,uint8_t status);
int matoclserv_no_more_pending_jobs(void);
void matoclserv_disconnect_all(void);
int matoclserv_init(void);

#endif
