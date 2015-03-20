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

#ifndef _OPLOG_H_
#define _OPLOG_H_

#include <fuse_lowlevel.h>
#include <inttypes.h>

#ifndef __printflike
#ifdef __GNUC__
#define __printflike(fmt,va1) __attribute__((__format__(printf, fmt, va1)))
#else
#define __printflike(fmt, va1)
#endif
#endif /* __printflike */

void oplog_printf(const struct fuse_ctx *ctx,const char *format,...) __printflike(2, 3);
unsigned long oplog_newhandle(int hflag);
void oplog_releasehandle(unsigned long fh);
void oplog_getdata(unsigned long fh,uint8_t **buff,uint32_t *leng,uint32_t maxleng);
void oplog_releasedata(unsigned long fh);

#endif
