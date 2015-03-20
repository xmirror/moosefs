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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "bgjobs.h"
#include "random.h"
#include "hddspacemgr.h"
#include "masterconn.h"
#include "csserv.h"
#include "mainserv.h"
#include "chartsdata.h"

#define STR_AUX(x) #x
#define STR(x) STR_AUX(x)
const char id[]="@(#) version: " VERSSTR ", written by Jakub Kruszona-Zawadzki";

#define MODULE_OPTIONS_GETOPT ""
#define MODULE_OPTIONS_SWITCH
#define MODULE_OPTIONS_SYNOPIS ""
#define MODULE_OPTIONS_DESC ""

/* Run Tab */
typedef int (*runfn)(void);
struct {
	runfn fn;
	char *name;
} RunTab[]={
	{rnd_init,"random generator"},
	{hdd_init,"hdd space manager"},
	{mainserv_init,"main server threads"},
	{job_init,"jobs manager"},
	{csserv_init,"main server acceptor"},	/* it has to be before "masterconn" */
	{masterconn_init,"master connection module"},
	{chartsdata_init,"charts module"},
	{(runfn)0,"****"}
},LateRunTab[]={
	{hdd_late_init,"hdd space manager - threads"},
	{(runfn)0,"****"}
};
