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

#ifndef _CSDB_H_
#define _CSDB_H_

#include <inttypes.h>
#include "bio.h"

void* csdb_new_connection(uint32_t ip,uint16_t port,uint16_t csid,void *eptr);
uint16_t csdb_get_csid(void *v_csptr);
void csdb_lost_connection(void *v_csptr);
void csdb_server_load(void *v_csptr,uint32_t load);
uint8_t csdb_server_is_overloaded(void *v_csptr,uint32_t now);
uint32_t csdb_servlist_size(void);
void csdb_servlist_data(uint8_t *ptr);
uint8_t csdb_remove_server(uint32_t ip,uint16_t port);
uint8_t csdb_back_to_work(uint32_t ip,uint16_t port);
uint8_t csdb_maintenance(uint32_t ip,uint16_t port,uint8_t onoff);
// uint8_t csdb_find(uint32_t ip,uint16_t port,uint16_t csid);
uint8_t csdb_have_all_servers(void);
uint8_t csdb_have_more_than_half_servers(void);
uint8_t csdb_replicate_undergoals(void);
void csdb_cleanup(void);
uint16_t csdb_sort_servers(void);
uint16_t csdb_getnumber(void *v_csptr);
uint8_t csdb_store(bio *fd);
int csdb_load(bio *fd,uint8_t mver,int ignoreflag);
int csdb_init(void);
uint8_t csdb_mr_op(uint8_t csop,uint32_t ip,uint16_t port,uint16_t csid);
#define csdb_mr_csadd(x,y) csdb_mr_op(0,x,y,0)
#define csdb_mr_csdel(x,y) csdb_mr_op(1,x,y,0)
uint32_t csdb_getdisconnecttime(void);

#endif
