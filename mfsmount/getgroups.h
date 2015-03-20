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

#ifndef _GETGROUPS_H_
#define _GETGROUPS_H_

#include <sys/types.h>
#include <inttypes.h>

typedef struct groups {
	double time;
	pid_t pid;
	uid_t uid;
	gid_t gid;
	uint16_t lcnt;
	uint8_t locked;
	uint32_t gidcnt;
	uint32_t *gidtab;
	struct groups *next,**prev;
} groups;

groups* groups_get_x(pid_t pid,uid_t uid,gid_t gid,uint8_t lockmode);
#define groups_get(p,u,g) groups_get_x(p,u,g,0)
void groups_rel(groups* g);
void groups_init(double _to,int dm);

#endif
