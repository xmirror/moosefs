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

#ifndef _MFSSTRERR_H_
#define _MFSSTRERR_H_

#include "MFSCommunication.h"

static inline const char* mfsstrerr(uint8_t status) {
	static const char* errtab[]={ERROR_STRINGS};
	if (status>ERROR_MAX) {
		status=ERROR_MAX;
	}
	return errtab[status];
}

#endif
