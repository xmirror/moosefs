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

#ifndef _BGJOBS_H_
#define _BGJOBS_H_

#include <inttypes.h>

void job_stats(uint32_t *maxjobscnt);
uint32_t job_getload(void);

void job_pool_disable_job(uint32_t jobid);
void job_pool_change_callback(uint32_t jobid,void (*callback)(uint8_t status,void *extra),void *extra);


uint32_t job_inval(void (*callback)(uint8_t status,void *extra),void *extra);
uint32_t job_chunkop(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid,uint32_t version,uint32_t newversion,uint64_t copychunkid,uint32_t copyversion,uint32_t length);

#define job_delete(_cb,_ex,_chunkid,_version) job_chunkop(_cb,_ex,_chunkid,_version,0,0,0,0)
#define job_create(_cb,_ex,_chunkid,_version) job_chunkop(_cb,_ex,_chunkid,_version,0,0,0,1)
#define job_test(_cb,_ex,_chunkid,_version) job_chunkop(_cb,_ex,_chunkid,_version,0,0,0,2)
#define job_version(_cb,_ex,_chunkid,_version,_newversion) (((_newversion)>0)?job_chunkop(_cb,_ex,_chunkid,_version,_newversion,0,0,0xFFFFFFFF):job_inval(_cb,_ex))
#define job_truncate(_cb,_ex,_chunkid,_version,_newversion,_length) (((_newversion)>0&&(_length)!=0xFFFFFFFF)?job_chunkop(_cb,_ex,_chunkid,_version,_newversion,0,0,_length):job_inval(_cb,_ex))
#define job_duplicate(_cb,_ex,_chunkid,_version,_newversion,_copychunkid,_copyversion) (((_newversion>0)&&(_copychunkid)>0)?job_chunkop(_cb,_ex,_chunkid,_version,_newversion,_copychunkid,_copyversion,0xFFFFFFFF):job_inval(_cb,_ex))
#define job_duptrunc(_cb,_ex,_chunkid,_version,_newversion,_copychunkid,_copyversion,_length) (((_newversion>0)&&(_copychunkid)>0&&(_length)!=0xFFFFFFFF)?job_chunkop(_cb,_ex,_chunkid,_version,_newversion,_copychunkid,_copyversion,_length):job_inval(_cb,_ex))

// uint32_t job_open(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid,uint32_t version);
// uint32_t job_close(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid);
// uint32_t job_read(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid,uint32_t version,uint16_t blocknum,uint8_t *buffer,uint32_t offset,uint32_t size,uint8_t *crcbuff);
// uint32_t job_write(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid,uint32_t version,uint16_t blocknum,const uint8_t *buffer,uint32_t offset,uint32_t size,const uint8_t *crcbuff);

uint32_t job_serv_read(void (*callback)(uint8_t status,void *extra),void *extra,int sock,const uint8_t *packet,uint32_t length);
uint32_t job_serv_write(void (*callback)(uint8_t status,void *extra),void *extra,int sock,const uint8_t *packet,uint32_t length);

/* srcs: srccnt * (chunkid:64 version:32 ip:32 port:16) */
uint32_t job_replicate(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid,uint32_t version,uint8_t srccnt,const uint8_t *srcs);
uint32_t job_replicate_simple(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid,uint32_t version,uint32_t ip,uint16_t port);

uint32_t job_get_chunk_blocks(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid,uint32_t version,uint8_t *blocks);
uint32_t job_get_chunk_checksum(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid,uint32_t version,uint8_t *checksum);
uint32_t job_get_chunk_checksum_tab(void (*callback)(uint8_t status,void *extra),void *extra,uint64_t chunkid,uint32_t version,uint8_t *checksum_tab);

// uint32_t job_mainserv(int sock);

int job_init(void);

#endif
