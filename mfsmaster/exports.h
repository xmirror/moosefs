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

#ifndef _EXPORTS_H_
#define _EXPORTS_H_

#include <inttypes.h>

uint32_t exports_info_size(uint8_t versmode);
void exports_info_data(uint8_t versmode,uint8_t *buff);
uint8_t exports_check(uint32_t ip,uint32_t version,uint8_t meta,const uint8_t *path,const uint8_t rndcode[32],const uint8_t passcode[16],uint8_t *sesflags,uint32_t *rootuid,uint32_t *rootgid,uint32_t *mapalluid,uint32_t *mapallgid,uint8_t *mingoal,uint8_t *maxgoal,uint32_t *mintrashtime,uint32_t *maxtrashtime);
int exports_init(void);

#endif
