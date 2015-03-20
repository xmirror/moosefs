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

#ifndef _PCQUEUE_H_
#define _PCQUEUE_H_

#include <inttypes.h>

void* queue_new(uint32_t size);
void queue_delete(void *que);
void queue_close(void *que);
int queue_isempty(void *que);
uint32_t queue_elements(void *que);
int queue_isfull(void *que);
uint32_t queue_sizeleft(void *que);
void queue_put(void *que,uint32_t id,uint32_t op,uint8_t *data,uint32_t leng);
int queue_tryput(void *que,uint32_t id,uint32_t op,uint8_t *data,uint32_t leng);
void queue_get(void *que,uint32_t *id,uint32_t *op,uint8_t **data,uint32_t *leng);
int queue_tryget(void *que,uint32_t *id,uint32_t *op,uint8_t **data,uint32_t *leng);

#endif
