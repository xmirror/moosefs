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

#ifndef _STATS_H_
#define _STATS_H_

#include <inttypes.h>

void stats_counter_add(void *node,uint64_t delta);
void stats_counter_sub(void *node,uint64_t delta);
void stats_counter_inc(void *node);
void stats_counter_dec(void *node);
void* stats_get_subnode(void *node,const char *name,uint8_t absolute,uint8_t printflag);
// uint64_t* stats_get_counterptr(void *node);
void stats_reset_all(void);
void stats_show_all(char **buff,uint32_t *leng);
void stats_term(void);

#endif
