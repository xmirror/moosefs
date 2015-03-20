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

#ifndef _MASTERPROXY_H_
#define _MASTERPROXY_H_

#include <inttypes.h>

void masterproxy_getlocation(uint8_t *masterinfo);

void masterproxy_term(void);
int masterproxy_init(const char *masterproxyip);

#endif
