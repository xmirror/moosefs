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

#ifndef _METADATA_H_
#define _METADATA_H_

#include <stdio.h>
#include <inttypes.h>

uint64_t meta_version_inc(void);
uint64_t meta_version(void);
void meta_cleanup(void);
void meta_setignoreflag(void);
void meta_allowautorestore(void);
void meta_emptystart(void);
void meta_incverboselevel(void);
void meta_sendall(int socket);
int meta_downloadall(int socket);

void meta_text_dump(FILE *fd);

uint64_t meta_get_fileid(void);
void meta_set_fileid(uint64_t metaid);

void meta_info(uint32_t *lsstore,uint32_t *lstime,uint8_t *lsstat);
int meta_init(void);


#endif
