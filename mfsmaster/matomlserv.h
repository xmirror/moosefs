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

#ifndef _MATOMLSERV_H_
#define _MATOMLSERV_H_

#include <inttypes.h>

uint32_t matomlserv_get_lastsupervisor(void);

uint32_t matomlserv_mloglist_size(void);
void matomlserv_mloglist_data(uint8_t *ptr);

void matomlserv_broadcast_logstring(uint64_t version,uint8_t *logstr,uint32_t logstrsize);
void matomlserv_broadcast_logrotate();
int matomlserv_no_more_pending_jobs(void);
void matomlserv_disconnect_all(void);
uint16_t matomlserv_getport(void);
int matomlserv_init(void);

#endif
