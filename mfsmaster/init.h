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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "topology.h"
#include "exports.h"
#include "datacachemgr.h"
#include "matomlserv.h"
#include "matocsserv.h"
#include "matoclserv.h"
#include "metadata.h"
#include "random.h"
#include "changelog.h"
#include "chartsdata.h"

#define STR_AUX(x) #x
#define STR(x) STR_AUX(x)
const char id[]="@(#) version: " VERSSTR ", written by Jakub Kruszona-Zawadzki";

#define MODULE_OPTIONS_GETOPT "iaxe"
#define MODULE_OPTIONS_SWITCH \
	case 'i': \
		meta_setignoreflag(); \
		break; \
	case 'a': \
		meta_allowautorestore(); \
		break; \
	case 'e': \
		meta_emptystart(); \
		break; \
	case 'x': \
		meta_incverboselevel(); \
		break;
#define MODULE_OPTIONS_SYNOPIS "[-i] [-a] [-e] [-x [-x]] "
#define MODULE_OPTIONS_DESC "-i : ignore some metadata structure errors (attach orphans to root, ignore names without inode, etc.)\n-a : automatically restore metadata from change logs\n-e : start without metadata (download all from leader)\n-x : produce more verbose output\n-xx : even more verbose output\n"

/* Run Tab */
typedef int (*runfn)(void);
struct {
	runfn fn;
	char *name;
} RunTab[]={
	{changelog_init,"change log"},
	{rnd_init,"random generator"},
	{dcm_init,"data cache manager"}, // has to be before 'fs_init' and 'matoclserv_networkinit'
	{exports_init,"exports manager"},
	{topology_init,"net topology module"},
	{meta_init,"metadata manager"},
	{chartsdata_init,"charts module"},
	{matomlserv_init,"communication with metalogger"},
	{matocsserv_init,"communication with chunkserver"},
	{matoclserv_init,"communication with clients"},
	{(runfn)0,"****"}
},LateRunTab[]={
	{(runfn)0,"****"}
};
