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

#ifndef _CFG_H_
#define _CFG_H_

#include <inttypes.h>

#define _CONFIG_MAKE_PROTOTYPE(fname,type) type cfg_get##fname(const char *name,type def)

int cfg_load (const char *fname,int logundefined);
int cfg_reload (void);
void cfg_term (void);

int cfg_isdefined(const char *name);

_CONFIG_MAKE_PROTOTYPE(str,char*);
_CONFIG_MAKE_PROTOTYPE(num,int);
_CONFIG_MAKE_PROTOTYPE(uint8,uint8_t);
_CONFIG_MAKE_PROTOTYPE(int8,int8_t);
_CONFIG_MAKE_PROTOTYPE(uint16,uint16_t);
_CONFIG_MAKE_PROTOTYPE(int16,int16_t);
_CONFIG_MAKE_PROTOTYPE(uint32,uint32_t);
_CONFIG_MAKE_PROTOTYPE(int32,int32_t);
_CONFIG_MAKE_PROTOTYPE(uint64,uint64_t);
_CONFIG_MAKE_PROTOTYPE(int64,int64_t);
_CONFIG_MAKE_PROTOTYPE(double,double);

#endif
