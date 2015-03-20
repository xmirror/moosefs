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

#ifndef _CRC_H_
#define _CRC_H_
#include <inttypes.h>

uint32_t mycrc32(uint32_t crc,const uint8_t *block,uint32_t leng);
uint32_t mycrc32_combine(uint32_t crc1, uint32_t crc2, uint32_t leng2);
#define mycrc32_zeroblock(crc,zeros) mycrc32_combine((crc)^0xFFFFFFFF,0xFFFFFFFF,(zeros))
#define mycrc32_zeroexpanded(crc,block,leng,zeros) mycrc32_zeroblock(mycrc32((crc),(block),(leng)),(zeros))
#define mycrc32_xorblocks(crc,crcblock1,crcblock2,leng) ((crcblock1)^(crcblock2)^mycrc32_zeroblock(crc,leng))

void mycrc32_init(void);

#endif
