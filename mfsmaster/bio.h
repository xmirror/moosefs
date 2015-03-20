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

#ifndef _BIO_H_
#define _BIO_H_

#include <inttypes.h>

typedef struct _bio bio;

enum {BIO_READ,BIO_WRITE};

bio* bio_file_open(const char *fname,uint8_t direction,uint32_t buffersize);
bio* bio_socket_open(int socket,uint8_t direction,uint32_t buffersize,uint32_t msecto);
uint64_t bio_file_position(bio *b);
uint64_t bio_file_size(bio *b);
int64_t bio_read(bio *b,void *dst,uint64_t len);
int64_t bio_write(bio *b,const void *src,uint64_t len);
int8_t bio_seek(bio *b,int64_t offset,int whence);
void bio_skip(bio *b,uint64_t len);
uint8_t bio_eof(bio *b);
uint8_t bio_error(bio *b);
int bio_descriptor(bio *b);
void bio_close(bio *b);


#endif
