/*
   Copyright 2008 Gemius SA.

   This file is part of MooseFS.

   MooseFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   MooseFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with MooseFS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>
#include <syslog.h>
#include <time.h>
#include <pthread.h>

#include "MFSCommunication.h"
#include "stats.h"
#include "sockets.h"
#include "md5.h"
#include "datapack.h"

typedef struct _threc {
	pthread_t thid;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	uint8_t *buff;
	uint32_t buffsize;
	uint8_t sent;
	uint8_t status;
	uint8_t release;	// cond variable
	uint32_t size;
	uint32_t cmd;
	uint32_t packetid;
	struct _threc *next;
} threc;

typedef struct _aquired_file {
	uint32_t inode;
	uint32_t cnt;
	struct _aquired_file *next;
} aquired_file;


#define DEFAULT_BUFFSIZE 10000
#define RECEIVE_TIMEOUT 10
#define RETRIES 30

static threc *threchead=NULL;

static aquired_file *afhead=NULL;

static int fd;
static int disconnect;
static time_t lastwrite;
static int sessionlost;

static pthread_t rpthid,npthid;
static pthread_mutex_t fdlock,reclock,aflock;

static uint32_t sessionid;

static char masterstrip[17];
static uint32_t masterip=0;
static uint16_t masterport=0;

void fs_getmasterlocation(uint8_t loc[10]) {
	put32bit(&loc,masterip);
	put16bit(&loc,masterport);
	put32bit(&loc,sessionid);
}

enum {
	MASTER_CONNECTS = 0,
	MASTER_BYTESSENT,
	MASTER_BYTESRCVD,
	MASTER_PACKETSSENT,
	MASTER_PACKETSRCVD,
	STATNODES
};

static uint64_t *statsptr[STATNODES];
static pthread_mutex_t statsptrlock = PTHREAD_MUTEX_INITIALIZER;

void master_statsptr_init(void) {
	void *s;
	s = stats_get_subnode(NULL,"master");
	statsptr[MASTER_PACKETSRCVD] = stats_get_counterptr(stats_get_subnode(s,"packets_received"));
	statsptr[MASTER_PACKETSSENT] = stats_get_counterptr(stats_get_subnode(s,"packets_sent"));
	statsptr[MASTER_BYTESRCVD] = stats_get_counterptr(stats_get_subnode(s,"bytes_received"));
	statsptr[MASTER_BYTESSENT] = stats_get_counterptr(stats_get_subnode(s,"bytes_sent"));
	statsptr[MASTER_CONNECTS] = stats_get_counterptr(stats_get_subnode(s,"reconnects"));
}

void master_stats_inc(uint8_t id) {
	if (id<STATNODES) {
		pthread_mutex_lock(&statsptrlock);
		(*statsptr[id])++;
		pthread_mutex_unlock(&statsptrlock);
	}
}

void master_stats_add(uint8_t id,uint64_t s) {
	if (id<STATNODES) {
		pthread_mutex_lock(&statsptrlock);
		(*statsptr[id])+=s;
		pthread_mutex_unlock(&statsptrlock);
	}
}

const char* errtab[]={ERROR_STRINGS};

static inline const char* mfs_strerror(uint8_t status) {
	if (status>ERROR_MAX) {
		status=ERROR_MAX;
	}
	return errtab[status];
}
/*
void fs_lock_acnt(void) {
	pthread_mutex_lock(&aflock);
}

void fs_unlock_acnt(void) {
	pthread_mutex_unlock(&aflock);
}

uint32_t fs_get_acnt(uint32_t inode) {
	aquired_file *afptr;
	for (afptr=afhead ; afptr ; afptr=afptr->next) {
		if (afptr->inode==inode) {
			return (afptr->cnt);
		}
	}
	return 0;
}
*/

void fs_inc_acnt(uint32_t inode) {
	aquired_file *afptr,**afpptr;
	pthread_mutex_lock(&aflock);
	afpptr = &afhead;
	while ((afptr=*afpptr)) {
		if (afptr->inode==inode) {
			afptr->cnt++;
			pthread_mutex_unlock(&aflock);
			return;
		}
		if (afptr->inode>inode) {
			break;
		}
		afpptr = &(afptr->next);
	}
	afptr = (aquired_file*)malloc(sizeof(aquired_file));
	afptr->inode = inode;
	afptr->cnt = 1;
	afptr->next = *afpptr;
	*afpptr = afptr;
	pthread_mutex_unlock(&aflock);
}

void fs_dec_acnt(uint32_t inode) {
	aquired_file *afptr,**afpptr;
	pthread_mutex_lock(&aflock);
	afpptr = &afhead;
	while ((afptr=*afpptr)) {
		if (afptr->inode == inode) {
			if (afptr->cnt<=1) {
				*afpptr = afptr->next;
				free(afptr);
			} else {
				afptr->cnt--;
			}
			pthread_mutex_unlock(&aflock);
			return;
		}
		afpptr = &(afptr->next);
	}
	pthread_mutex_unlock(&aflock);
}

threc* fs_get_my_threc() {
	pthread_t mythid = pthread_self();
	threc *rec;
	pthread_mutex_lock(&reclock);
	for (rec = threchead ; rec ; rec=rec->next) {
		if (pthread_equal(rec->thid,mythid)) {
			pthread_mutex_unlock(&reclock);
			return rec;
		}
	}
	rec = malloc(sizeof(threc));
	rec->thid = mythid;
	if (threchead==NULL) {
		rec->packetid = 1;
	} else {
		rec->packetid = threchead->packetid+1;
	}
	pthread_mutex_init(&(rec->mutex),NULL);
	pthread_cond_init(&(rec->cond),NULL);
	rec->buff = malloc(DEFAULT_BUFFSIZE);
	if (rec->buff==NULL) {
		free(rec);
		pthread_mutex_unlock(&reclock);
		return NULL;
	}
	rec->buffsize = DEFAULT_BUFFSIZE;
	rec->sent = 0;
	rec->status = 0;
	rec->release = 0;
	rec->cmd = 0;
	rec->size = 0;
	rec->next = threchead;
	//syslog(LOG_NOTICE,"mastercomm: create new threc (%"PRIu32")",rec->packetid);
	threchead = rec;
	pthread_mutex_unlock(&reclock);
	return rec;
}

threc* fs_get_threc_by_id(uint32_t packetid) {
	threc *rec;
	pthread_mutex_lock(&reclock);
	for (rec = threchead ; rec ; rec=rec->next) {
		if (rec->packetid==packetid) {
			pthread_mutex_unlock(&reclock);
			return rec;
		}
	}
	pthread_mutex_unlock(&reclock);
	return NULL;
}

void fs_buffer_init(threc *rec,uint32_t size) {
	if (size>DEFAULT_BUFFSIZE) {
		rec->buff = realloc(rec->buff,size);
		rec->buffsize = size;
	} else if (rec->buffsize>DEFAULT_BUFFSIZE) {
		rec->buff = realloc(rec->buff,DEFAULT_BUFFSIZE);
		rec->buffsize = DEFAULT_BUFFSIZE;
	}
}

uint8_t* fs_createpacket(threc *rec,uint32_t cmd,uint32_t size) {
	uint8_t *ptr;
	uint32_t hdrsize = size+4;
	fs_buffer_init(rec,size+12);
	if (rec->buff==NULL) {
		return NULL;
	}
	ptr = rec->buff;
	put32bit(&ptr,cmd);
	put32bit(&ptr,hdrsize);
	put32bit(&ptr,rec->packetid);
	rec->size = size+12;
	return rec->buff+12;
}

const uint8_t* fs_sendandreceive(threc *rec,uint32_t command_info,uint32_t *info_length) {
	uint32_t cnt;
	uint32_t size = rec->size;

	for (cnt=0 ; cnt<RETRIES ; cnt++) {
		pthread_mutex_lock(&fdlock);
		if (sessionlost) {
			pthread_mutex_unlock(&fdlock);
			return NULL;
		}
		if (fd==-1) {
			pthread_mutex_unlock(&fdlock);
			sleep(1);
			continue;
		}
		//syslog(LOG_NOTICE,"threc(%"PRIu32") - sending ...",rec->packetid);
		rec->release=0;
		if (tcptowrite(fd,rec->buff,size,1000)!=(int32_t)(size)) {
			syslog(LOG_WARNING,"tcp send error: %m");
			disconnect = 1;
			pthread_mutex_unlock(&fdlock);
			sleep(1);
			continue;
		}
		master_stats_add(MASTER_BYTESSENT,size);
		master_stats_inc(MASTER_PACKETSSENT);
		rec->sent = 1;
		lastwrite = time(NULL);
		pthread_mutex_unlock(&fdlock);
		// syslog(LOG_NOTICE,"master: lock: %"PRIu32,rec->packetid);
		pthread_mutex_lock(&(rec->mutex));
		while (rec->release==0) { pthread_cond_wait(&(rec->cond),&(rec->mutex)); }
		pthread_mutex_unlock(&(rec->mutex));
		// syslog(LOG_NOTICE,"master: unlocked: %"PRIu32,rec->packetid);
		// syslog(LOG_NOTICE,"master: command_info: %"PRIu32" ; reccmd: %"PRIu32,command_info,rec->cmd);
		if (rec->status!=0) {
			sleep(1);
			continue;
		}
		if (rec->cmd!=command_info) {
			pthread_mutex_lock(&fdlock);
			disconnect = 1;
			pthread_mutex_unlock(&fdlock);
			sleep(1);
			continue;
		}
		//syslog(LOG_NOTICE,"threc(%"PRIu32") - received",rec->packetid);
		*info_length = rec->size;
		return rec->buff+size;
	}
	return NULL;
}

/*
int fs_direct_connect() {
	int rfd;
	rfd = tcpsocket();
	if (tcpnumconnect(rfd,masterip,masterport)<0) {
		tcpclose(rfd);
		return -1;
	}
	master_stats_inc(MASTER_TCONNECTS);
	return rfd;
}

void fs_direct_close(int rfd) {
	tcpclose(rfd);
}

int fs_direct_write(int rfd,const uint8_t *buff,uint32_t size) {
	int rsize = tcptowrite(rfd,buff,size,60000);
	if (rsize==(int)size) {
		master_stats_add(MASTER_BYTESSENT,size);
	}
	return rsize;
}

int fs_direct_read(int rfd,uint8_t *buff,uint32_t size) {
	int rsize = tcptoread(rfd,buff,size,60000);
	if (rsize>0) {
		master_stats_add(MASTER_BYTESRCVD,rsize);
	}
	return rsize;
}
*/

void fs_reconnect() {
	uint32_t i;
	uint8_t *wptr,regbuff[8+64+9];
	const uint8_t *rptr;

	if (sessionid==0) {
		syslog(LOG_WARNING,"can't register: session not created");
		return;
	}
	fd = tcpsocket();
	if (tcpnodelay(fd)<0) {
		syslog(LOG_WARNING,"can't set TCP_NODELAY: %m");
	}
	if (tcpnumconnect(fd,masterip,masterport)<0) {
		syslog(LOG_WARNING,"can't connect to master (\"%s\":\"%"PRIu16"\")",masterstrip,masterport);
		tcpclose(fd);
		fd=-1;
		return;
	}
	master_stats_inc(MASTER_CONNECTS);
	wptr = regbuff;
	put32bit(&wptr,CUTOMA_FUSE_REGISTER);
	put32bit(&wptr,73);
	memcpy(wptr,FUSE_REGISTER_BLOB_ACL,64);
	wptr+=64;
	put8bit(&wptr,REGISTER_RECONNECT);
	put32bit(&wptr,sessionid);
	put16bit(&wptr,VERSMAJ);
	put8bit(&wptr,VERSMID);
	put8bit(&wptr,VERSMIN);
	if (tcptowrite(fd,regbuff,8+64+9,1000)!=8+64+9) {
		syslog(LOG_WARNING,"master: register error (write: %m)");
		tcpclose(fd);
		fd=-1;
		return;
	}
	master_stats_add(MASTER_BYTESSENT,16+64);
	master_stats_inc(MASTER_PACKETSSENT);
	if (tcptoread(fd,regbuff,8,1000)!=8) {
		syslog(LOG_WARNING,"master: register error (read header: %m)");
		tcpclose(fd);
		fd=-1;
		return;
	}
	master_stats_add(MASTER_BYTESRCVD,8);
	rptr = regbuff;
	i = get32bit(&rptr);
	if (i!=MATOCU_FUSE_REGISTER) {
		syslog(LOG_WARNING,"master: register error (bad answer: %"PRIu32")",i);
		tcpclose(fd);
		fd=-1;
		return;
	}
	i = get32bit(&rptr);
	if (i!=1) {
		syslog(LOG_WARNING,"master: register error (bad length: %"PRIu32")",i);
		tcpclose(fd);
		fd=-1;
		return;
	}
	if (tcptoread(fd,regbuff,i,1000)!=(int32_t)i) {
		syslog(LOG_WARNING,"master: register error (read data: %m)");
		tcpclose(fd);
		fd=-1;
		return;
	}
	master_stats_add(MASTER_BYTESRCVD,i);
	master_stats_inc(MASTER_PACKETSRCVD);
	rptr = regbuff;
	if (rptr[0]!=0) {
		sessionlost=1;
		syslog(LOG_WARNING,"master: register status: %s",mfs_strerror(rptr[0]));
		tcpclose(fd);
		fd=-1;
		return;
	}
	lastwrite=time(NULL);
	syslog(LOG_NOTICE,"registered to master");
}

int fs_connect(uint8_t meta,const char *info,const char *subfolder,const uint8_t passworddigest[16],uint8_t *sesflags,uint32_t *rootuid,uint32_t *rootgid,uint32_t *mapalluid,uint32_t *mapallgid) {
	uint32_t i;
	uint8_t *wptr,*regbuff;
	md5ctx ctx;
	uint8_t digest[16];
	const uint8_t *rptr;
	uint8_t havepassword;
	uint32_t pleng,ileng;

	havepassword=(passworddigest==NULL)?0:1;
	ileng=strlen(info)+1;
	if (meta) {
		pleng=0;
		regbuff = malloc(8+64+9+ileng+16);
	} else {
		pleng=strlen(subfolder)+1;
		regbuff = malloc(8+64+13+pleng+ileng+16);
	}

	fd = tcpsocket();
	if (tcpnodelay(fd)<0) {
//		syslog(LOG_WARNING,"can't set TCP_NODELAY: %m");
		fprintf(stderr,"can't set RCP_NODELAY\n");
	}
	if (tcpnumconnect(fd,masterip,masterport)<0) {
//		syslog(LOG_WARNING,"can't connect to master (\"%s\":\"%"PRIu16"\")",masterstrip,masterport);
		fprintf(stderr,"can't connect to mfsmaster (\"%s\":\"%"PRIu16"\")\n",masterstrip,masterport);
		tcpclose(fd);
		fd=-1;
		free(regbuff);
		return -1;
	}
	if (havepassword) {
		wptr = regbuff;
		put32bit(&wptr,CUTOMA_FUSE_REGISTER);
		put32bit(&wptr,65);
		memcpy(wptr,FUSE_REGISTER_BLOB_ACL,64);
		wptr+=64;
		put8bit(&wptr,REGISTER_GETRANDOM);
		if (tcptowrite(fd,regbuff,8+65,1000)!=8+65) {
//			syslog(LOG_WARNING,"master: register error (write: %m)");
			fprintf(stderr,"error sending data to mfsmaster\n");
			tcpclose(fd);
			fd=-1;
			free(regbuff);
			return -1;
		}
		if (tcptoread(fd,regbuff,8,1000)!=8) {
//			syslog(LOG_WARNING,"master: register error (read header: %m)");
			fprintf(stderr,"error receiving data from mfsmaster\n");
			tcpclose(fd);
			fd=-1;
			free(regbuff);
			return -1;
		}
		rptr = regbuff;
		i = get32bit(&rptr);
		if (i!=MATOCU_FUSE_REGISTER) {
//			syslog(LOG_WARNING,"master: register error (bad answer: %"PRIu32")",i);
			fprintf(stderr,"got incorrect answer from mfsmaster\n");
			tcpclose(fd);
			fd=-1;
			free(regbuff);
			return -1;
		}
		i = get32bit(&rptr);
		if (i!=32) {
			fprintf(stderr,"got incorrect answer from mfsmaster\n");
			tcpclose(fd);
			fd=-1;
			free(regbuff);
			return -1;
		}
		if (tcptoread(fd,regbuff,32,1000)!=32) {
//			syslog(LOG_WARNING,"master: register error (read header: %m)");
			fprintf(stderr,"error receiving data from mfsmaster\n");
			tcpclose(fd);
			fd=-1;
			free(regbuff);
			return -1;
		}
//		memcpy(passwordblock+32,passwordblock+16,16);
//		memcpy(passwordblock+16,passworddigest,16);
		md5_init(&ctx);
		md5_update(&ctx,regbuff,16);
		md5_update(&ctx,passworddigest,16);
		md5_update(&ctx,regbuff+16,16);
		md5_final(digest,&ctx);
	}
	wptr = regbuff;
	put32bit(&wptr,CUTOMA_FUSE_REGISTER);
	if (meta) {
		if (havepassword) {
			put32bit(&wptr,64+9+ileng+16);
		} else {
			put32bit(&wptr,64+9+ileng);
		}
	} else {
		if (havepassword) {
			put32bit(&wptr,64+13+ileng+pleng+16);
		} else {
			put32bit(&wptr,64+13+ileng+pleng);
		}
	}
	memcpy(wptr,FUSE_REGISTER_BLOB_ACL,64);
	wptr+=64;
	put8bit(&wptr,(meta)?REGISTER_NEWMETASESSION:REGISTER_NEWSESSION);
	put16bit(&wptr,VERSMAJ);
	put8bit(&wptr,VERSMID);
	put8bit(&wptr,VERSMIN);
	put32bit(&wptr,ileng);
	memcpy(wptr,info,ileng);
	wptr+=ileng;
	if (!meta) {
		put32bit(&wptr,pleng);
		memcpy(wptr,subfolder,pleng);
	}
	if (havepassword) {
		memcpy(wptr+pleng,digest,16);
	}
	if (tcptowrite(fd,regbuff,8+64+(meta?9:13)+ileng+pleng+(havepassword?16:0),1000)!=(int32_t)(8+64+(meta?9:13)+ileng+pleng+(havepassword?16:0))) {
//		syslog(LOG_WARNING,"master: register error (write: %m)");
		fprintf(stderr,"error sending data to mfsmaster\n");
		tcpclose(fd);
		fd=-1;
		free(regbuff);
		return -1;
	}
	if (tcptoread(fd,regbuff,8,1000)!=8) {
//		syslog(LOG_WARNING,"master: register error (read header: %m)");
		fprintf(stderr,"error receiving data from mfsmaster\n");
		tcpclose(fd);
		fd=-1;
		free(regbuff);
		return -1;
	}
	rptr = regbuff;
	i = get32bit(&rptr);
	if (i!=MATOCU_FUSE_REGISTER) {
//		syslog(LOG_WARNING,"master: register error (bad answer: %"PRIu32")",i);
		fprintf(stderr,"got incorrect answer from mfsmaster\n");
		tcpclose(fd);
		fd=-1;
		free(regbuff);
		return -1;
	}
	i = get32bit(&rptr);
	if ( !(i==1 || (meta && i==5) || (meta==0 && (i==13 || i==21)))) {
//		syslog(LOG_WARNING,"master: register error (bad length: %"PRIu32")",i);
		fprintf(stderr,"got incorrect answer from mfsmaster\n");
		tcpclose(fd);
		fd=-1;
		free(regbuff);
		return -1;
	}
	if (tcptoread(fd,regbuff,i,1000)!=(int32_t)i) {
//		syslog(LOG_WARNING,"master: register error (read data: %m)");
		fprintf(stderr,"error receiving data from mfsmaster\n");
		tcpclose(fd);
		fd=-1;
		free(regbuff);
		return -1;
	}
	rptr = regbuff;
	if (i==1) {
//		syslog(LOG_WARNING,"master: register status: %"PRIu8,rptr[0]);
		fprintf(stderr,"mfsmaster register error: %s\n",mfs_strerror(rptr[0]));
		tcpclose(fd);
		fd=-1;
		free(regbuff);
		return -1;
	}
	sessionid = get32bit(&rptr);
	if (sesflags) {
		*sesflags = get8bit(&rptr);
	} else {
		rptr++;
	}
	if (!meta) {
		if (rootuid) {
			*rootuid = get32bit(&rptr);
		} else {
			rptr+=4;
		}
		if (rootgid) {
			*rootgid = get32bit(&rptr);
		} else {
			rptr+=4;
		}
		if (i==21) {
			if (mapalluid) {
				*mapalluid = get32bit(&rptr);
			} else {
				rptr+=4;
			}
			if (mapallgid) {
				*mapallgid = get32bit(&rptr);
			} else {
				rptr+=4;
			}
		} else {
			if (mapalluid) {
				*mapalluid = 0;
			}
			if (mapallgid) {
				*mapallgid = 0;
			}
		}
	}
	free(regbuff);
	lastwrite=time(NULL);
//	syslog(LOG_NOTICE,"registered to master");
	return 0;
}


void* fs_nop_thread(void *arg) {
	uint8_t *ptr,hdr[12],*inodespacket;
	int32_t inodesleng;
	aquired_file *afptr;
	int now;
	int lastinodeswrite=0;
	(void)arg;
	for (;;) {
		now = time(NULL);
		pthread_mutex_lock(&fdlock);
		if (disconnect==0 && fd>=0) {
			if (lastwrite+2<now) {	// NOP
				ptr = hdr;
				put32bit(&ptr,ANTOAN_NOP);
				put32bit(&ptr,4);
				put32bit(&ptr,0);
				if (tcptowrite(fd,hdr,12,1000)!=12) {
					disconnect=1;
				} else {
					master_stats_add(MASTER_BYTESSENT,12);
					master_stats_inc(MASTER_PACKETSSENT);
				}
				lastwrite=now;
			}
			if (lastinodeswrite+60<now) {	// RESERVED INODES
				pthread_mutex_lock(&aflock);
				inodesleng=8;
				for (afptr=afhead ; afptr ; afptr=afptr->next) {
					//syslog(LOG_NOTICE,"reserved inode: %"PRIu32,afptr->inode);
					inodesleng+=4;
				}
				inodespacket = malloc(inodesleng);
				ptr = inodespacket;
				put32bit(&ptr,CUTOMA_FUSE_RESERVED_INODES);
				put32bit(&ptr,inodesleng-8);
				for (afptr=afhead ; afptr ; afptr=afptr->next) {
					put32bit(&ptr,afptr->inode);
				}
				if (tcptowrite(fd,inodespacket,inodesleng,1000)!=inodesleng) {
					disconnect=1;
				} else {
					master_stats_add(MASTER_BYTESSENT,inodesleng);
					master_stats_inc(MASTER_PACKETSSENT);
				}
				free(inodespacket);
				pthread_mutex_unlock(&aflock);
				lastinodeswrite=now;
			}
		}
		pthread_mutex_unlock(&fdlock);
		sleep(1);
	}
}

void* fs_receive_thread(void *arg) {
	const uint8_t *ptr;
	uint8_t hdr[12];
	threc *rec;
	uint32_t cmd,size,packetid;
	int r;

	(void)arg;
	for (;;) {
		pthread_mutex_lock(&fdlock);
		if (disconnect) {
			tcpclose(fd);
			fd=-1;
			disconnect=0;
			// send to any threc status error and unlock them
			pthread_mutex_lock(&reclock);
			for (rec=threchead ; rec ; rec=rec->next) {
				if (rec->sent) {
					rec->status = 1;
					pthread_mutex_lock(&(rec->mutex));
					rec->release = 1;
					pthread_mutex_unlock(&(rec->mutex));
					pthread_cond_signal(&(rec->cond));
				}
			}
			pthread_mutex_unlock(&reclock);
		}
		if (fd==-1) {
			fs_reconnect();
		}
		if (fd==-1) {
			pthread_mutex_unlock(&fdlock);
			sleep(2);	// reconnect every 2 seconds
			continue;
		}
		pthread_mutex_unlock(&fdlock);
		r = tcptoread(fd,hdr,12,RECEIVE_TIMEOUT*1000);	// read timeout - 4 seconds
		// syslog(LOG_NOTICE,"master: header size: %d",r);
		if (r==0) {
			syslog(LOG_WARNING,"master: connection lost (1)");
			disconnect=1;
			continue;
		}
		if (r!=12) {
			syslog(LOG_WARNING,"master: tcp recv error: %m (1)");
			disconnect=1;
			continue;
		}
		master_stats_add(MASTER_BYTESRCVD,12);

		ptr = hdr;
		cmd = get32bit(&ptr);
		size = get32bit(&ptr);
		packetid = get32bit(&ptr);
		if (cmd==ANTOAN_NOP && size==4) {
			// syslog(LOG_NOTICE,"master: got nop");
			master_stats_inc(MASTER_PACKETSRCVD);
			continue;
		}
		if (size<4) {
			syslog(LOG_WARNING,"master: packet too small");
			disconnect=1;
			continue;
		}
		size-=4;
		rec = fs_get_threc_by_id(packetid);
		if (rec==NULL) {
			syslog(LOG_WARNING,"master: got unexpected queryid");
			disconnect=1;
			continue;
		}
		fs_buffer_init(rec,rec->size+size);
		if (rec->buff==NULL) {
			disconnect=1;
			continue;
		}
		// syslog(LOG_NOTICE,"master: expected data size: %"PRIu32,size);
		if (size>0) {
			r = tcptoread(fd,rec->buff+rec->size,size,1000);
			// syslog(LOG_NOTICE,"master: data size: %d",r);
			if (r==0) {
				syslog(LOG_WARNING,"master: connection lost (2)");
				disconnect=1;
				continue;
			}
			if (r!=(int32_t)(size)) {
				syslog(LOG_WARNING,"master: tcp recv error: %m (2)");
				disconnect=1;
				continue;
			}
			master_stats_add(MASTER_BYTESRCVD,size);
		}
		master_stats_inc(MASTER_PACKETSRCVD);
		rec->sent=0;
		rec->status=0;
		rec->size = size;
		rec->cmd = cmd;
		// syslog(LOG_NOTICE,"master: unlock: %"PRIu32,rec->packetid);
		pthread_mutex_lock(&(rec->mutex));
		rec->release = 1;
		pthread_mutex_unlock(&(rec->mutex));
		pthread_cond_signal(&(rec->cond));
	}
}

// called before fork
int fs_init_master_connection(const char *masterhostname,const char *masterportname,uint8_t meta,const char *info,const char *subfolder,const uint8_t passworddigest[16],uint8_t *flags,uint32_t *rootuid,uint32_t *rootgid,uint32_t *mapalluid,uint32_t *mapallgid) {
	master_statsptr_init();
	if (sockaddrconvert(masterhostname,masterportname,"tcp",&masterip,&masterport)<0) {
		return -1;
	}
	snprintf(masterstrip,17,"%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8,(masterip>>24)&0xFF,(masterip>>16)&0xFF,(masterip>>8)&0xFF,masterip&0xFF);
	masterstrip[16]=0;
	fd = -1;
	sessionlost = 0;
	sessionid = 0;
	disconnect = 0;
	return fs_connect(meta,info,subfolder,passworddigest,flags,rootuid,rootgid,mapalluid,mapallgid);
}

// called after fork
void fs_init_threads(void) {
	pthread_mutex_init(&reclock,NULL);
	pthread_mutex_init(&fdlock,NULL);
	pthread_mutex_init(&aflock,NULL);
	pthread_create(&rpthid,NULL,fs_receive_thread,NULL);
	pthread_create(&npthid,NULL,fs_nop_thread,NULL);
}


void fs_statfs(uint64_t *totalspace,uint64_t *availspace,uint64_t *trashspace,uint64_t *reservedspace,uint32_t *inodes) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_STATFS,0);
	if (wptr==NULL) {
		*totalspace = 0;
		*availspace = 0;
		*trashspace = 0;
		*reservedspace = 0;
		*inodes = 0;
		return;
	}
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_STATFS,&i);
	if (rptr==NULL || i!=36) {
		*totalspace = 0;
		*availspace = 0;
		*trashspace = 0;
		*reservedspace = 0;
		*inodes = 0;
	} else {
		*totalspace = get64bit(&rptr);
		*availspace = get64bit(&rptr);
		*trashspace = get64bit(&rptr);
		*reservedspace = get64bit(&rptr);
		*inodes = get32bit(&rptr);
	}
}

uint8_t fs_access(uint32_t inode,uint32_t uid,uint32_t gid,uint8_t modemask) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_ACCESS,13);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	put8bit(&wptr,modemask);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_ACCESS,&i);
	if (!rptr || i!=1) {
		ret = ERROR_IO;
	} else {
		ret = rptr[0];
	}
	return ret;
}

uint8_t fs_lookup(uint32_t parent,uint8_t nleng,const uint8_t *name,uint32_t uid,uint32_t gid,uint32_t *inode,uint8_t attr[35]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint32_t t32;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_LOOKUP,13+nleng);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,parent);
	put8bit(&wptr,nleng);
	memcpy(wptr,name,nleng);
	wptr+=nleng;
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_LOOKUP,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i!=39) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		t32 = get32bit(&rptr);
		*inode = t32;
		memcpy(attr,rptr,35);
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_getattr(uint32_t inode,uint32_t uid,uint32_t gid,uint8_t attr[35]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_GETATTR,12);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_GETATTR,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i!=35) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		memcpy(attr,rptr,35);
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_setattr(uint32_t inode,uint32_t uid,uint32_t gid,uint8_t setmask,uint16_t attrmode,uint32_t attruid,uint32_t attrgid,uint32_t attratime,uint32_t attrmtime,uint8_t attr[35]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_SETATTR,31);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	put8bit(&wptr,setmask);
	put16bit(&wptr,attrmode);
	put32bit(&wptr,attruid);
	put32bit(&wptr,attrgid);
	put32bit(&wptr,attratime);
	put32bit(&wptr,attrmtime);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_SETATTR,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i!=35) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		memcpy(attr,rptr,35);
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_truncate(uint32_t inode,uint8_t opened,uint32_t uid,uint32_t gid,uint64_t attrlength,uint8_t attr[35]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_TRUNCATE,21);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put8bit(&wptr,opened);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	put64bit(&wptr,attrlength);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_TRUNCATE,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i!=35) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		memcpy(attr,rptr,35);
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_readlink(uint32_t inode,const uint8_t **path) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint32_t pleng;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_READLINK,4);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_READLINK,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i<4) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		pleng = get32bit(&rptr);
		if (i!=4+pleng || pleng==0 || rptr[pleng-1]!=0) {
			pthread_mutex_lock(&fdlock);
			disconnect = 1;
			pthread_mutex_unlock(&fdlock);
			ret = ERROR_IO;
		} else {
			*path = rptr;
			//*path = malloc(pleng);
			//memcpy(*path,ptr,pleng);
			ret = STATUS_OK;
		}
	}
	return ret;
}

uint8_t fs_symlink(uint32_t parent,uint8_t nleng,const uint8_t *name,const uint8_t *path,uint32_t uid,uint32_t gid,uint32_t *inode,uint8_t attr[35]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint32_t t32;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	t32 = strlen((const char *)path)+1;
	wptr = fs_createpacket(rec,CUTOMA_FUSE_SYMLINK,t32+nleng+17);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,parent);
	put8bit(&wptr,nleng);
	memcpy(wptr,name,nleng);
	wptr+=nleng;
	put32bit(&wptr,t32);
	memcpy(wptr,path,t32);
	wptr+=t32;
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_SYMLINK,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i!=39) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		t32 = get32bit(&rptr);
		*inode = t32;
		memcpy(attr,rptr,35);
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_mknod(uint32_t parent,uint8_t nleng,const uint8_t *name,uint8_t type,uint16_t mode,uint32_t uid,uint32_t gid,uint32_t rdev,uint32_t *inode,uint8_t attr[35]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint32_t t32;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_MKNOD,20+nleng);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,parent);
	put8bit(&wptr,nleng);
	memcpy(wptr,name,nleng);
	wptr+=nleng;
	put8bit(&wptr,type);
	put16bit(&wptr,mode);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	put32bit(&wptr,rdev);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_MKNOD,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i!=39) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		t32 = get32bit(&rptr);
		*inode = t32;
		memcpy(attr,rptr,35);
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_mkdir(uint32_t parent,uint8_t nleng,const uint8_t *name,uint16_t mode,uint32_t uid,uint32_t gid,uint32_t *inode,uint8_t attr[35]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint32_t t32;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_MKDIR,15+nleng);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,parent);
	put8bit(&wptr,nleng);
	memcpy(wptr,name,nleng);
	wptr+=nleng;
	put16bit(&wptr,mode);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_MKDIR,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i!=39) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		t32 = get32bit(&rptr);
		*inode = t32;
		memcpy(attr,rptr,35);
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_unlink(uint32_t parent,uint8_t nleng,const uint8_t *name,uint32_t uid,uint32_t gid) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_UNLINK,13+nleng);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,parent);
	put8bit(&wptr,nleng);
	memcpy(wptr,name,nleng);
	wptr+=nleng;
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_UNLINK,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	return ret;
}

uint8_t fs_rmdir(uint32_t parent,uint8_t nleng,const uint8_t *name,uint32_t uid,uint32_t gid) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_RMDIR,13+nleng);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,parent);
	put8bit(&wptr,nleng);
	memcpy(wptr,name,nleng);
	wptr+=nleng;
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_RMDIR,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	return ret;
}

uint8_t fs_rename(uint32_t parent_src,uint8_t nleng_src,const uint8_t *name_src,uint32_t parent_dst,uint8_t nleng_dst,const uint8_t *name_dst,uint32_t uid,uint32_t gid) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_RENAME,18+nleng_src+nleng_dst);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,parent_src);
	put8bit(&wptr,nleng_src);
	memcpy(wptr,name_src,nleng_src);
	wptr+=nleng_src;
	put32bit(&wptr,parent_dst);
	put8bit(&wptr,nleng_dst);
	memcpy(wptr,name_dst,nleng_dst);
	wptr+=nleng_dst;
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_RENAME,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	return ret;
}

uint8_t fs_link(uint32_t inode_src,uint32_t parent_dst,uint8_t nleng_dst,const uint8_t *name_dst,uint32_t uid,uint32_t gid,uint32_t *inode,uint8_t attr[35]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint32_t t32;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_LINK,17+nleng_dst);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode_src);
	put32bit(&wptr,parent_dst);
	put8bit(&wptr,nleng_dst);
	memcpy(wptr,name_dst,nleng_dst);
	wptr+=nleng_dst;
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_LINK,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i!=39) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		t32 = get32bit(&rptr);
		*inode = t32;
		memcpy(attr,rptr,35);
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_getdir(uint32_t inode,uint32_t uid,uint32_t gid,const uint8_t **dbuff,uint32_t *dbuffsize) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_GETDIR,12);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_GETDIR,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		*dbuff = rptr;
		*dbuffsize = i;
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_getdir_plus(uint32_t inode,uint32_t uid,uint32_t gid,const uint8_t **dbuff,uint32_t *dbuffsize) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_GETDIR,13);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	put8bit(&wptr,GETDIR_FLAG_WITHATTR);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_GETDIR,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		*dbuff = rptr;
		*dbuffsize = i;
		ret = STATUS_OK;
	}
	return ret;
}

/*
uint8_t fs_check(uint32_t inode,uint8_t dbuff[22]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	uint16_t cbuff[11];
	uint8_t copies;
	uint16_t chunks;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_CHECK,4);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_CHECK,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i%3!=0) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		for (copies=0 ; copies<11 ; copies++) {
			cbuff[copies]=0;
		}
		while (i>0) {
			copies = get8bit(&rptr);
			chunks = get16bit(&rptr);
			if (copies<10) {
				cbuff[copies]+=chunks;
			} else {
				cbuff[10]+=chunks;
			}
			i-=3;
		}
		wptr = dbuff;
		for (copies=0 ; copies<11 ; copies++) {
			chunks = cbuff[copies];
			put16bit(&wptr,chunks);
		}
		ret = STATUS_OK;
	}
	return ret;
}
*/
// FUSE - I/O

uint8_t fs_opencheck(uint32_t inode,uint32_t uid,uint32_t gid,uint8_t flags) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_OPEN,13);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	put8bit(&wptr,flags);
	fs_inc_acnt(inode);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_OPEN,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	if (ret) {	// release on error
		fs_dec_acnt(inode);
	}
	return ret;
}

void fs_release(uint32_t inode) {
	fs_dec_acnt(inode);
}

// release - decrease aquire cnt - if reach 0 send CUTOMA_FUSE_RELEASE
/*
uint8_t fs_release(uint32_t inode) {
	uint8_t *ptr;
	uint32_t i;
	uint8_t ret;
	ptr = fs_createpacket(rec,CUTOMA_FUSE_RELEASE,4);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&ptr,inode);
	ptr = fs_sendandreceive(rec,MATOCU_FUSE_RELEASE,&i);
	if (ptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = ptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	return ret;
}
*/

uint8_t fs_readchunk(uint32_t inode,uint32_t indx,uint64_t *length,uint64_t *chunkid,uint32_t *version,const uint8_t **csdata,uint32_t *csdatasize) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	uint64_t t64;
	uint32_t t32;
	threc *rec = fs_get_my_threc();
	*csdata=NULL;
	*csdatasize=0;
	wptr = fs_createpacket(rec,CUTOMA_FUSE_READ_CHUNK,8);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,indx);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_READ_CHUNK,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i<20 || ((i-20)%6)!=0) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		t64 = get64bit(&rptr);
		*length = t64;
		t64 = get64bit(&rptr);
		*chunkid = t64;
		t32 = get32bit(&rptr);
		*version = t32;
		if (i>20) {
			*csdata = rptr;
			*csdatasize = i-20;
		}
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_writechunk(uint32_t inode,uint32_t indx,uint64_t *length,uint64_t *chunkid,uint32_t *version,const uint8_t **csdata,uint32_t *csdatasize) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	uint64_t t64;
	uint32_t t32;
	threc *rec = fs_get_my_threc();
	*csdata=NULL;
	*csdatasize=0;
	wptr = fs_createpacket(rec,CUTOMA_FUSE_WRITE_CHUNK,8);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,indx);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_WRITE_CHUNK,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i<20 || ((i-20)%6)!=0) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		t64 = get64bit(&rptr);
		*length = t64;
		t64 = get64bit(&rptr);
		*chunkid = t64;
		t32 = get32bit(&rptr);
		*version = t32;
		if (i>20) {
			*csdata = rptr;
			*csdatasize = i-20;
		}
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_writeend(uint64_t chunkid, uint32_t inode, uint64_t length) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_WRITE_CHUNK_END,20);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put64bit(&wptr,chunkid);
	put32bit(&wptr,inode);
	put64bit(&wptr,length);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_WRITE_CHUNK_END,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	return ret;
}


// FUSE - META


uint8_t fs_getreserved(const uint8_t **dbuff,uint32_t *dbuffsize) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_GETRESERVED,0);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_GETRESERVED,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		*dbuff = rptr;
		*dbuffsize = i;
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_gettrash(const uint8_t **dbuff,uint32_t *dbuffsize) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_GETTRASH,0);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_GETTRASH,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		*dbuff = rptr;
		*dbuffsize = i;
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_getdetachedattr(uint32_t inode,uint8_t attr[35]) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_GETDETACHEDATTR,4);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_GETDETACHEDATTR,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i!=35) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		memcpy(attr,rptr,35);
		ret = STATUS_OK;
	}
	return ret;
}

uint8_t fs_gettrashpath(uint32_t inode,const uint8_t **path) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint32_t pleng;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_GETTRASHPATH,4);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_GETTRASHPATH,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else if (i<4) {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	} else {
		pleng = get32bit(&rptr);
		if (i!=4+pleng || pleng==0 || rptr[pleng-1]!=0) {
			pthread_mutex_lock(&fdlock);
			disconnect = 1;
			pthread_mutex_unlock(&fdlock);
			ret = ERROR_IO;
		} else {
			*path = rptr;
			ret = STATUS_OK;
		}
	}
	return ret;
}

uint8_t fs_settrashpath(uint32_t inode,const uint8_t *path) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint32_t t32;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	t32 = strlen((const char *)path)+1;
	wptr = fs_createpacket(rec,CUTOMA_FUSE_SETTRASHPATH,t32+8);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,t32);
	memcpy(wptr,path,t32);
//	ptr+=t32;
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_SETTRASHPATH,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	return ret;
}

uint8_t fs_undel(uint32_t inode) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_UNDEL,4);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_UNDEL,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	return ret;
}

uint8_t fs_purge(uint32_t inode) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_PURGE,4);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_PURGE,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	return ret;
}

/*
uint8_t fs_append(uint32_t inode,uint32_t ainode,uint32_t uid,uint32_t gid) {
	uint8_t *wptr;
	const uint8_t *rptr;
	uint32_t i;
	uint8_t ret;
	threc *rec = fs_get_my_threc();
	wptr = fs_createpacket(rec,CUTOMA_FUSE_APPEND,16);
	if (wptr==NULL) {
		return ERROR_IO;
	}
	put32bit(&wptr,inode);
	put32bit(&wptr,ainode);
	put32bit(&wptr,uid);
	put32bit(&wptr,gid);
	rptr = fs_sendandreceive(rec,MATOCU_FUSE_APPEND,&i);
	if (rptr==NULL) {
		ret = ERROR_IO;
	} else if (i==1) {
		ret = rptr[0];
	} else {
		pthread_mutex_lock(&fdlock);
		disconnect = 1;
		pthread_mutex_unlock(&fdlock);
		ret = ERROR_IO;
	}
	return ret;
}
*/
