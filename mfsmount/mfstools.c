/*
 * Copyright (C) 2015 Jakub Kruszona-Zawadzki, Core Technology Sp. z o.o.
 * 
 * This file is part of MooseFS.
 * 
 * MooseFS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 (only).
 * 
 * MooseFS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MooseFS; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * or visit http://www.gnu.org/licenses/gpl-2.0.html
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <errno.h>

#include "labelparser.h"
#include "datapack.h"
#include "strerr.h"
#include "mfsstrerr.h"
#include "sockets.h"
#include "md5.h"
#include "MFSCommunication.h"

#define tcpread(s,b,l) tcptoread(s,b,l,10000)
#define tcpwrite(s,b,l) tcptowrite(s,b,l,10000)

#define STR_AUX(x) #x
#define STR(x) STR_AUX(x)
const char id[]="@(#) version: " VERSSTR ", written by Jakub Kruszona-Zawadzki";

#define FILEINFO_QUICK 0x01
#define FILEINFO_CRC 0x02
#define FILEINFO_SIGNATURE 0x04

#define DIRINFO_INODES 0x01
#define DIRINFO_DIRS 0x02
#define DIRINFO_FILES 0x04
#define DIRINFO_CHUNKS 0x08
#define DIRINFO_LENGTH 0x10
#define DIRINFO_SIZE 0x20
#define DIRINFO_REALSIZE 0x40

#define INODE_VALUE_MASK 0x1FFFFFFF
#define INODE_TYPE_MASK 0x60000000
#define INODE_TYPE_TRASH 0x20000000
#define INODE_TYPE_SUSTAINED 0x40000000
#define INODE_TYPE_SPECIAL 0x00000000

static const char* eattrtab[EATTR_BITS]={EATTR_STRINGS};
static const char* eattrdesc[EATTR_BITS]={EATTR_DESCRIPTIONS};

static uint8_t humode=0;
static uint8_t numbermode=0;

#define PHN_USESI       0x01
#define PHN_USEIEC      0x00
void print_humanized_number(const char *format,uint64_t number,uint8_t flags) {
	char numbuf[6];	// [ "xxx" , "xx" , "x" , "x.x" ] + ["" , "X" , "Xi"]
	uint64_t divisor;
	uint16_t b;
	uint8_t i;
	uint8_t scale;

	if (flags & PHN_USESI) {
		divisor = 1000;
	} else {
		divisor = 1024;
	}
	if (number>(UINT64_MAX/100)) {
		number /= divisor;
		number *= 100;
		scale = 1;
	} else {
		number *= 100;
		scale = 0;
	}
	while (number>=99950) {
		number /= divisor;
		scale+=1;
	}
	i=0;
	if (number<995 && scale>0) {
		b = ((uint32_t)number + 5) / 10;
		numbuf[i++]=(b/10)+'0';
		numbuf[i++]='.';
		numbuf[i++]=(b%10)+'0';
	} else {
		b = ((uint32_t)number + 50) / 100;
		if (b>=100) {
			numbuf[i++]=(b/100)+'0';
			b%=100;
		}
		if (b>=10 || i>0) {
			numbuf[i++]=(b/10)+'0';
			b%=10;
		}
		numbuf[i++]=b+'0';
	}
	if (scale>0) {
		if (flags&PHN_USESI) {
			numbuf[i++]="-kMGTPE"[scale];
		} else {
			numbuf[i++]="-KMGTPE"[scale];
			numbuf[i++]='i';
		}
	}
	numbuf[i++]='\0';
	printf(format,numbuf);
}

void print_number_only(uint64_t number,uint8_t bytesflag) {
	if (humode>0) {
		if (bytesflag) {
			if (humode==1 || humode==3) {
				print_humanized_number("%5sB",number,PHN_USEIEC);
			} else {
				print_humanized_number("%4sB",number,PHN_USESI);
			}
		} else {
			if (humode==1 || humode==3) {
				print_humanized_number("%5s",number,PHN_USEIEC);
			} else {
				print_humanized_number("%4s",number,PHN_USESI);
			}
		}
		if (humode>2) {
			printf(" (%"PRIu64")",number);
		}
	} else {
		if (numbermode==0) {
			printf("%"PRIu64,number);
		} else if (numbermode==1) {
			printf("%"PRIu64,number/1024);
		} else if (numbermode==2) {
			printf("%"PRIu64,number/(1024*1024));
		} else if (numbermode==3) {
			printf("%"PRIu64,number/(1024*1024*1024));
		}
	}
}

void print_number(const char *prefix,const char *suffix,uint64_t number,uint8_t mode32,uint8_t bytesflag,uint8_t dflag) {
	if (prefix) {
		printf("%s",prefix);
	}
	if (dflag) {
		if (humode>0) {
			if (bytesflag) {
				if (humode==1 || humode==3) {
					print_humanized_number("%5sB",number,PHN_USEIEC);
				} else {
					print_humanized_number("%4sB",number,PHN_USESI);
				}
			} else {
				if (humode==1 || humode==3) {
					print_humanized_number(" %5s",number,PHN_USEIEC);
				} else {
					print_humanized_number(" %4s",number,PHN_USESI);
				}
			}
			if (humode>2) {
				if (mode32) {
					printf(" (%10"PRIu32")",(uint32_t)number);
				} else {
					printf(" (%20"PRIu64")",number);
				}
			}
		} else {
			if (numbermode==0) {
				if (mode32) {
					printf("%10"PRIu32,(uint32_t)number);
				} else {
					printf("%20"PRIu64,number);
				}
			} else if (numbermode==1) {
				if (mode32) {
					printf("%7"PRIu32,((uint32_t)number)/1024);
				} else {
					printf("%17"PRIu64,number/1024);
				}
			} else if (numbermode==2) {
				if (mode32) {
					printf("%4"PRIu32,((uint32_t)number)/(1024*1024));
				} else {
					printf("%14"PRIu64,number/(1024*1024));
				}
			} else if (numbermode==3) {
				if (mode32) {
					printf("%1"PRIu32,((uint32_t)number)/(1024*1024*1024));
				} else {
					printf("%11"PRIu64,number/(1024*1024*1024));
				}
			}
		}
	} else {
		switch(humode) {
		case 0:
			if (numbermode==0) {
				if (mode32) {
					printf("         -");
				} else {
					printf("                   -");
				}
			} else if (numbermode==1) {
				if (mode32) {
					printf("      -");
				} else {
					printf("                -");
				}
			} else if (numbermode==2) {
				if (mode32) {
					printf("   -");
				} else {
					printf("             -");
				}
			} else if (numbermode==3) {
				if (mode32) {
					printf("-");
				} else {
					printf("          -");
				}
			}
			break;
		case 1:
			printf("     -");
			break;
		case 2:
			printf("    -");
			break;
		case 3:
			if (mode32) {
				printf("                  -");
			} else {
				printf("                            -");
			}
			break;
		case 4:
			if (mode32) {
				printf("                 -");
			} else {
				printf("                           -");
			}
			break;
		}
	}
	if (suffix) {
		printf("%s",suffix);
	}
}

int my_get_number(const char *str,uint64_t *ret,double max,uint8_t bytesflag) {
	uint64_t val,frac,fracdiv;
	double drval,mult;
	int f;
	val=0;
	frac=0;
	fracdiv=1;
	f=0;
	while (*str>='0' && *str<='9') {
		f=1;
		val*=10;
		val+=(*str-'0');
		str++;
	}
	if (*str=='.') {	// accept ".5" (without 0)
		str++;
		while (*str>='0' && *str<='9') {
			fracdiv*=10;
			frac*=10;
			frac+=(*str-'0');
			str++;
		}
		if (fracdiv==1) {	// if there was '.' expect number afterwards
			return -1;
		}
	} else if (f==0) {	// but not empty string
		return -1;
	}
	if (str[0]=='\0' || (bytesflag && str[0]=='B' && str[1]=='\0')) {
		mult=1.0;
	} else if (str[0]!='\0' && (str[1]=='\0' || (bytesflag && str[1]=='B' && str[2]=='\0'))) {
		switch(str[0]) {
		case 'k':
			mult=1e3;
			break;
		case 'M':
			mult=1e6;
			break;
		case 'G':
			mult=1e9;
			break;
		case 'T':
			mult=1e12;
			break;
		case 'P':
			mult=1e15;
			break;
		case 'E':
			mult=1e18;
			break;
		default:
			return -1;
		}
	} else if (str[0]!='\0' && str[1]=='i' && (str[2]=='\0' || (bytesflag && str[2]=='B' && str[3]=='\0'))) {
		switch(str[0]) {
		case 'K':
			mult=1024.0;
			break;
		case 'M':
			mult=1048576.0;
			break;
		case 'G':
			mult=1073741824.0;
			break;
		case 'T':
			mult=1099511627776.0;
			break;
		case 'P':
			mult=1125899906842624.0;
			break;
		case 'E':
			mult=1152921504606846976.0;
			break;
		default:
			return -1;
		}
	} else {
		return -1;
	}
	drval = round(((double)frac/(double)fracdiv+(double)val)*mult);
	if (drval>max) {
		return -2;
	} else {
		*ret = drval;
	}
	return 1;
}

int bsd_basename(const char *path,char *bname) {
	const char *endp, *startp;

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') {
		(void)strcpy(bname, ".");
		return 0;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/') {
		endp--;
	}

	/* All slashes becomes "/" */
	if (endp == path && *endp == '/') {
		(void)strcpy(bname, "/");
		return 0;
	}

	/* Find the start of the base */
	startp = endp;
	while (startp > path && *(startp - 1) != '/') {
		startp--;
	}
	if (endp - startp + 2 > PATH_MAX) {
		return -1;
	}
	(void)strncpy(bname, startp, endp - startp + 1);
	bname[endp - startp + 1] = '\0';
	return 0;
}

int bsd_dirname(const char *path,char *bname) {
	const char *endp;

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') {
		(void)strcpy(bname, ".");
		return 0;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/') {
		endp--;
	}

	/* Find the start of the dir */
	while (endp > path && *endp != '/') {
		endp--;
	}

	/* Either the dir is "/" or there are no slashes */
	if (endp == path) {
		(void)strcpy(bname, *endp == '/' ? "/" : ".");
		return 0;
	} else {
		do {
			endp--;
		} while (endp > path && *endp == '/');
	}

	if (endp - path + 2 > PATH_MAX) {
		return -1;
	}
	(void)strncpy(bname, path, endp - path + 1);
	bname[endp - path + 1] = '\0';
	return 0;
}

void dirname_inplace(char *path) {
	char *endp;

	if (path==NULL) {
		return;
	}
	if (path[0]=='\0') {
		path[0]='.';
		path[1]='\0';
		return;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/') {
		endp--;
	}

	/* Find the start of the dir */
	while (endp > path && *endp != '/') {
		endp--;
	}

	if (endp == path) {
		if (path[0]=='/') {
			path[1]='\0';
		} else {
			path[0]='.';
			path[1]='\0';
		}
		return;
	} else {
		*endp = '\0';
	}
}

/*
int32_t socket_read(int sock,void *buff,uint32_t leng) {
	uint32_t rcvd=0;
	int i;
	while (rcvd<leng) {
		i = read(sock,((uint8_t*)buff)+rcvd,leng-rcvd);
		if (i<=0) return i;
		rcvd+=i;
	}
	return rcvd;
}

int32_t socket_write(int sock,void *buff,uint32_t leng) {
	uint32_t sent=0;
	int i;
	while (sent<leng) {
		i = write(sock,((uint8_t*)buff)+sent,leng-sent);
		if (i<=0) return i;
		sent+=i;
	}
	return sent;
}
*/

int master_register_old(int rfd) {
	uint32_t i;
	const uint8_t *rptr;
	uint8_t *wptr,regbuff[8+72];

	wptr = regbuff;
	put32bit(&wptr,CLTOMA_FUSE_REGISTER);
	put32bit(&wptr,68);
	memcpy(wptr,FUSE_REGISTER_BLOB_TOOLS_NOACL,64);
	wptr+=64;
	put16bit(&wptr,VERSMAJ);
	put8bit(&wptr,VERSMID);
	put8bit(&wptr,VERSMIN);
	if (tcpwrite(rfd,regbuff,8+68)!=8+68) {
		printf("register to master: send error\n");
		return -1;
	}
	if (tcpread(rfd,regbuff,9)!=9) {
		printf("register to master: receive error\n");
		return -1;
	}
	rptr = regbuff;
	i = get32bit(&rptr);
	if (i!=MATOCL_FUSE_REGISTER) {
		printf("register to master: wrong answer (type)\n");
		return -1;
	}
	i = get32bit(&rptr);
	if (i!=1) {
		printf("register to master: wrong answer (length)\n");
		return -1;
	}
	if (*rptr) {
		printf("register to master: %s\n",mfsstrerr(*rptr));
		return -1;
	}
	return 0;
}

int master_register(int rfd,uint32_t cuid) {
	uint32_t i;
	const uint8_t *rptr;
	uint8_t *wptr,regbuff[8+73];

	wptr = regbuff;
	put32bit(&wptr,CLTOMA_FUSE_REGISTER);
	put32bit(&wptr,73);
	memcpy(wptr,FUSE_REGISTER_BLOB_ACL,64);
	wptr+=64;
	put8bit(&wptr,REGISTER_TOOLS);
	put32bit(&wptr,cuid);
	put16bit(&wptr,VERSMAJ);
	put8bit(&wptr,VERSMID);
	put8bit(&wptr,VERSMIN);
	if (tcpwrite(rfd,regbuff,8+73)!=8+73) {
		printf("register to master: send error\n");
		return -1;
	}
	if (tcpread(rfd,regbuff,9)!=9) {
		printf("register to master: receive error\n");
		return -1;
	}
	rptr = regbuff;
	i = get32bit(&rptr);
	if (i!=MATOCL_FUSE_REGISTER) {
		printf("register to master: wrong answer (type)\n");
		return -1;
	}
	i = get32bit(&rptr);
	if (i!=1) {
		printf("register to master: wrong answer (length)\n");
		return -1;
	}
	if (*rptr) {
		printf("register to master: %s\n",mfsstrerr(*rptr));
		return -1;
	}
	return 0;
}

static dev_t current_device = 0;
static int current_master = -1;
static uint32_t masterversion = 0;

int open_master_conn(const char *name,uint32_t *inode,mode_t *mode,uint64_t *leng,uint8_t needsamedev,uint8_t needrwfs) {
	char rpath[PATH_MAX+1];
	struct stat stb;
	struct statvfs stvfsb;
	int sd;
	uint8_t masterinfo[14];
	const uint8_t *miptr;
	uint8_t cnt;
	uint32_t masterip;
	uint16_t masterport;
	uint32_t mastercuid;
	uint32_t pinode;
	int rpathlen;

	rpath[0]=0;
	if (realpath(name,rpath)==NULL) {
		printf("%s: realpath error on (%s): %s\n",name,rpath,strerr(errno));
		return -1;
	}
//	p = rpath;
	if (needrwfs) {
		if (statvfs(rpath,&stvfsb)!=0) {
			printf("%s: (%s) statvfs error: %s\n",name,rpath,strerr(errno));
			return -1;
		}
		if (stvfsb.f_flag&ST_RDONLY) {
			printf("%s: (%s) Read-only file system\n",name,rpath);
			return -1;
		}
	}
	if (lstat(rpath,&stb)!=0) {
		printf("%s: (%s) lstat error: %s\n",name,rpath,strerr(errno));
		return -1;
	}
	pinode = stb.st_ino;
	*inode = pinode;
	if (mode) {
		*mode = stb.st_mode;
	}
	if (leng) {
		*leng = stb.st_size;
	}
	if (current_master>=0) {
	       	if (current_device==stb.st_dev) {
			return current_master;
		}
		if (needsamedev) {
			printf("%s: different device\n",name);
			return -1;
		}
	}
	if (current_master>=0) {
		close(current_master);
		current_master=-1;
	}
	current_device = stb.st_dev;
	for(;;) {
		rpathlen = strlen(rpath);
		if (rpathlen+strlen("/.masterinfo")<PATH_MAX) {
			strcpy(rpath+rpathlen,"/.masterinfo");
			if (lstat(rpath,&stb)==0) {
				if ((stb.st_ino==0x7FFFFFFF || stb.st_ino==0x7FFFFFFE) && stb.st_nlink==1 && stb.st_uid==0 && stb.st_gid==0 && (stb.st_size==10 || stb.st_size==14)) {
					if (stb.st_ino==0x7FFFFFFE) {	// meta master
						if (((*inode)&INODE_TYPE_MASK)!=INODE_TYPE_TRASH && ((*inode)&INODE_TYPE_MASK)!=INODE_TYPE_SUSTAINED) {
							printf("%s: only files in 'trash' and 'sustained' are usable in mfsmeta\n",name);
							return -1;
						}
						(*inode)&=INODE_VALUE_MASK;
					}
					sd = open(rpath,O_RDONLY);
					if (stb.st_size==10) {
						if (read(sd,masterinfo,10)!=10) {
							printf("%s: can't read '.masterinfo'\n",name);
							close(sd);
							return -1;
						}
					} else if (stb.st_size==14) {
						if (read(sd,masterinfo,14)!=14) {
							printf("%s: can't read '.masterinfo'\n",name);
							close(sd);
							return -1;
						}
					}
					close(sd);
					miptr = masterinfo;
					masterip = get32bit(&miptr);
					masterport = get16bit(&miptr);
					mastercuid = get32bit(&miptr);
					if (stb.st_size==14) {
						masterversion = get32bit(&miptr);
					} else {
						masterversion = 0;
					}
					if (masterip==0 || masterport==0 || mastercuid==0) {
						printf("%s: incorrect '.masterinfo'\n",name);
						return -1;
					}
					cnt=0;
					while (cnt<10) {
						sd = tcpsocket();
						if (sd<0) {
							printf("%s: can't create connection socket: %s\n",name,strerr(errno));
							return -1;
						}
						if (tcpnumtoconnect(sd,masterip,masterport,(cnt%2)?(300*(1<<(cnt>>1))):(200*(1<<(cnt>>1))))<0) {
							cnt++;
							if (cnt==10) {
								printf("%s: can't connect to master (.masterinfo): %s\n",name,strerr(errno));
								return -1;
							}
							tcpclose(sd);
						} else {
							cnt=10;
						}
					}
					if (master_register(sd,mastercuid)<0) {
						printf("%s: can't register to master (.masterinfo)\n",name);
						return -1;
					}
					current_master = sd;
					return sd;
				}
			} else if (pinode==1) { // this is root inode - if there is no .masterinfo here then it is not MFS.
				printf("%s: not MFS object\n",name);
				return -1;
			}
		} else if (pinode==1) { // found root inode, but path is still to long - give up
			printf("%s: path too long\n",name);
			return -1;
		}
		rpath[rpathlen]='\0';
		if (rpath[0]!='/' || rpath[1]=='\0') { // went to '/' without success - this is not MFS
			printf("%s: not MFS object\n",name);
			return -1;
		}
		dirname_inplace(rpath);
		if (lstat(rpath,&stb)!=0) {
			printf("%s: (%s) lstat error: %s\n",name,rpath,strerr(errno));
			return -1;
		}
		pinode = stb.st_ino;
	}
	return -1;
}

void close_master_conn(int err) {
	if (current_master<0) {
		return;
	}
	if (err) {
		close(current_master);
		current_master = -1;
		current_device = 0;
	}
}

/*
int open_master_conn(const char *name,uint32_t *inode) {
	char rpath[PATH_MAX],*p;
	struct stat stb;
	int sd;
	if (realpath(name,rpath)==NULL) {
		printf("%s: realpath error\n",name);
		return -1;
	}
	p = rpath;
	if (lstat(p,&stb)!=0) {
		printf("%s: (%s) lstat error\n",name,p);
		return -1;
	}
	*inode = stb.st_ino;
	for(;;) {
		if (stb.st_ino==1) {	// found fuse root
			p = strcat(p,"/.master");
			if (lstat(p,&stb)==0) {
				if ((stb.st_ino==0x7FFFFFFF || stb.st_ino==0x7FFFFFFE) && stb.st_nlink==1 && stb.st_uid==0 && stb.st_gid==0) {
					if (stb.st_ino==0x7FFFFFFE) {	// meta master
						if (((*inode)&INODE_TYPE_MASK)!=INODE_TYPE_TRASH && ((*inode)&INODE_TYPE_MASK)!=INODE_TYPE_SUSTAINED) {
							printf("%s: only files in 'trash' and 'sustained' are usable in mfsmeta\n",name);
							return -1;
						}
						(*inode)&=INODE_VALUE_MASK;
					}
					sd = open(p,O_RDWR);
					if (master_register(sd)<0) {
						printf("%s: can't register to master\n",name);
						return -1;
					}
					return sd;
				}
			}
			printf("%s: not MFS object\n",name);
			return -1;
		}
		if (p[0]!='/' || p[1]=='\0') {
			printf("%s: not MFS object\n",name);
			return -1;
		}
		p = dirname(p);
		if (lstat(p,&stb)!=0) {
			printf("%s: (%s) lstat error\n",name,p);
			return -1;
		}
	}
	return -1;
}

int open_two_files_master_conn(const char *fname,const char *sname,uint32_t *finode,uint32_t *sinode) {
	char frpath[PATH_MAX];
	char srpath[PATH_MAX];
	int i;
	char *p;
	struct stat stb;
	int sd;
	if (realpath(fname,frpath)==NULL) {
		printf("%s: realpath error\n",fname);
		return -1;
	}
	if (realpath(sname,srpath)==NULL) {
		printf("%s: realpath error\n",sname);
		return -1;
	}
	if (lstat(frpath,&stb)!=0) {
		printf("%s: (%s) lstat error\n",fname,frpath);
		return -1;
	}
	*finode = stb.st_ino;
	if (lstat(srpath,&stb)!=0) {
		printf("%s: (%s) lstat error\n",sname,srpath);
		return -1;
	}
	*sinode = stb.st_ino;

	for (i=0 ; i<PATH_MAX && frpath[i]==srpath[i] ; i++) {}
	frpath[i]='\0';
	p = dirname(frpath);
	if (lstat(p,&stb)!=0) {
		printf("%s: lstat error\n",p);
		return -1;
	}
	for(;;) {
		if (stb.st_ino==1) {	// found fuse root
			p = strcat(p,"/.master");
			if (lstat(p,&stb)==0) {
				if ((stb.st_ino==0x7FFFFFFF || stb.st_ino==0x7FFFFFFE) && stb.st_nlink==1 && stb.st_uid==0 && stb.st_gid==0) {
					if (stb.st_ino==0x7FFFFFFE) {	// meta master
						if ((((*finode)&INODE_TYPE_MASK)!=INODE_TYPE_TRASH && ((*finode)&INODE_TYPE_MASK)!=INODE_TYPE_SUSTAINED) \
						 || (((*sinode)&INODE_TYPE_MASK)!=INODE_TYPE_TRASH && ((*sinode)&INODE_TYPE_MASK)!=INODE_TYPE_SUSTAINED)) {
							printf("%s,%s: only files in 'trash' and 'sustained' are usable in mfsmeta\n",fname,sname);
							return -1;
						}
						(*finode)&=INODE_VALUE_MASK;
						(*sinode)&=INODE_VALUE_MASK;
					}
					sd = open(p,O_RDWR);
					if (master_register(sd)<0) {
						printf("%s,%s: can't register to master\n",fname,sname);
						return -1;
					}
					return sd;
				}
			}
			printf("%s,%s: not same MFS objects\n",fname,sname);
			return -1;
		}
		if (p[0]!='/' || p[1]=='\0') {
			printf("%s,%s: not same MFS objects\n",fname,sname);
			return -1;
		}
		p = dirname(p);
		if (lstat(p,&stb)!=0) {
			printf("%s: lstat error\n",p);
			return -1;
		}
	}
	return -1;
}
*/

#if 0

#define LABELS_BUFF_SIZE ((((26+1)*MASKORGROUP)+5)*9)

static inline char* make_label_expr(char *strbuff,uint8_t labelscnt,uint32_t labelmasks[9][MASKORGROUP]) {
	uint8_t i,j;
	char *p,c;

	p = strbuff;
	for (i=0 ; i<labelscnt ; i++) {
		if (i>0) {
			*p = ' ';
			p++;
			*p = ',';
			p++;
			*p = ' ';
			p++;
		}
		*p = '[';
		p++;
		for (j=0 ; j<MASKORGROUP ; j++) {
			if (labelmasks[i][j]==0) {
				break;
			}
			if (j>0) {
				*p = '+';
				p++;
			}
			for (c='A' ; c<='Z' ; c++) {
				if (labelmasks[i][j] & (1 << (c-'A'))) {
					*p = c;
					p++;
				}
			}
		}
		if (j==0) {
			*p = '*';
			p++;
		}
		*p = ']';
		p++;
	}
	*p = '\0';
	return strbuff;
}

/* grammar productions:
 *	A -> [1-9] E ',' A | [1-9] E ';' A
 *	E -> '*' | S
 *	S -> S '+' M | S '|' M | S '||' M | M
 *	M -> M '*' L | M '&' L | M '&&' L | M L | L
 *	L -> 'a' .. 'z' | 'A' .. 'Z' | '(' S ')' | '[' S ']'
 */

enum {
	OR,
	AND,
	REF,
	ANY,
	SYM
};

typedef struct _node {
	uint8_t op;
	uint8_t val;
	struct _node *arg1;
	struct _node *arg2;
} node;

typedef struct _expr {
	const char *str;
	node *terms[9];
	uint8_t erroroccured;
} expr;


static inline void expr_rfree(node *actnode) {
	if (actnode!=NULL) {
		if (actnode->op!=REF) {
			expr_rfree(actnode->arg1);
			expr_rfree(actnode->arg2);
		}
		free(actnode);
	}
}

static inline node* newnode(uint8_t op,int8_t val,node *arg1,node *arg2) {
	node *aux;
	aux = (node*)malloc(sizeof(node));
	aux->op = op;
	aux->val = val;
	aux->arg1 = arg1;
	aux->arg2 = arg2;
	return aux;
}

static inline node* expr_or(expr *e);

static inline void expr_eat_white(expr *e) {
	while (e->str[0]==' ' || e->str[0]=='\t') {
		e->str++;
	}
}

/* L -> 'a' .. 'z' | 'A' .. 'Z' | '(' S ')' | '[' S ']' */
static inline node* expr_sym(expr *e) {
	node *a;
	uint8_t v;
	expr_eat_white(e);
	if (e->str[0]=='(') {
		e->str++;
		expr_eat_white(e);
		a=expr_or(e);
		expr_eat_white(e);
		if (e->str[0]==')') {
			e->str++;
			return a;
		} else {
			if ((int8_t)(e->str[0])>=32) {
				printf("parse error, closing round bracket expected, next char: '%c'\n",e->str[0]);
			} else {
				printf("parse error, closing round bracket expected, next code: 0x%02"PRIX8"\n",(uint8_t)(e->str[0]));
			}
			expr_rfree(a);
			e->erroroccured = 1;
			return NULL;
		}
	}
	if (e->str[0]=='[') {
		e->str++;
		expr_eat_white(e);
		a=expr_or(e);
		expr_eat_white(e);
		if (e->str[0]==']') {
			e->str++;
			return a;
		} else {
			if ((int8_t)(e->str[0])>=32) {
				printf("parse error, closing round bracket expected, next char: '%c'\n",e->str[0]);
			} else {
				printf("parse error, closing round bracket expected, next code: 0x%02"PRIX8"\n",(uint8_t)(e->str[0]));
			}
			expr_rfree(a);
			e->erroroccured = 1;
			return NULL;
		}
	}
	if (e->str[0]>='A' && e->str[0]<='Z') {
		v = e->str[0]-'A';
		e->str++;
		return newnode(SYM,v,NULL,NULL);
	}
	if (e->str[0]>='a' && e->str[0]<='z') {
		v = e->str[0]-'a';
		e->str++;
		return newnode(SYM,v,NULL,NULL);
	}
	if ((int8_t)(e->str[0])>=32) {
		printf("parse error, next char: '%c'\n",e->str[0]);
	} else {
		printf("parse error, next code: 0x%02"PRIX8"\n",(uint8_t)(e->str[0]));
	}
	e->erroroccured = 1;
	return NULL;
}

/* M -> M '*' L | M '&' L | M '&&' L | M L | L */
static inline node* expr_and(expr *e) {
	node *a;
	node *b;
	expr_eat_white(e);
	a = expr_sym(e);
	expr_eat_white(e);
	if (e->str[0]=='&' && e->str[1]=='&') {
		e->str += 2;
		b = expr_and(e);
		return newnode(AND,0,a,b);
	} else if (e->str[0]=='&' || e->str[0]=='*') {
		e->str ++;
		b = expr_and(e);
		return newnode(AND,0,a,b);	
	} else if ((e->str[0]>='A' && e->str[0]<='Z') || (e->str[0]>='a' && e->str[0]<='z') || e->str[0]=='(' || e->str[0]=='[') {
		b = expr_and(e);
		return newnode(AND,0,a,b);
	} else {
		return a;
	}
}

/* S -> S '+' M | S '|' M | S '||' M | M */
static inline node* expr_or(expr *e) {
	node *a;
	node *b;
	expr_eat_white(e);
	a = expr_and(e);
	expr_eat_white(e);
	if (e->str[0]=='|' && e->str[1]=='|') {
		e->str += 2;
		b = expr_or(e);
		return newnode(OR,0,a,b);
	} else if (e->str[0]=='|' || e->str[0]=='+') {
		e->str ++;
		b = expr_or(e);
		return newnode(OR,0,a,b);
	} else {
		return a;
	}
}

/* E -> '*' | S */
static inline node* expr_first(expr *e) {
	expr_eat_white(e);
	if (e->str[0]=='*') {
		e->str++;
		return newnode(ANY,0,NULL,NULL);
	}
	return expr_or(e);
}

/* A -> [1-9] E ',' A | [1-9] E ';' A */
static inline void expr_top(expr *e) {
	uint32_t i;
	uint32_t g;
	uint8_t f;
	node *a;

	i = 0;
	while (i<9) {
		expr_eat_white(e);
		f = 0;
		if (e->str[0]>='1' && e->str[0]<='9') {
			g = e->str[0]-'0';
			e->str++;
			f = 1;
		} else {
			g = 1;
		}
		expr_eat_white(e);
		if (i==0 && f==1 && e->str[0]==0) { // number only
			a = newnode(ANY,0,NULL,NULL);
		} else {
			a = expr_first(e);
		}
		expr_eat_white(e);
		if (e->erroroccured) {
			expr_rfree(a);
			return;
		}
		if (i+g>9) {
			break;
		}
		f = 0;
		while (g>0) {
			if (f==1) {
				e->terms[i] = newnode(REF,0,a,NULL);
			} else {
				e->terms[i] = a;
				f = 1;
			}
			i++;
			g--;
		}
		if (e->str[0]==',' || e->str[0]==';') {
			e->str++;
		} else if (e->str[0]) {
			if ((int8_t)(e->str[0])>=32) {
				printf("parse error, next char: '%c'\n",e->str[0]);
			} else {
				printf("parse error, next code: 0x%02"PRIX8"\n",(uint8_t)(e->str[0]));
			}
			e->erroroccured = 1;
			return;
		} else {
			return;
		}
	}
	printf("parse error, too many copies\n");
	e->erroroccured = 1;
	return;
}

typedef struct _termval {
	uint8_t cnt;
	uint32_t *labelmasks;
} termval;

static int label_cmp(const void *a,const void *b) {
	uint32_t aa = *((const uint32_t*)a);
	uint32_t bb = *((const uint32_t*)b);
	return (aa>bb)?1:(aa<bb)?-1:0;
}

static inline termval* expr_eval(node *a) {
	termval *t1,*t2,*t;
	uint32_t i,j;
	t1 = NULL;
	t2 = NULL;
	t = NULL;
	if (a->op==REF) {
		return expr_eval(a->arg1);
	}
	if (a->op==ANY) {
		t1 = malloc(sizeof(termval));
		t1->cnt = 0;
		t1->labelmasks = NULL;
		return t1;
	}
	if (a->op==SYM) {
		t1 = malloc(sizeof(termval));
		t1->cnt = 1;
		t1->labelmasks = malloc(sizeof(uint32_t));
		t1->labelmasks[0] = 1 << a->val;
		return t1;
	}
	if (a->op==OR || a->op==AND) {
		t1 = expr_eval(a->arg1);
		t2 = expr_eval(a->arg2);
		if (t1==NULL || t2==NULL || t1->cnt==0 || t2->cnt==0) {
			if (t1) {
				free(t1->labelmasks);
				free(t1);
			}
			if (t2) {
				free(t2->labelmasks);
				free(t2);
			}
			return NULL;
		}
		t = malloc(sizeof(termval));
	}
	if (a->op==AND) {
		t->cnt = t1->cnt*t2->cnt;
		t->labelmasks = malloc(sizeof(uint32_t)*t->cnt);
		for (i=0 ; i<t1->cnt ; i++) {
			for (j=0 ; j<t2->cnt ; j++) {
				t->labelmasks[i*t2->cnt+j] = (t1->labelmasks[i] | t2->labelmasks[j]);
			}
		}
	} else if (a->op==OR) {
		t->cnt = t1->cnt+t2->cnt;
		t->labelmasks = malloc(sizeof(uint32_t)*t->cnt);
		memcpy(t->labelmasks,t1->labelmasks,sizeof(uint32_t)*t1->cnt);
		memcpy(t->labelmasks+t1->cnt,t2->labelmasks,sizeof(uint32_t)*t2->cnt);
	} else {
		if (t) { /* satisify cppcheck */
			free(t);
		}
		return NULL;
	}
	free(t1->labelmasks);
	free(t2->labelmasks);
	free(t1);
	free(t2);
	if (t->cnt>1) {
		qsort(t->labelmasks,t->cnt,sizeof(uint32_t),label_cmp);
		for (i=0 ; i+1<t->cnt ; i++) {
			while (t->labelmasks[i]==t->labelmasks[i+1] && i+1<t->cnt) {
				if (i+2<t->cnt) {
					memmove(t->labelmasks+i+1,t->labelmasks+i+2,sizeof(uint32_t)*(t->cnt-i-2));
				}
				t->cnt--;
			}
		}
	}
	if (t->cnt > MASKORGROUP) {
		printf("Too many 'or' groups (max: %u)\n",MASKORGROUP);
		free(t->labelmasks);
		free(t);
		return NULL;
	}
	return t;
}

static inline int parse_label_expr(char *exprstr,uint8_t *labelscnt,uint32_t labelmasks[9][MASKORGROUP]) {
	expr e;
	termval *t;
	uint32_t i,j;
	int res;

	res = 0;
	e.str = exprstr;
	e.erroroccured = 0;
	for (i=0 ; i<9 ; i++) {
		e.terms[i] = NULL;
	}
	expr_top(&e);
	if (e.erroroccured) {
		res = -1;
	}
	for (i=0 ; i<9 && res==0 && e.terms[i]!=NULL ; i++) {
		t = expr_eval(e.terms[i]);
		if (t==NULL) {
			res = -1;
		} else {
			for (j=0 ; j<MASKORGROUP ; j++) {
				if (j<t->cnt) {
					labelmasks[i][j] = t->labelmasks[j];
				} else {
					labelmasks[i][j] = 0;
				}
			}
			free(t->labelmasks);
			free(t);
		}
	}
	if (res==0) {
		*labelscnt = i;
	}
	for (i=0 ; i<9 ; i++) {
		expr_rfree(e.terms[i]);
	}
	return res;
}
#endif

// formats:
//  #     - number of seconds
//  #s    - number of seconds
//  #.#m  - number of minutes
//  #.#h  - number of hours
//  #.#d  - number of days
//  #.#w  - number of weeks
static inline uint32_t parse_period(char *str,char **endpos) {
	double base;
	double divisor;
	base = 0.0;
	while ((*str)>='0' && (*str)<='9') {
		base *= 10.0;
		base += (*str)-'0';
		str++;
	}
	if ((*str)=='.') {
		divisor = 0.1;
		str++;
		while ((*str)>='0' && (*str)<='9') {
			base += ((*str)-'0')*divisor;
			divisor /= 10.0;
		}
	}
	while ((*str)==' ') {
		str++;
	}
	if ((*str)=='s') {
		str++;
	} else if ((*str)=='m') {
		str++;
		base *= 60.0;
	} else if ((*str)=='h') {
		str++;
		base *= 3600.0;
	} else if ((*str)=='d') {
		str++;
		base *= 86400.0;
	} else if ((*str)=='w') {
		str++;
		base *= 604800.0;
	}
	*endpos = str;
	if (base >= UINT32_MAX) {
		return UINT32_MAX;
	}
	if (base <= 0) {
		return 0;
	}
	return base;
}

int file_paths(const char* fname) {
	uint8_t reqbuff[16],*wptr,*buff;
	const uint8_t *rptr;
	const char *p;
	struct stat st;
	char cwdbuff[MAXPATHLEN];
	uint32_t arginode;
	uint32_t cmd,leng,inode;
	uint32_t pleng;
	int fd;

	p = fname;
	while (*p>='0' && *p<='9') {
		p++;
	}

	if (*p=='\0' && stat(fname,&st)<0 && errno==ENOENT) {
		arginode = strtoul(fname,NULL,10);
		p = getcwd(cwdbuff,MAXPATHLEN);
		fd = open_master_conn(p,&inode,NULL,NULL,0,0);
		inode = arginode;
	} else {
		fd = open_master_conn(fname,&inode,NULL,NULL,0,0);
	}
	if (fd<0) {
		return -1;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_PATHS);
	put32bit(&wptr,8);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	if (tcpwrite(fd,reqbuff,16)!=16) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_PATHS) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	}
	printf("%s:\n",fname);
	while (leng>=4) {
		pleng = get32bit(&rptr);
		leng-=4;
		if (leng>=pleng) {
			while (pleng) {
				putchar(get8bit(&rptr));
				pleng--;
				leng--;
			}
			putchar('\n');
		} else {
			leng=0;
		}
	}
	free(buff);
	return 0;
}

/* - code moved to file_info
int check_file(const char* fname) {
	uint8_t reqbuff[16],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode;
	uint8_t copies;
	uint32_t chunks;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,0);
	if (fd<0) {
		return -1;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_CHECK);
	put32bit(&wptr,8);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	if (tcpwrite(fd,reqbuff,16)!=16) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_CHECK) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	} else if (leng%3!=0 && leng!=44) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	}
	printf("%s:\n",fname);
	if (leng%3==0) {
		for (cmd=0 ; cmd<leng ; cmd+=3) {
			copies = get8bit(&rptr);
			chunks = get16bit(&rptr);
			if (copies==1) {
				printf("1 copy:");
			} else {
				printf("%"PRIu8" copies:",copies);
			}
			print_number(" ","\n",chunks,1,0,1);
		}
	} else {
		for (cmd=0 ; cmd<11 ; cmd++) {
			chunks = get32bit(&rptr);
			if (chunks>0) {
				if (cmd==1) {
					printf(" chunks with 1 copy:    ");
				} else if (cmd>=10) {
					printf(" chunks with 10+ copies:");
				} else {
					printf(" chunks with %u copies:  ",cmd);
				}
				print_number(" ","\n",chunks,1,0,1);
			}
		}
	}
	free(buff);
	return 0;
}
*/

/*
int copy_goal_src(const char *fname,void **params) {
	int fd;
	open_master_conn(fname,&inode,NULL,NULL,0,0);
	if (fd<0) {
		return -1;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_GETGOAL);
	put32bit(&wptr,9);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put8bit(&wptr,GMODE_NORMAL);
	if (tcpwrite(fd,reqbuff,17)!=17) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_GETGOAL) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	} else if (leng<2) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	}
	fn = get8bit(&rptr);
	dn = get8bit(&rptr);
	if ((fn!=0 || dn!=1) && (fn!=1 || dn!=0)) {
		printf("%s: master query: wrong answer (fn,dn)\n",fname);
		free(buff);
		return -1;
	}
	goal = get8bit(&rptr);
	if (goal>0) {
		*params = malloc(1);
		wptr = *params;
		put8bit(&wptr,goal);
	} else {
		labelscnt = get8bit(&rptr);
		if (labelscnt>9 || labelscnt<1) {
			printf("%s: master query: wrong answer (labelscnt)\n",fname);
			free(buff);
			return -1;
		}
		*params = malloc(2+labelscnt*MASKORGROUP*sizeof(uint32_t));
		wptr = *params;
		put8bit(&wptr,0);
		put8bit(&wptr,labelscnt);
		memcpy(wptr,rptr,labelscnt*MASKORGROUP*sizeof(uint32_t));
	}
	cnt = get32bit(&rptr);
	if (cnt!=1) {
		printf("%s: master query: wrong answer (cnt)\n",fname);
		free(buff);
		free(*params);
		return -1;
	}
	free(buff);
	return 0;
}
*/

static inline int labels_deserialize(const uint8_t **rptr,uint8_t *create_mode,uint8_t *create_labelscnt,uint32_t create_labelmasks[9][MASKORGROUP],uint8_t *keep_labelscnt,uint32_t keep_labelmasks[9][MASKORGROUP],uint8_t *arch_labelscnt,uint32_t arch_labelmasks[9][MASKORGROUP],uint16_t *arch_delay) {
	uint8_t lc,og;
	if (masterversion>=VERSION2INT(3,0,9)) {
		*create_mode = get8bit(rptr);
		*arch_delay = get16bit(rptr);
		*create_labelscnt = get8bit(rptr);
		*keep_labelscnt = get8bit(rptr);
		*arch_labelscnt = get8bit(rptr);
		if (*create_labelscnt>9 || *create_labelscnt<1 || *keep_labelscnt>9 || *keep_labelscnt<1 || *arch_labelscnt>9 || *arch_labelscnt<1) {
			return -1;
		}
		for (lc=0 ; lc<*create_labelscnt ; lc++) {
			for (og=0 ; og<MASKORGROUP ; og++) {
				create_labelmasks[lc][og] = get32bit(rptr);
			}
		}
		for (lc=0 ; lc<*keep_labelscnt ; lc++) {
			for (og=0 ; og<MASKORGROUP ; og++) {
				keep_labelmasks[lc][og] = get32bit(rptr);
			}
		}
		for (lc=0 ; lc<*arch_labelscnt ; lc++) {
			for (og=0 ; og<MASKORGROUP ; og++) {
				arch_labelmasks[lc][og] = get32bit(rptr);
			}
		}
	} else {
		*create_mode = CREATE_MODE_STD;
		*arch_delay = 0;
		*create_labelscnt = get8bit(rptr);
		if (*create_labelscnt>9 || *create_labelscnt<1) {
			return -1;
		}
		*keep_labelscnt = *create_labelscnt;
		*arch_labelscnt = *create_labelscnt;
		for (lc=0 ; lc<*create_labelscnt ; lc++) {
			for (og=0 ; og<MASKORGROUP ; og++) {
				create_labelmasks[lc][og] = keep_labelmasks[lc][og] = arch_labelmasks[lc][og] = get32bit(rptr);
			}
		}
	}
	return 0;
}

void printf_goal(uint8_t create_mode,uint8_t create_labelscnt,uint32_t create_labelmasks[9][MASKORGROUP],uint8_t keep_labelscnt,uint32_t keep_labelmasks[9][MASKORGROUP],uint8_t arch_labelscnt,uint32_t arch_labelmasks[9][MASKORGROUP],uint16_t arch_delay,char *endstr) {
	char create_labelsbuff[LABELS_BUFF_SIZE];
	char keep_labelsbuff[LABELS_BUFF_SIZE];
	char arch_labelsbuff[LABELS_BUFF_SIZE];
	if (arch_delay==0) {
		if (create_labelscnt==keep_labelscnt) {
			printf("%"PRIu8" ; create_mode: %s ; create_labels: %s ; keep_labels: %s%s",create_labelscnt,(create_mode==CREATE_MODE_LOOSE)?"LOOSE":(create_mode==CREATE_MODE_STRICT)?"STRICT":"STD",make_label_expr(create_labelsbuff,create_labelscnt,create_labelmasks),make_label_expr(keep_labelsbuff,keep_labelscnt,keep_labelmasks),endstr);
		} else {
			printf("%"PRIu8"->%"PRIu8" ; create_mode: %s ; create_labels: %s ; keep_labels: %s%s",create_labelscnt,keep_labelscnt,(create_mode==CREATE_MODE_LOOSE)?"LOOSE":(create_mode==CREATE_MODE_STRICT)?"STRICT":"STD",make_label_expr(create_labelsbuff,create_labelscnt,create_labelmasks),make_label_expr(keep_labelsbuff,keep_labelscnt,keep_labelmasks),endstr);
		}
	} else {
		if (create_labelscnt==keep_labelscnt && keep_labelscnt==arch_labelscnt) {
			printf("%"PRIu8" ; create_mode: %s ; create_labels: %s ; keep_labels: %s ; arch_labels: %s ; arch_delay: %"PRIu16"d%s",create_labelscnt,(create_mode==CREATE_MODE_LOOSE)?"LOOSE":(create_mode==CREATE_MODE_STRICT)?"STRICT":"STD",make_label_expr(create_labelsbuff,create_labelscnt,create_labelmasks),make_label_expr(keep_labelsbuff,keep_labelscnt,keep_labelmasks),make_label_expr(arch_labelsbuff,arch_labelscnt,arch_labelmasks),arch_delay,endstr);
		} else {
			printf("%"PRIu8"->%"PRIu8"->%"PRIu8" ; create_mode: %s ; create_labels: %s ; keep_labels: %s ; arch_labels: %s ; arch_delay: %"PRIu16"d%s",create_labelscnt,keep_labelscnt,arch_labelscnt,(create_mode==CREATE_MODE_LOOSE)?"LOOSE":(create_mode==CREATE_MODE_STRICT)?"STRICT":"STD",make_label_expr(create_labelsbuff,create_labelscnt,create_labelmasks),make_label_expr(keep_labelsbuff,keep_labelscnt,keep_labelmasks),make_label_expr(arch_labelsbuff,arch_labelscnt,arch_labelmasks),arch_delay,endstr);
		}
	}
}

int get_goal(const char *fname,uint8_t *goal,uint8_t *create_mode,uint8_t *create_labelscnt,uint32_t create_labelmasks[9][MASKORGROUP],uint8_t *keep_labelscnt,uint32_t keep_labelmasks[9][MASKORGROUP],uint8_t *arch_labelscnt,uint32_t arch_labelmasks[9][MASKORGROUP],uint16_t *arch_delay,uint8_t mode) {
	uint8_t reqbuff[17],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode;
	uint8_t fn,dn,i;
//	uint8_t goal;
//	uint8_t labelscnt;
//	uint32_t labelmasks[9][MASKORGROUP];
	uint32_t cnt;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,0);
	if (fd<0) {
		return -1;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_GETGOAL);
	put32bit(&wptr,9);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put8bit(&wptr,mode);
	if (tcpwrite(fd,reqbuff,17)!=17) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_GETGOAL) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	} else if (leng<2) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	}
	if (mode==GMODE_NORMAL) {
		fn = get8bit(&rptr);
		dn = get8bit(&rptr);
		if ((fn!=0 || dn!=1) && (fn!=1 || dn!=0)) {
			printf("%s: master query: wrong answer (fn,dn)\n",fname);
			free(buff);
			return -1;
		}
		*goal = get8bit(&rptr);
		if (*goal==0) {
			if (labels_deserialize(&rptr,create_mode,create_labelscnt,create_labelmasks,keep_labelscnt,keep_labelmasks,arch_labelscnt,arch_labelmasks,arch_delay)<0) {
				printf("%s: master query: wrong answer (labels)\n",fname);
				free(buff);
				return -1;
			}
		}
		cnt = get32bit(&rptr);
		if (cnt!=1) {
			printf("%s: master query: wrong answer (cnt)\n",fname);
			free(buff);
			return -1;
		}
		if (*goal==0) {
			printf("%s: ",fname);
			printf_goal(*create_mode,*create_labelscnt,create_labelmasks,*keep_labelscnt,keep_labelmasks,*arch_labelscnt,arch_labelmasks,*arch_delay,"\n");
		} else {
			printf("%s: %"PRIu8"\n",fname,*goal);
		}
	} else {
		fn = get8bit(&rptr);
		dn = get8bit(&rptr);
		printf("%s:\n",fname);
		for (i=0 ; i<fn ; i++) {
			*goal = get8bit(&rptr);
			if (*goal==0) {
				if (labels_deserialize(&rptr,create_mode,create_labelscnt,create_labelmasks,keep_labelscnt,keep_labelmasks,arch_labelscnt,arch_labelmasks,arch_delay)<0) {
					printf("%s: master query: wrong answer (labels)\n",fname);
					free(buff);
					return -1;
				}
			}
			cnt = get32bit(&rptr);
			if (*goal==0) {
				printf(" files with goal        ");
				printf_goal(*create_mode,*create_labelscnt,create_labelmasks,*keep_labelscnt,keep_labelmasks,*arch_labelscnt,arch_labelmasks,*arch_delay," :");
			} else {
				printf(" files with goal        %"PRIu8" :",*goal);
			}
			print_number(" ","\n",cnt,1,0,1);
		}
		for (i=0 ; i<dn ; i++) {
			*goal = get8bit(&rptr);
			if (*goal==0) {
				if (labels_deserialize(&rptr,create_mode,create_labelscnt,create_labelmasks,keep_labelscnt,keep_labelmasks,arch_labelscnt,arch_labelmasks,arch_delay)<0) {
					printf("%s: master query: wrong answer (labels)\n",fname);
					free(buff);
					return -1;
				}
			}
			cnt = get32bit(&rptr);
			if (*goal==0) {
				printf(" directories with goal  ");
				printf_goal(*create_mode,*create_labelscnt,create_labelmasks,*keep_labelscnt,keep_labelmasks,*arch_labelscnt,arch_labelmasks,*arch_delay," :");
			} else {
				printf(" directories with goal  %"PRIu8" :",*goal);
			}
			print_number(" ","\n",cnt,1,0,1);
		}
	}
	free(buff);
	return 0;
}

int get_trashtime(const char *fname,uint32_t *trashtime,uint8_t mode) {
	uint8_t reqbuff[17],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode;
	uint32_t fn,dn,i;
//	uint32_t trashtime;
	uint32_t cnt;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,0);
	if (fd<0) {
		return -1;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_GETTRASHTIME);
	put32bit(&wptr,9);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put8bit(&wptr,mode);
	if (tcpwrite(fd,reqbuff,17)!=17) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_GETTRASHTIME) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	} else if (leng<8 || leng%8!=0) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	} else if (mode==GMODE_NORMAL && leng!=16) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	}
	if (mode==GMODE_NORMAL) {
		fn = get32bit(&rptr);
		dn = get32bit(&rptr);
		*trashtime = get32bit(&rptr);
		cnt = get32bit(&rptr);
		if ((fn!=0 || dn!=1) && (fn!=1 || dn!=0)) {
			printf("%s: master query: wrong answer (fn,dn)\n",fname);
			free(buff);
			return -1;
		}
		if (cnt!=1) {
			printf("%s: master query: wrong answer (cnt)\n",fname);
			free(buff);
			return -1;
		}
		printf("%s: %"PRIu32"\n",fname,*trashtime);
	} else {
		fn = get32bit(&rptr);
		dn = get32bit(&rptr);
		printf("%s:\n",fname);
		for (i=0 ; i<fn ; i++) {
			*trashtime = get32bit(&rptr);
			cnt = get32bit(&rptr);
			printf(" files with trashtime        %10"PRIu32" :",*trashtime);
			print_number(" ","\n",cnt,1,0,1);
		}
		for (i=0 ; i<dn ; i++) {
			*trashtime = get32bit(&rptr);
			cnt = get32bit(&rptr);
			printf(" directories with trashtime  %10"PRIu32" :",*trashtime);
			print_number(" ","\n",cnt,1,0,1);
		}
	}
	free(buff);
	return 0;
}

int get_eattr(const char *fname,uint8_t *eattr,uint8_t mode) {
	uint8_t reqbuff[17],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode;
	uint8_t fn,dn,i,j;
	uint32_t fcnt[EATTR_BITS];
	uint32_t dcnt[EATTR_BITS];
//	uint8_t eattr;
	uint32_t cnt;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,0);
	if (fd<0) {
		return -1;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_GETEATTR);
	put32bit(&wptr,9);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put8bit(&wptr,mode);
	if (tcpwrite(fd,reqbuff,17)!=17) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_GETEATTR) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	} else if (leng%5!=2) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	} else if (mode==GMODE_NORMAL && leng!=7) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	}
	if (mode==GMODE_NORMAL) {
		fn = get8bit(&rptr);
		dn = get8bit(&rptr);
		*eattr = get8bit(&rptr);
		cnt = get32bit(&rptr);
		if ((fn!=0 || dn!=1) && (fn!=1 || dn!=0)) {
			printf("%s: master query: wrong answer (fn,dn)\n",fname);
			free(buff);
			return -1;
		}
		if (cnt!=1) {
			printf("%s: master query: wrong answer (cnt)\n",fname);
			free(buff);
			return -1;
		}
		printf("%s: ",fname);
		if (*eattr>0) {
			cnt=0;
			for (j=0 ; j<EATTR_BITS ; j++) {
				if ((*eattr) & (1<<j)) {
					printf("%s%s",(cnt)?",":"",eattrtab[j]);
					cnt=1;
				}
			}
			printf("\n");
		} else {
			printf("-\n");
		}
	} else {
		for (j=0 ; j<EATTR_BITS ; j++) {
			fcnt[j]=0;
			dcnt[j]=0;
		}
		fn = get8bit(&rptr);
		dn = get8bit(&rptr);
		for (i=0 ; i<fn ; i++) {
			*eattr = get8bit(&rptr);
			cnt = get32bit(&rptr);
			for (j=0 ; j<EATTR_BITS ; j++) {
				if ((*eattr) & (1<<j)) {
					fcnt[j]+=cnt;
				}
			}
		}
		for (i=0 ; i<dn ; i++) {
			*eattr = get8bit(&rptr);
			cnt = get32bit(&rptr);
			for (j=0 ; j<EATTR_BITS ; j++) {
				if ((*eattr) & (1<<j)) {
					dcnt[j]+=cnt;
				}
			}
		}
		printf("%s:\n",fname);
		for (j=0 ; j<EATTR_BITS ; j++) {
			if (eattrtab[j][0]) {
				printf(" not directory nodes with attribute %16s :",eattrtab[j]);
				print_number(" ","\n",fcnt[j],1,0,1);
				printf(" directories with attribute         %16s :",eattrtab[j]);
				print_number(" ","\n",dcnt[j],1,0,1);
			} else {
				if (fcnt[j]>0) {
					printf(" not directory nodes with attribute      'unknown-%u' :",j);
					print_number(" ","\n",fcnt[j],1,0,1);
				}
				if (dcnt[j]>0) {
					printf(" directories with attribute              'unknown-%u' :",j);
					print_number(" ","\n",dcnt[j],1,0,1);
				}
			}
		}
	}
	free(buff);
	return 0;
}

int set_goal(const char *fname,uint8_t goal,uint8_t create_mode,uint8_t create_labelscnt,uint32_t create_labelmasks[9][MASKORGROUP],uint8_t keep_labelscnt,uint32_t keep_labelmasks[9][MASKORGROUP],uint8_t arch_labelscnt,uint32_t arch_labelmasks[9][MASKORGROUP],uint16_t arch_delay,uint8_t mode) {
	uint8_t reqbuff[28+3*9*4*MASKORGROUP],*wptr,*buff;
	const uint8_t *rptr;
	int32_t rleng;
	uint32_t cmd,leng,inode,uid;
	uint32_t changed,notchanged,notpermitted,quotaexceeded;
	uint8_t i,og;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,1);
	if (fd<0) {
		return -1;
	}
	uid = getuid();
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_SETGOAL);
	if (goal>0) {
		rleng = 14;
	} else {
		if (masterversion<VERSION2INT(2,1,0)) {
			printf("%s: labels not supported (master too old)\n",fname);
			close_master_conn(0);
			return -1;
		}
		mode = (mode&SMODE_RMASK) | SMODE_LABELS;
		if (masterversion<VERSION2INT(3,0,9)) {
			rleng = 14+keep_labelscnt*4*MASKORGROUP;
		} else {
			rleng = 20+(create_labelscnt+keep_labelscnt+arch_labelscnt)*4*MASKORGROUP;
		}
	}
	put32bit(&wptr,rleng);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	if (goal>0) {
		put8bit(&wptr,goal);
	} else {
		if (masterversion<VERSION2INT(3,0,9)) {
			put8bit(&wptr,keep_labelscnt);
		} else {
			put8bit(&wptr,0);
		}
	}
	put8bit(&wptr,mode);
	if (goal==0) {
		if (masterversion<VERSION2INT(3,0,9)) {
			for (i=0 ; i<keep_labelscnt ; i++) {
				for (og=0 ; og<MASKORGROUP ; og++) {
					put32bit(&wptr,keep_labelmasks[i][og]);
				}
			}
		} else {
			put8bit(&wptr,create_mode);
			put16bit(&wptr,arch_delay);
			put8bit(&wptr,create_labelscnt);
			put8bit(&wptr,keep_labelscnt);
			put8bit(&wptr,arch_labelscnt);
			for (i=0 ; i<create_labelscnt ; i++) {
				for (og=0 ; og<MASKORGROUP ; og++) {
					put32bit(&wptr,create_labelmasks[i][og]);
				}
			}
			for (i=0 ; i<keep_labelscnt ; i++) {
				for (og=0 ; og<MASKORGROUP ; og++) {
					put32bit(&wptr,keep_labelmasks[i][og]);
				}
			}
			for (i=0 ; i<arch_labelscnt ; i++) {
				for (og=0 ; og<MASKORGROUP ; og++) {
					put32bit(&wptr,arch_labelmasks[i][og]);
				}
			}
		}
	}
	rleng += 8;
	if (tcpwrite(fd,reqbuff,rleng)!=rleng) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_SETGOAL) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	} else if (leng!=12 && leng!=16) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	}
	changed = get32bit(&rptr);
	notchanged = get32bit(&rptr);
	notpermitted = get32bit(&rptr);
	if (leng==16) {
		quotaexceeded = get32bit(&rptr);
	} else {
		quotaexceeded = 0;
	}
	if ((mode&SMODE_RMASK)==0) {
		if (changed || mode==SMODE_SET || mode==SMODE_LABELS) {
			if (mode==SMODE_LABELS) {
				printf("%s: ",fname);
				printf_goal(create_mode,create_labelscnt,create_labelmasks,keep_labelscnt,keep_labelmasks,arch_labelscnt,arch_labelmasks,arch_delay,"\n");
			} else {
				printf("%s: %"PRIu8"\n",fname,goal);
			}
		} else {
			printf("%s: goal not changed\n",fname);
		}
	} else {
		printf("%s:\n",fname);
		print_number(" inodes with goal changed:      ","\n",changed,1,0,1);
		print_number(" inodes with goal not changed:  ","\n",notchanged,1,0,1);
		print_number(" inodes with permission denied: ","\n",notpermitted,1,0,1);
		if (leng==16) {
			print_number(" inodes with quota exceeded:    ","\n",quotaexceeded,1,0,1);
		}
	}
	free(buff);
	return 0;
}

int set_trashtime(const char *fname,uint32_t trashtime,uint8_t mode) {
	uint8_t reqbuff[25],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode,uid;
	uint32_t changed,notchanged,notpermitted;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,1);
	if (fd<0) {
		return -1;
	}
	uid = getuid();
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_SETTRASHTIME);
	put32bit(&wptr,17);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	put32bit(&wptr,trashtime);
	put8bit(&wptr,mode);
	if (tcpwrite(fd,reqbuff,25)!=25) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_SETTRASHTIME) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	} else if (leng!=12) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	}
	changed = get32bit(&rptr);
	notchanged = get32bit(&rptr);
	notpermitted = get32bit(&rptr);
	if ((mode&SMODE_RMASK)==0) {
		if (changed || mode==SMODE_SET) {
			printf("%s: %"PRIu32"\n",fname,trashtime);
		} else {
			printf("%s: trashtime not changed\n",fname);
		}
	} else {
		printf("%s:\n",fname);
		print_number(" inodes with trashtime changed:     ","\n",changed,1,0,1);
		print_number(" inodes with trashtime not changed: ","\n",notchanged,1,0,1);
		print_number(" inodes with permission denied:     ","\n",notpermitted,1,0,1);
	}
	free(buff);
	return 0;
}

int set_eattr(const char *fname,uint8_t eattr,uint8_t mode) {
	uint8_t reqbuff[22],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode,uid;
	uint32_t changed,notchanged,notpermitted;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,1);
	if (fd<0) {
		return -1;
	}
	uid = getuid();
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_SETEATTR);
	put32bit(&wptr,14);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	put8bit(&wptr,eattr);
	put8bit(&wptr,mode);
	if (tcpwrite(fd,reqbuff,22)!=22) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_SETEATTR) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	} else if (leng!=12) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	}
	changed = get32bit(&rptr);
	notchanged = get32bit(&rptr);
	notpermitted = get32bit(&rptr);
	if ((mode&SMODE_RMASK)==0) {
		if (changed) {
			printf("%s: attribute(s) changed\n",fname);
		} else {
			printf("%s: attribute(s) not changed\n",fname);
		}
	} else {
		printf("%s:\n",fname);
		print_number(" inodes with attributes changed:     ","\n",changed,1,0,1);
		print_number(" inodes with attributes not changed: ","\n",notchanged,1,0,1);
		print_number(" inodes with permission denied:      ","\n",notpermitted,1,0,1);
	}
	free(buff);
	return 0;
}

int archive_control(const char *fname,uint8_t archcmd) {
	uint8_t reqbuff[21],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode,uid;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,1);
	if (fd<0) {
		return -1;
	}
	uid = getuid();
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_ARCHCTL);
	put32bit(&wptr,archcmd==ARCHCTL_GET?9:13);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put8bit(&wptr,archcmd);
	if (archcmd==ARCHCTL_GET) {
		if (tcpwrite(fd,reqbuff,17)!=17) {
			printf("%s: master query: send error\n",fname);
			close_master_conn(1);
			return -1;
		}
	} else {
		put32bit(&wptr,uid);
		if (tcpwrite(fd,reqbuff,21)!=21) {
			printf("%s: master query: send error\n",fname);
			close_master_conn(1);
			return -1;
		}
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_ARCHCTL) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	}
	if (archcmd==ARCHCTL_GET) {
		uint32_t archinodes,partinodes,notarchinodes;
		uint64_t archchunks,notarchchunks;
		if (leng!=28) {
			printf("%s: master query: wrong answer (leng)\n",fname);
			free(buff);
			return -1;
		}
		archchunks = get64bit(&rptr);
		notarchchunks = get64bit(&rptr);
		archinodes = get32bit(&rptr);
		partinodes = get32bit(&rptr);
		notarchinodes = get32bit(&rptr);
		if (archinodes+partinodes+notarchinodes==1) {
			if (archinodes==1) {
				printf("%s: all chunks are archived\n",fname);
			} else if (notarchinodes==1) {
				printf("%s: all chunks are not archived\n",fname);
			} else {
				printf("%s: file is partially archived (archived chunks: %"PRIu64" ; not archived chunks: %"PRIu64")\n",fname,archchunks,notarchchunks);
			}
		} else {
			printf("%s:\n",fname);
			print_number(" files with all chunks archived:     ","\n",archinodes,1,0,1);
			print_number(" files with all chunks not archived: ","\n",notarchinodes,1,0,1);
			print_number(" files partially archived:           ","\n",partinodes,1,0,1);
			print_number(" archived chunks:                    ","\n",archchunks,1,0,1);
			print_number(" not archived chunks:                ","\n",notarchchunks,1,0,1);
		}
	} else {
		uint64_t changed,notchanged;
		uint32_t notpermitted;
		if (leng!=20) {
			printf("%s: master query: wrong answer (leng)\n",fname);
			free(buff);
			return -1;
		}
		changed = get64bit(&rptr);
		notchanged = get64bit(&rptr);
		notpermitted = get32bit(&rptr);
		printf("%s:\n",fname);
		print_number(" chunks changed:               ","\n",changed,1,0,1);
		print_number(" chunks not changed:           ","\n",notchanged,1,0,1);
		print_number(" files with permission denied: ","\n",notpermitted,1,0,1);
	}
	free(buff);
	return 0;
}

int ip_port_cmp(const void*a,const void*b) {
	return memcmp(a,b,6);
}

int get_checksum_block(const char *csstrip,uint32_t csip,uint16_t csport,uint64_t chunkid,uint32_t version,uint8_t crcblock[4096]) {
	uint8_t reqbuff[20],*wptr,*buff;
	const uint8_t *rptr;
	int fd;
	uint32_t cmd,leng;
	uint16_t cnt;

	cnt=0;
	while (cnt<10) {
		fd = tcpsocket();
		if (fd<0) {
			printf("can't create connection socket: %s\n",strerr(errno));
			return -1;
		}
		if (tcpnumtoconnect(fd,csip,csport,(cnt%2)?(300*(1<<(cnt>>1))):(200*(1<<(cnt>>1))))<0) {
			cnt++;
			if (cnt==10) {
				printf("can't connect to chunkserver %s:%"PRIu16": %s\n",csstrip,csport,strerr(errno));
				return -1;
			}
			tcpclose(fd);
		} else {
			cnt=10;
		}
	}
	wptr = reqbuff;
	put32bit(&wptr,ANTOCS_GET_CHUNK_CHECKSUM_TAB);
	put32bit(&wptr,12);
	put64bit(&wptr,chunkid);
	put32bit(&wptr,version);
	if (tcpwrite(fd,reqbuff,20)!=20) {
		printf("%s:%"PRIu16": cs query: send error\n",csstrip,csport);
		tcpclose(fd);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s:%"PRIu16" cs query: receive error\n",csstrip,csport);
		tcpclose(fd);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=CSTOAN_CHUNK_CHECKSUM_TAB) {
		printf("%s:%"PRIu16" cs query: wrong answer (type)\n",csstrip,csport);
		tcpclose(fd);
		return -1;
	}
	if (leng!=13 && leng!=(4096+12)) {
		printf("%s:%"PRIu16" cs query: wrong answer (size)\n",csstrip,csport);
		tcpclose(fd);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s:%"PRIu16" cs query: receive error\n",csstrip,csport);
		free(buff);
		tcpclose(fd);
		return -1;
	}
	tcpclose(fd);
	rptr = buff;
	if (chunkid!=get64bit(&rptr)) {
		printf("%s:%"PRIu16" cs query: wrong answer (chunkid)\n",csstrip,csport);
		free(buff);
		return -1;
	}
	if (version!=get32bit(&rptr)) {
		printf("%s:%"PRIu16" cs query: wrong answer (version)\n",csstrip,csport);
		free(buff);
		return -1;
	}
	leng-=12;
	if (leng==1) {
		printf("%s:%"PRIu16" cs query error: %s\n",csstrip,csport,mfsstrerr(*rptr));
		free(buff);
		return -1;
	}
	memcpy(crcblock,rptr,4096);
	free(buff);
	return 0;
}

void digest_to_str(char strdigest[33],uint8_t digest[16]) {
	uint32_t i;
	for (i=0 ; i<16 ; i++) {
		snprintf(strdigest+2*i,3,"%02X",digest[i]);
	}
	strdigest[32]='\0';
}

int file_info(uint8_t fileinfomode,const char *fname) {
	uint8_t reqbuff[21],*wptr,*buff;
	const uint8_t *rptr;
	int32_t rleng;
	uint32_t fchunks;
	uint8_t fchunksvalid;
	uint32_t indx,cmd,leng,inode,version;
	uint32_t chunks,copies,vcopies,copy;
	char *strtype;
	char csstrip[16];
	uint32_t csip;
	uint16_t csport;
	uint8_t protover;
	uint64_t chunkid;
	uint64_t fleng;
	uint8_t crcblock[4096];
	md5ctx filectx,chunkctx;
	uint8_t chunkdigest[16],currentdigest[16];
	uint8_t firstdigest;
	uint8_t checksumerror;
	char strdigest[33];
	int fd;
	fd = open_master_conn(fname,&inode,NULL,&fleng,0,0);
	if (fd<0) {
		return -1;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_CHECK);
	put32bit(&wptr,8);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	if (tcpwrite(fd,reqbuff,16)!=16) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_CHECK) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	} else if ((leng%3!=0 || leng>33) && leng!=44 && leng!=48) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	}
	if (fileinfomode&FILEINFO_QUICK) {
		printf("%s:\n",fname);
	}
	fchunks = 0;
	fchunksvalid = 0;
	if (leng%3==0 && leng<=33) {
		for (cmd=0 ; cmd<leng ; cmd+=3) {
			copies = get8bit(&rptr);
			chunks = get16bit(&rptr);
			if (fileinfomode&FILEINFO_QUICK) {
				if (copies==1) {
					printf("1 copy:");
				} else {
					printf("%"PRIu32" copies:",copies);
				}
				print_number(" ","\n",chunks,1,0,1);
			}
			fchunks += chunks;
		}
	} else {
		for (cmd=0 ; cmd<11 ; cmd++) {
			chunks = get32bit(&rptr);
			if (chunks>0 && (fileinfomode&FILEINFO_QUICK)) {
				if (cmd==1) {
					printf(" chunks with 1 copy:    ");
				} else if (cmd>=10) {
					printf(" chunks with 10+ copies:");
				} else {
					printf(" chunks with %u copies:  ",cmd);
				}
				print_number(" ","\n",chunks,1,0,1);
			}
			fchunks += chunks;
		}
		if (leng==48) {
			chunks = get32bit(&rptr);
			if (chunks>0 && (fileinfomode&FILEINFO_QUICK)) {
				printf(" empty (zero) chunks:   ");
				print_number(" ","\n",chunks,1,0,1);
			}
			fchunks += chunks;
			fchunksvalid = 1;
		}
	}
	free(buff);
	if ((fileinfomode&FILEINFO_QUICK)==0) {
		if (fchunksvalid==0) { // in this case fchunks doesn't include 'empty' chunks, so use file size to fix 'fchunks' if necessary
			if (fchunks < ((fleng+MFSCHUNKMASK)>>MFSCHUNKBITS)) {
				fchunks = ((fleng+MFSCHUNKMASK)>>MFSCHUNKBITS);
			}
		}
//	printf("masterversion: %08X\n",masterversion);
		if (fileinfomode&FILEINFO_SIGNATURE) {
			md5_init(&filectx);
		}
		printf("%s:\n",fname);
		if (fchunks==0) {
			printf("\tno chunks - empty file\n");
		}
		for (indx=0 ; indx<fchunks ; indx++) {
			wptr = reqbuff;
			if (masterversion<VERSION2INT(3,0,26)) {
				put32bit(&wptr,CLTOMA_FUSE_READ_CHUNK);
				if (masterversion>VERSION2INT(3,0,3)) {
					rleng = 21;
					put32bit(&wptr,13);
				} else {
					rleng = 20;
					put32bit(&wptr,12);
				}
				put32bit(&wptr,0);
				put32bit(&wptr,inode);
				put32bit(&wptr,indx);
				if (masterversion>VERSION2INT(3,0,3)) {
					put8bit(&wptr,0); // canmodatime
				}
				if (tcpwrite(fd,reqbuff,rleng)!=rleng) {
					printf("%s [%"PRIu32"]: master query: send error\n",fname,indx);
					close_master_conn(1);
					return -1;
				}
				if (tcpread(fd,reqbuff,8)!=8) {
					printf("%s [%"PRIu32"]: master query: receive error\n",fname,indx);
					close_master_conn(1);
					return -1;
				}
				rptr = reqbuff;
				cmd = get32bit(&rptr);
				leng = get32bit(&rptr);
				if (cmd!=MATOCL_FUSE_READ_CHUNK) {
					printf("%s [%"PRIu32"]: master query: wrong answer (type)\n",fname,indx);
					close_master_conn(1);
					return -1;
				}
				buff = malloc(leng);
				if (tcpread(fd,buff,leng)!=(int32_t)leng) {
					printf("%s [%"PRIu32"]: master query: receive error\n",fname,indx);
					free(buff);
					close_master_conn(1);
					return -1;
				}
				rptr = buff;
				cmd = get32bit(&rptr);	// queryid
				if (cmd!=0) {
					printf("%s [%"PRIu32"]: master query: wrong answer (queryid)\n",fname,indx);
					free(buff);
					close_master_conn(1);
					return -1;
				}
				leng-=4;
				if (leng==1) {
					printf("%s [%"PRIu32"]: %s\n",fname,indx,mfsstrerr(*rptr));
					free(buff);
					close_master_conn(1);
					return -1;
				} else if (leng&1) {
					protover = get8bit(&rptr);
					if (protover!=1 && protover!=2) {
						printf("%s [%"PRIu32"]: master query: unknown protocol id (%"PRIu8")\n",fname,indx,protover);
						free(buff);
						close_master_conn(1);
						return -1;
					}
					if (leng<21 || (protover==1 && ((leng-21)%10)!=0) || (protover==2 && ((leng-21)%14)!=0)) {
						printf("%s [%"PRIu32"]: master query: wrong answer (leng)\n",fname,indx);
						free(buff);
						close_master_conn(1);
						return -1;
					}
				} else {
					if (leng<20 || ((leng-20)%6)!=0) {
						printf("%s [%"PRIu32"]: master query: wrong answer (leng)\n",fname,indx);
						free(buff);
						close_master_conn(1);
						return -1;
					}
					protover = 0;
				}
				rptr += 8; // fleng
				if (protover==2) {
					copies = (leng-21)/14;
				} else if (protover==1) {
					copies = (leng-21)/10;
				} else {
					copies = (leng-20)/6;
				}
			} else {
				put32bit(&wptr,CLTOMA_FUSE_CHECK);
				put32bit(&wptr,12);
				put32bit(&wptr,0);
				put32bit(&wptr,inode);
				put32bit(&wptr,indx);
				if (tcpwrite(fd,reqbuff,20)!=20) {
					printf("%s [%"PRIu32"]: master query: send error\n",fname,indx);
					close_master_conn(1);
					return -1;
				}
				if (tcpread(fd,reqbuff,8)!=8) {
					printf("%s [%"PRIu32"]: master query: receive error\n",fname,indx);
					close_master_conn(1);
					return -1;
				}
				rptr = reqbuff;
				cmd = get32bit(&rptr);
				leng = get32bit(&rptr);
				if (cmd!=MATOCL_FUSE_CHECK) {
					printf("%s [%"PRIu32"]: master query: wrong answer (type)\n",fname,indx);
					close_master_conn(1);
					return -1;
				}
				buff = malloc(leng);
				if (tcpread(fd,buff,leng)!=(int32_t)leng) {
					printf("%s [%"PRIu32"]: master query: receive error\n",fname,indx);
					free(buff);
					close_master_conn(1);
					return -1;
				}
				rptr = buff;
				cmd = get32bit(&rptr);	// queryid
				if (cmd!=0) {
					printf("%s [%"PRIu32"]: master query: wrong answer (queryid)\n",fname,indx);
					free(buff);
					close_master_conn(1);
					return -1;
				}
				leng-=4;
				if (leng==1) {
					printf("%s [%"PRIu32"]: %s\n",fname,indx,mfsstrerr(*rptr));
					free(buff);
					close_master_conn(1);
					return -1;
				} else {
					if (leng<12 || ((leng-12)%7)!=0) {
						printf("%s [%"PRIu32"]: master query: wrong answer (leng)\n",fname,indx);
						free(buff);
						close_master_conn(1);
						return -1;
					}
					protover = 255;
					copies = (leng-12)/7;
				}
			}
			chunkid = get64bit(&rptr);
			version = get32bit(&rptr);
			if (chunkid==0 && version==0) {
				printf("\tchunk %"PRIu32": empty\n",indx);
			} else {
				printf("\tchunk %"PRIu32": %016"PRIX64"_%08"PRIX32" / (id:%"PRIu64" ver:%"PRIu32")\n",indx,chunkid,version,chunkid,version);
				vcopies = 0;
				wptr = (uint8_t*)rptr;
				if (copies>0) {
					qsort(wptr,copies,(protover==255)?7:(protover==2)?14:(protover==1)?10:6,ip_port_cmp);
				}
				firstdigest = 1;
				checksumerror = 0;
				for (copy=0 ; copy<copies ; copy++) {
					snprintf(csstrip,16,"%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8,rptr[0],rptr[1],rptr[2],rptr[3]);
					csstrip[15]=0;
					csip = get32bit(&rptr);
					csport = get16bit(&rptr);
					strtype = "???";
					if (protover==255) {
						switch (get8bit(&rptr)) {
							case CHECK_VALID:
								strtype = "VALID";
								vcopies++;
								break;
							case CHECK_MARKEDFORREMOVAL:
								strtype = "MARKED FOR REMOVAL";
								break;
							case CHECK_WRONGVERSION:
								strtype = "WRONG VERSION";
								break;
							case CHECK_WV_AND_MFR:
								strtype = "WRONG VERSION , MARKED FOR REMOVAL";
								break;
							case CHECK_INVALID:
								strtype = "INVALID";
								break;
							default:
								strtype = "???";
						}
					} else if (protover==2) {
						strtype = "VALID";
						vcopies++;
						rptr+=8;
					} else if (protover==1) {
						strtype = "VALID";
						vcopies++;
						rptr+=4;
					} else {
						strtype = "VALID";
						vcopies++;
					}
					if (fileinfomode&(FILEINFO_CRC|FILEINFO_SIGNATURE)) {
						if (get_checksum_block(csstrip,csip,csport,chunkid,version,crcblock)==0) {
							md5_init(&chunkctx);
							md5_update(&chunkctx,crcblock,4096);
							if ((fileinfomode&FILEINFO_SIGNATURE) && firstdigest) {
								md5_update(&filectx,crcblock,4096);
							}
							md5_final(currentdigest,&chunkctx);
							if (firstdigest) {
								memcpy(chunkdigest,currentdigest,16);
							} else {
								if (memcmp(chunkdigest,currentdigest,16)!=0) {
									checksumerror = 1;
								}
							}
							firstdigest = 0;
							if (fileinfomode&FILEINFO_CRC) {
								digest_to_str(strdigest,currentdigest);
								printf("\t\tcopy %"PRIu32": %s:%"PRIu16" (status:%s ; checksum digest: %s)\n",copy+1,csstrip,csport,strtype,strdigest);
							} else {
								printf("\t\tcopy %"PRIu32": %s:%"PRIu16" (status:%s)\n",copy+1,csstrip,csport,strtype);
							}
						} else {
							if (fileinfomode&FILEINFO_CRC) {
								printf("\t\tcopy %"PRIu32": %s:%"PRIu16" (status:%s) - can't get checksum\n",copy+1,csstrip,csport,strtype);
							} else {
								printf("\t\tcopy %"PRIu32": %s:%"PRIu16" (status:%s)\n",copy+1,csstrip,csport,strtype);
							}
						}
					} else {
						printf("\t\tcopy %"PRIu32": %s:%"PRIu16" (status:%s)\n",copy+1,csstrip,csport,strtype);
					}
				}
				if (checksumerror) {
					printf("\t\tcopies have different checksums !!!\n");
				}
				if ((fileinfomode&FILEINFO_SIGNATURE) && firstdigest) {
					printf("\t\tcouldn't add this chunk to signature !!!\n");
				}
				if (vcopies==0) {
					printf("\t\tno valid copies !!!\n");
				}
			}
			free(buff);
		}
		close_master_conn(0);
		if (fileinfomode&FILEINFO_SIGNATURE) {
			md5_final(currentdigest,&filectx);
			digest_to_str(strdigest,currentdigest);
			printf("%s signature: %s\n",fname,strdigest);
		}
	}
	return 0;
}

int append_file(const char *fname,const char *afname) {
	uint8_t reqbuff[28+NGROUPS_MAX*4+4],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode,ainode,uid,gid;
	gid_t grouplist[NGROUPS_MAX];
	uint32_t i,gids;
	uint8_t addmaingroup;
	mode_t dmode,smode;
	int fd;
	fd = open_master_conn(fname,&inode,&dmode,NULL,0,1);
	if (fd<0) {
		return -1;
	}
	if (open_master_conn(afname,&ainode,&smode,NULL,1,1)<0) {
		return -1;
	}

	if ((smode&S_IFMT)!=S_IFREG) {
		printf("%s: not a file\n",afname);
		return -1;
	}
	if ((dmode&S_IFMT)!=S_IFREG) {
		printf("%s: not a file\n",fname);
		return -1;
	}
	uid = getuid();
	gid = getgid();
	if (masterversion>=VERSION2INT(2,0,0)) {
		gids = getgroups(NGROUPS_MAX,grouplist);
		addmaingroup = 1;
		for (i=0 ; i<gids ; i++) {
			if (grouplist[i]==gid) {
				addmaingroup = 0;
			}
		}
	} else {
		gids = 0;
		addmaingroup = 0;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_APPEND);
	put32bit(&wptr,20+(addmaingroup+gids)*4);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put32bit(&wptr,ainode);
	put32bit(&wptr,uid);
	if (masterversion<VERSION2INT(2,0,0)) {
		put32bit(&wptr,gid);
	} else {
		put32bit(&wptr,addmaingroup+gids);
		if (addmaingroup) {
			put32bit(&wptr,gid);
		}
		for (i=0 ; i<gids ; i++) {
			put32bit(&wptr,grouplist[i]);
		}
	}
	if (tcpwrite(fd,reqbuff,28+(addmaingroup+gids)*4)!=(int32_t)(28+(addmaingroup+gids)*4)) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_APPEND) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		return -1;
	}
	leng-=4;
	if (leng!=1) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		return -1;
	} else if (*rptr!=STATUS_OK) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		return -1;
	}
	free(buff);
	return 0;
}

int dir_info(uint8_t dirinfomode,const char *fname) {
	uint8_t reqbuff[16],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode;
	uint32_t inodes,dirs,files,chunks;
	uint64_t length,size,realsize;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,0);
	if (fd<0) {
		return -1;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_GETDIRSTATS);
	put32bit(&wptr,8);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	if (tcpwrite(fd,reqbuff,16)!=16) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_GETDIRSTATS) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		close_master_conn(1);
		return -1;
	} else if (leng!=56 && leng!=40) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	inodes = get32bit(&rptr);
	dirs = get32bit(&rptr);
	files = get32bit(&rptr);
	if (leng==56) {
		rptr+=8;
	}
	chunks = get32bit(&rptr);
	if (leng==56) {
		rptr+=8;
	}
	length = get64bit(&rptr);
	size = get64bit(&rptr);
	realsize = get64bit(&rptr);
	free(buff);
	if (dirinfomode==0) {
		printf("%s:\n",fname);
		print_number(" inodes:       ","\n",inodes,0,0,1);
		print_number("  directories: ","\n",dirs,0,0,1);
		print_number("  files:       ","\n",files,0,0,1);
		print_number(" chunks:       ","\n",chunks,0,0,1);
		print_number(" length:       ","\n",length,0,1,1);
		print_number(" size:         ","\n",size,0,1,1);
		print_number(" realsize:     ","\n",realsize,0,1,1);
	} else {
		if (dirinfomode&DIRINFO_INODES) {
			print_number_only(inodes,0);
			printf("\t");
		}
		if (dirinfomode&DIRINFO_DIRS) {
			print_number_only(dirs,0);
			printf("\t");
		}
		if (dirinfomode&DIRINFO_FILES) {
			print_number_only(files,0);
			printf("\t");
		}
		if (dirinfomode&DIRINFO_CHUNKS) {
			print_number_only(chunks,0);
			printf("\t");
		}
		if (dirinfomode&DIRINFO_LENGTH) {
			print_number_only(length,1);
			printf("\t");
		}
		if (dirinfomode&DIRINFO_SIZE) {
			print_number_only(size,1);
			printf("\t");
		}
		if (dirinfomode&DIRINFO_REALSIZE) {
			print_number_only(realsize,1);
			printf("\t");
		}
		printf("%s\n",fname);
	}
	return 0;
}

int file_repair(const char *fname) {
	uint8_t reqbuff[24+NGROUPS_MAX*4+4],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode,uid,gid;
	gid_t grouplist[NGROUPS_MAX];
	uint32_t i,gids;
	uint8_t addmaingroup;
	uint32_t notchanged,erased,repaired;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,1);
	if (fd<0) {
		return -1;
	}
	uid = getuid();
	gid = getgid();
	if (masterversion>=VERSION2INT(2,0,0)) {
		gids = getgroups(NGROUPS_MAX,grouplist);
		addmaingroup = 1;
		for (i=0 ; i<gids ; i++) {
			if (grouplist[i]==gid) {
				addmaingroup = 0;
			}
		}
	} else {
		gids = 0;
		addmaingroup = 0;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_REPAIR);
	put32bit(&wptr,16+(addmaingroup+gids)*4);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put32bit(&wptr,uid);
	if (masterversion<VERSION2INT(2,0,0)) {
		put32bit(&wptr,gid);
	} else {
		put32bit(&wptr,addmaingroup+gids);
		if (addmaingroup) {
			put32bit(&wptr,gid);
		}
		for (i=0 ; i<gids ; i++) {
			put32bit(&wptr,grouplist[i]);
		}
	}
	if (tcpwrite(fd,reqbuff,24+(addmaingroup+gids)*4)!=(int32_t)(24+(addmaingroup+gids)*4)) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_REPAIR) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		close_master_conn(1);
		return -1;
	} else if (leng!=12) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	notchanged = get32bit(&rptr);
	erased = get32bit(&rptr);
	repaired = get32bit(&rptr);
	free(buff);
	printf("%s:\n",fname);
	print_number(" chunks not changed: ","\n",notchanged,1,0,1);
	print_number(" chunks erased:      ","\n",erased,1,0,1);
	print_number(" chunks repaired:    ","\n",repaired,1,0,1);
	return 0;
}

/*
int eattr_control(const char *fname,uint8_t mode,uint8_t eattr) {
	uint8_t reqbuff[22],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode;
	uint8_t nodeeattr,functioneattr;
//	uint32_t curinodes;
//	uint64_t curlength,cursize,currealsize;
	int fd;
	fd = open_master_conn(fname,&inode,NULL,NULL,0,(mode<2)?1:0);
	if (fd<0) {
		return -1;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_EATTR);
	put32bit(&wptr,14);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put32bit(&wptr,getuid());
	put8bit(&wptr,mode&1);
	put8bit(&wptr,(mode>1)?0:eattr);
	if (tcpwrite(fd,reqbuff,22)!=22) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_EATTR) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		close_master_conn(1);
		return -1;
	} else if (leng!=2) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	nodeeattr = get8bit(&rptr) & eattr;
	functioneattr = get8bit(&rptr) & eattr;
	free(buff);
	printf("%s:",fname);
	printf(" nodeeattr:");
	if (nodeeattr==0) {
		printf("-");
	} else {
		// as for now there is only one eattr: noowner
		if (nodeeattr&EATTR_NOOWNER) {
			printf("noowner");
		} else {
			printf("?");
		}
	}
	printf("; workingeattr:");
	if (functioneattr==0) {
		printf("-");
	} else {
		// as for now there is only one eattr: noowner
		if (functioneattr&EATTR_NOOWNER) {
			printf("noowner");
		} else {
			printf("?");
		}
	}
	printf("\n");
	return 0;
}
*/

int quota_control(const char *fname,uint8_t mode,uint8_t *qflags,uint32_t *graceperiod,uint32_t *sinodes,uint64_t *slength,uint64_t *ssize,uint64_t *srealsize,uint32_t *hinodes,uint64_t *hlength,uint64_t *hsize,uint64_t *hrealsize) {
	uint8_t reqbuff[73],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,inode;
	uint32_t curinodes;
	uint64_t curlength,cursize,currealsize;
	int32_t psize;
	int fd;
//	printf("set quota: %s (soft:%1X,i:%"PRIu32",l:%"PRIu64",w:%"PRIu64",r:%"PRIu64"),(hard:%1X,i:%"PRIu32",l:%"PRIu64",w:%"PRIu64",r:%"PRIu64")\n",fname,sflags,sinodes,slength,ssize,srealsize,hflags,hinodes,hlength,hsize,hrealsize);
	if (mode==2) {
		*qflags = 0;
	}
	fd = open_master_conn(fname,&inode,NULL,NULL,0,(*qflags)?1:0);
	if (fd<0) {
		return -1;
	}
	psize = (mode)?9:(masterversion<VERSION2INT(3,0,9))?65:69;
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_QUOTACONTROL);
	put32bit(&wptr,psize);
	put32bit(&wptr,0);
	put32bit(&wptr,inode);
	put8bit(&wptr,*qflags);
	if (mode==0) {
		if (masterversion>=VERSION2INT(3,0,9)) {
			put32bit(&wptr,*graceperiod);
		}
		put32bit(&wptr,*sinodes);
		put64bit(&wptr,*slength);
		put64bit(&wptr,*ssize);
		put64bit(&wptr,*srealsize);
		put32bit(&wptr,*hinodes);
		put64bit(&wptr,*hlength);
		put64bit(&wptr,*hsize);
		put64bit(&wptr,*hrealsize);
	}
	if (tcpwrite(fd,reqbuff,psize+8)!=(psize+8)) {
		printf("%s: master query: send error\n",fname);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s: master query: receive error\n",fname);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_QUOTACONTROL) {
		printf("%s: master query: wrong answer (type)\n",fname);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s: master query: receive error\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s: master query: wrong answer (queryid)\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	leng-=4;
	if (leng==1) {
		printf("%s: %s\n",fname,mfsstrerr(*rptr));
		free(buff);
		close_master_conn(1);
		return -1;
	} else if (leng!=85 && leng!=89) {
		printf("%s: master query: wrong answer (leng)\n",fname);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	*qflags = get8bit(&rptr);
	if (leng==89) {
		*graceperiod = get32bit(&rptr);
	} else {
		*graceperiod = 0;
	}
	*sinodes = get32bit(&rptr);
	*slength = get64bit(&rptr);
	*ssize = get64bit(&rptr);
	*srealsize = get64bit(&rptr);
	*hinodes = get32bit(&rptr);
	*hlength = get64bit(&rptr);
	*hsize = get64bit(&rptr);
	*hrealsize = get64bit(&rptr);
	curinodes = get32bit(&rptr);
	curlength = get64bit(&rptr);
	cursize = get64bit(&rptr);
	currealsize = get64bit(&rptr);
	free(buff);
	if ((*graceperiod)>0) {
		printf("%s: (current values | soft quota | hard quota) ; soft quota grace period: %u seconds\n",fname,*graceperiod);
	} else {
		printf("%s: (current values | soft quota | hard quota) ; soft quota grace period: default\n",fname);
	}
	print_number(" inodes   | ",NULL,curinodes,0,0,1);
	print_number(" | ",NULL,*sinodes,0,0,(*qflags)&QUOTA_FLAG_SINODES);
	print_number(" | "," |\n",*hinodes,0,0,(*qflags)&QUOTA_FLAG_HINODES);
	print_number(" length   | ",NULL,curlength,0,1,1);
	print_number(" | ",NULL,*slength,0,1,(*qflags)&QUOTA_FLAG_SLENGTH);
	print_number(" | "," |\n",*hlength,0,1,(*qflags)&QUOTA_FLAG_HLENGTH);
	print_number(" size     | ",NULL,cursize,0,1,1);
	print_number(" | ",NULL,*ssize,0,1,(*qflags)&QUOTA_FLAG_SSIZE);
	print_number(" | "," |\n",*hsize,0,1,(*qflags)&QUOTA_FLAG_HSIZE);
	print_number(" realsize | ",NULL,currealsize,0,1,1);
	print_number(" | ",NULL,*srealsize,0,1,(*qflags)&QUOTA_FLAG_SREALSIZE);
	print_number(" | "," |\n",*hrealsize,0,1,(*qflags)&QUOTA_FLAG_HREALSIZE);
	return 0;
}

/*
int get_quota(const char *fname) {
	printf("get quota: %s\n",fname);
	return 0;
}

int delete_quota(const char *fname,uint8_t sflags,uint8_t hflags) {
	printf("delete quota: %s (soft:%1X,hard:%1X)\n",fname,sflags,hflags);
	return 0;
}
*/

int snapshot_ctl(const char *dstdir,const char *dstbase,const char *srcname,uint32_t srcinode,uint8_t smode) {
	uint8_t reqbuff[8+24+255+NGROUPS_MAX*4+4],*wptr,*buff;
	const uint8_t *rptr;
	uint32_t cmd,leng,dstinode,uid,gid;
	gid_t grouplist[NGROUPS_MAX];
	uint32_t i,gids;
	uint8_t addmaingroup;
	uint32_t nleng;
	uint16_t umsk;
	int fd;
	umsk = umask(0);
	umask(umsk);
	nleng = strlen(dstbase);
	if (nleng>255) {
		printf("%s: name too long\n",dstbase);
		return -1;
	}
	fd = open_master_conn(dstdir,&dstinode,NULL,NULL,0,1);
	if (fd<0) {
		return -1;
	}
	uid = getuid();
	gid = getgid();
	if (masterversion>=VERSION2INT(2,0,0)) {
		gids = getgroups(NGROUPS_MAX,grouplist);
		addmaingroup = 1;
		for (i=0 ; i<gids ; i++) {
			if (grouplist[i]==gid) {
				addmaingroup = 0;
			}
		}
	} else {
		gids = 0;
		addmaingroup = 0;
	}
	wptr = reqbuff;
	put32bit(&wptr,CLTOMA_FUSE_SNAPSHOT);
	if (masterversion<VERSION2INT(1,7,0)) {
		put32bit(&wptr,22+nleng);
	} else {
		put32bit(&wptr,24+nleng+(addmaingroup+gids)*4);
	}
	put32bit(&wptr,0);
	put32bit(&wptr,srcinode);
	put32bit(&wptr,dstinode);
	put8bit(&wptr,nleng);
	memcpy(wptr,dstbase,nleng);
	wptr+=nleng;
	put32bit(&wptr,uid);
	if (masterversion<VERSION2INT(2,0,0)) {
		put32bit(&wptr,gid);
	} else {
		put32bit(&wptr,addmaingroup+gids);
		if (addmaingroup) {
			put32bit(&wptr,gid);
		}
		for (i=0 ; i<gids ; i++) {
			put32bit(&wptr,grouplist[i]);
		}
	}
	put8bit(&wptr,smode);
	if (masterversion>=VERSION2INT(1,7,0)) {
		put16bit(&wptr,umsk);
	}
	if (tcpwrite(fd,reqbuff,((masterversion>=VERSION2INT(1,7,0))?32:30)+nleng+(addmaingroup+gids)*4)!=(int32_t)(((masterversion>=VERSION2INT(1,7,0))?32:30)+nleng+(addmaingroup+gids)*4)) {
		printf("%s->%s/%s: master query: send error\n",srcname,dstdir,dstbase);
		close_master_conn(1);
		return -1;
	}
	if (tcpread(fd,reqbuff,8)!=8) {
		printf("%s->%s/%s: master query: receive error\n",srcname,dstdir,dstbase);
		close_master_conn(1);
		return -1;
	}
	rptr = reqbuff;
	cmd = get32bit(&rptr);
	leng = get32bit(&rptr);
	if (cmd!=MATOCL_FUSE_SNAPSHOT) {
		printf("%s->%s/%s: master query: wrong answer (type)\n",srcname,dstdir,dstbase);
		close_master_conn(1);
		return -1;
	}
	buff = malloc(leng);
	if (tcpread(fd,buff,leng)!=(int32_t)leng) {
		printf("%s->%s/%s: master query: receive error\n",srcname,dstdir,dstbase);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	rptr = buff;
	cmd = get32bit(&rptr);	// queryid
	if (cmd!=0) {
		printf("%s->%s/%s: master query: wrong answer (queryid)\n",srcname,dstdir,dstbase);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	leng-=4;
	if (leng!=1) {
		printf("%s->%s/%s: master query: wrong answer (leng)\n",srcname,dstdir,dstbase);
		free(buff);
		close_master_conn(1);
		return -1;
	}
	close_master_conn(0);
	if (*rptr!=0) {
		printf("%s->%s/%s: %s\n",srcname,dstdir,dstbase,mfsstrerr(*rptr));
		free(buff);
		return -1;
	}
	return 0;
}

int remove_snapshot(const char *dstname,uint8_t smode) {
	char dstpath[PATH_MAX+1],base[PATH_MAX+1],dir[PATH_MAX+1];

	if (realpath(dstname,dstpath)==NULL) {
		printf("%s: realpath error on %s: %s\n",dstname,dstpath,strerr(errno));
	}
	memcpy(dir,dstpath,PATH_MAX+1);
	dirname_inplace(dir);
	if (bsd_basename(dstpath,base)<0) {
		printf("%s: basename error\n",dstpath);
		return -1;
	}
	return snapshot_ctl(dir,base,NULL,0,smode | SNAPSHOT_MODE_DELETE);
}

int make_snapshot(const char *dstname,char * const *srcnames,uint32_t srcelements,uint8_t smode) {
	char to[PATH_MAX+1],base[PATH_MAX+1],dir[PATH_MAX+1];
	char src[PATH_MAX+1];
	struct stat sst,dst;
	int status;
	uint32_t i,l;

	if (stat(dstname,&dst)<0) {	// dst does not exist
		if (errno!=ENOENT) {
			printf("%s: stat error: %s\n",dstname,strerr(errno));
			return -1;
		}
		if (srcelements>1) {
			printf("can snapshot multiple elements only into existing directory\n");
			return -1;
		}
		if (lstat(srcnames[0],&sst)<0) {
			printf("%s: lstat error: %s\n",srcnames[0],strerr(errno));
			return -1;
		}
		if (bsd_dirname(dstname,dir)<0) {
			printf("%s: dirname error\n",dstname);
			return -1;
		}
		if (stat(dir,&dst)<0) {
			printf("%s: stat error: %s\n",dir,strerr(errno));
			return -1;
		}
		if (sst.st_dev != dst.st_dev) {
			printf("(%s,%s): both elements must be on the same device\n",dstname,srcnames[0]);
			return -1;
		}
		if (realpath(dir,to)==NULL) {
			printf("%s: realpath error on %s: %s\n",dir,to,strerr(errno));
			return -1;
		}
		if (bsd_basename(dstname,base)<0) {
			printf("%s: basename error\n",dstname);
			return -1;
		}
		if (strlen(dstname)>0 && dstname[strlen(dstname)-1]=='/' && !S_ISDIR(sst.st_mode)) {
			printf("directory %s does not exist\n",dstname);
			return -1;
		}
		return snapshot_ctl(to,base,srcnames[0],sst.st_ino,smode);
	} else {	// dst exists
		if (realpath(dstname,to)==NULL) {
			printf("%s: realpath error on %s: %s\n",dstname,to,strerr(errno));
			return -1;
		}
		if (!S_ISDIR(dst.st_mode)) {	// dst id not a directory
		       	if (srcelements>1) {
				printf("can snapshot multiple elements only into existing directory\n");
				return -1;
			}
			if (lstat(srcnames[0],&sst)<0) {
				printf("%s: lstat error: %s\n",srcnames[0],strerr(errno));
				return -1;
			}
			if (sst.st_dev != dst.st_dev) {
				printf("(%s,%s): both elements must be on the same device\n",dstname,srcnames[0]);
				return -1;
			}
			memcpy(dir,to,PATH_MAX+1);
			dirname_inplace(dir);
//			if (bsd_dirname(to,dir)<0) {
//				printf("%s: dirname error\n",to);
//				return -1;
//			}
			if (bsd_basename(to,base)<0) {
				printf("%s: basename error\n",to);
				return -1;
			}
			return snapshot_ctl(dir,base,srcnames[0],sst.st_ino,smode);
		} else {	// dst is a directory
			status = 0;
			for (i=0 ; i<srcelements ; i++) {
				if (lstat(srcnames[i],&sst)<0) {
					printf("%s: lstat error: %s\n",srcnames[i],strerr(errno));
					status=-1;
					continue;
				}
				if (sst.st_dev != dst.st_dev) {
					printf("(%s,%s): both elements must be on the same device\n",dstname,srcnames[i]);
					status=-1;
					continue;
				}
				if (!S_ISDIR(sst.st_mode)) {	// src is not a directory
					if (!S_ISLNK(sst.st_mode)) {	// src is not a symbolic link
						if (realpath(srcnames[i],src)==NULL) {
							printf("%s: realpath error on %s: %s\n",srcnames[i],src,strerr(errno));
							status=-1;
							continue;
						}
						if (bsd_basename(src,base)<0) {
							printf("%s: basename error\n",src);
							status=-1;
							continue;
						}
					} else {	// src is a symbolic link
						if (bsd_basename(srcnames[i],base)<0) {
							printf("%s: basename error\n",srcnames[i]);
							status=-1;
							continue;
						}
					}
					if (snapshot_ctl(to,base,srcnames[i],sst.st_ino,smode)<0) {
						status=-1;
					}
				} else {	// src is a directory
					l = strlen(srcnames[i]);
					if (l>0 && srcnames[i][l-1]!='/') {	// src is a directory and name has trailing slash
						if (realpath(srcnames[i],src)==NULL) {
							printf("%s: realpath error on %s: %s\n",srcnames[i],src,strerr(errno));
							status=-1;
							continue;
						}
						if (bsd_basename(src,base)<0) {
							printf("%s: basename error\n",src);
							status=-1;
							continue;
						}
						if (snapshot_ctl(to,base,srcnames[i],sst.st_ino,smode)<0) {
							status=-1;
						}
					} else {	// src is a directory and name has not trailing slash
						memcpy(dir,to,PATH_MAX+1);
						dirname_inplace(dir);
						//if (bsd_dirname(to,dir)<0) {
						//	printf("%s: dirname error\n",to);
						//	status=-1;
						//	continue;
						//}
						if (bsd_basename(to,base)<0) {
							printf("%s: basename error\n",to);
							status=-1;
							continue;
						}
						if (snapshot_ctl(dir,base,srcnames[i],sst.st_ino,smode)<0) {
							status=-1;
						}
					}
				}
			}
			return status;
		}
	}
}

enum {
	MFSGETGOAL=1,
	MFSSETGOAL,
	MFSCOPYGOAL,
	MFSGETTRASHTIME,
	MFSSETTRASHTIME,
	MFSCOPYTRASHTIME,
	MFSCHECKFILE,
	MFSFILEINFO,
	MFSAPPENDCHUNKS,
	MFSDIRINFO,
	MFSFILEREPAIR,
	MFSMAKESNAPSHOT,
	MFSRMSNAPSHOT,
	MFSGETEATTR,
	MFSSETEATTR,
	MFSDELEATTR,
	MFSCOPYEATTR,
	MFSGETQUOTA,
	MFSSETQUOTA,
	MFSDELQUOTA,
	MFSCOPYQUOTA,
	MFSFILEPATHS,
	MFSCHKARCHIVE,
	MFSSETARCHIVE,
	MFSCLRARCHIVE
};

static inline void print_numberformat_options() {
	fprintf(stderr," -n - show numbers in plain format\n");
	fprintf(stderr," -h - \"human-readable\" numbers using base 2 prefixes (IEC 60027)\n");
	fprintf(stderr," -H - \"human-readable\" numbers using base 10 prefixes (SI)\n");
	fprintf(stderr," -k - show plain numbers in kibis (binary kilo - 1024)\n");
	fprintf(stderr," -m - show plain numbers in mebis (binary mega - 1024^2)\n");
	fprintf(stderr," -g - show plain numbers in gibis (binary giga - 1024^3)\n");
}

static inline void print_recursive_option() {
	fprintf(stderr," -r - do it recursively\n");
}

static inline void print_extra_attributes() {
	int j;
	fprintf(stderr,"\nattributes:\n");
	for (j=0 ; j<EATTR_BITS ; j++) {
		if (eattrtab[j][0]) {
			fprintf(stderr," %s - %s\n",eattrtab[j],eattrdesc[j]);
		}
	}
}

void usage(int f) {
	switch (f) {
		case MFSGETGOAL:
			fprintf(stderr,"get objects goal (desired number of copies)\n\nusage: mfsgetgoal [-nhHkmgr] name [name ...]\n");
			print_numberformat_options();
			print_recursive_option();
			break;
		case MFSSETGOAL:
			fprintf(stderr,"set objects goal (desired number of copies)\n\nusage: mfssetgoal [-lsnhHkmgr] GOAL[-|+]|LABELS name [name ...]\n       mfssetgoal [-lsnhHkmgr] -K KEEP_LABELS [ -C CREATE_LABELS ] [ -A ARCHIVE_LABELS -d ARCHIVE_DELAY ] name [name ...]\n");
			print_numberformat_options();
			print_recursive_option();
			fprintf(stderr," -l - use 'loose' mode for create (allow using other labels during chunk creation wnen servers are overloaded or full)\n");
			fprintf(stderr," -s - use 'strict' mode for create (never use other labels for creating new chunks)\n");
			fprintf(stderr," GOAL+ - increase goal to given value\n");
			fprintf(stderr," GOAL- - decrease goal to given value\n");
			fprintf(stderr," GOAL - just set goal to given value\n");
			fprintf(stderr," LABELS - set goal using labels\n");
			fprintf(stderr," KEEP_LABELS - specify separate labels/goal used to keep data\n");
			fprintf(stderr," CREATE_LABELS - specify separate labels/goal used to create new chunks\n");
			fprintf(stderr," ARCHIVE_LABELS - specify separate labels/goal used when ARCHIVE_DELAY days passed after last file modification (mtime)\n");
			break;
		case MFSCOPYGOAL:
			fprintf(stderr,"copy object goal (desired number of copies)\n\nusage: mfscopygoal [-nhHkmgr] srcname dstname [dstname ...]\n");
			print_numberformat_options();
			print_recursive_option();
			break;
		case MFSGETTRASHTIME:
			fprintf(stderr,"get objects trashtime (how many seconds file should be left in trash)\n\nusage: mfsgettrashtime [-nhHkmgr] name [name ...]\n");
			print_numberformat_options();
			print_recursive_option();
			break;
		case MFSSETTRASHTIME:
			fprintf(stderr,"set objects trashtime (how many seconds file should be left in trash)\n\nusage: mfssettrashtime [-nhHkmgr] SECONDS[-|+] name [name ...]\n");
			print_numberformat_options();
			print_recursive_option();
			fprintf(stderr," SECONDS+ - increase trashtime to given value\n");
			fprintf(stderr," SECONDS- - decrease trashtime to given value\n");
			fprintf(stderr," SECONDS - just set trashtime to given value\n");
			break;
		case MFSCOPYTRASHTIME:
			fprintf(stderr,"copy objects trashtime (how many seconds file should be left in trash)\n\nusage: mfscopytrashtime [-nhHkmgr] srcname dstname [dstname ...]\n");
			print_numberformat_options();
			print_recursive_option();
			break;
		case MFSCHECKFILE:
			fprintf(stderr,"check files\n\nusage: mfscheckfile [-nhHkmg] name [name ...]\n");
			break;
		case MFSFILEINFO:
			fprintf(stderr,"show files info (shows detailed info of each file chunk)\n\nusage: mfsfileinfo [-qcs] name [name ...]\n");
			fprintf(stderr,"switches:\n");
			fprintf(stderr,"-q - quick info (show only number of valid copies)\n");
			fprintf(stderr,"-c - receive chunk checksums from chunkservers\n");
			fprintf(stderr,"-s - calculate file signature (using checksums)\n");
			break;
		case MFSAPPENDCHUNKS:
			fprintf(stderr,"append file chunks to another file. If destination file doesn't exist then it's created as empty file and then chunks are appended\n\nusage: mfsappendchunks dstfile name [name ...]\n");
			break;
		case MFSDIRINFO:
			fprintf(stderr,"show directories stats\n\nusage: mfsdirinfo [-nhHkmg] [-idfclsr] name [name ...]\n");
			print_numberformat_options();
			fprintf(stderr,"'show' switches:\n");
			fprintf(stderr," -i - show number of inodes\n");
			fprintf(stderr," -d - show number of directories\n");
			fprintf(stderr," -f - show number of files\n");
			fprintf(stderr," -c - show number of chunks\n");
			fprintf(stderr," -l - show length\n");
			fprintf(stderr," -s - show size\n");
			fprintf(stderr," -r - show realsize\n");
			fprintf(stderr,"\nIf no 'show' switches are present then show everything\n");
			fprintf(stderr,"\nMeaning of some not obvious output data:\n 'length' is just sum of files lengths\n 'size' is sum of chunks lengths\n 'realsize' is estimated hdd usage (usually size multiplied by current goal)\n");
			break;
		case MFSFILEREPAIR:
			fprintf(stderr,"repair given file. Use it with caution. It forces file to be readable, so it could erase (fill with zeros) file when chunkservers are not currently connected.\n\nusage: mfsfilerepair [-nhHkmg] name [name ...]\n");
			print_numberformat_options();
			break;
		case MFSMAKESNAPSHOT:
			fprintf(stderr,"make snapshot (lazy copy)\n\nusage: mfsmakesnapshot [-op] src [src ...] dst\n");
			fprintf(stderr,"-o - allow to overwrite existing objects\n");
			fprintf(stderr,"-c - 'cp' mode for attributes (create objects using current uid,gid,umask etc.)\n");
			break;
		case MFSRMSNAPSHOT:
			fprintf(stderr,"remove snapshot (quick rm -r)\n\nusage: mfsrmsnapshot [-f] name [name ...]\n");
			fprintf(stderr,"-f - remove as much as possible (according to access rights and snapshot flags)\n");
			break;
		case MFSGETEATTR:
			fprintf(stderr,"get objects extra attributes\n\nusage: mfsgeteattr [-nhHkmgr] name [name ...]\n");
			print_numberformat_options();
			print_recursive_option();
			break;
		case MFSSETEATTR:
			fprintf(stderr,"set objects extra attributes\n\nusage: mfsseteattr [-nhHkmgr] -f attrname [-f attrname ...] name [name ...]\n");
			print_numberformat_options();
			print_recursive_option();
			fprintf(stderr," -f attrname - specify attribute to set\n");
			print_extra_attributes();
			break;
		case MFSDELEATTR:
			fprintf(stderr,"delete objects extra attributes\n\nusage: mfsdeleattr [-nhHkmgr] -f attrname [-f attrname ...] name [name ...]\n");
			print_numberformat_options();
			print_recursive_option();
			fprintf(stderr," -f attrname - specify attribute to delete\n");
			print_extra_attributes();
			break;
		case MFSCOPYEATTR:
			fprintf(stderr,"copy objects extra attributes\n\nusage: mfscopyeattr [-nhHkmgr] srcname dstname [dstname ...]\n");
			print_numberformat_options();
			print_recursive_option();
			break;
		case MFSGETQUOTA:
			fprintf(stderr,"get quota for given directory (directories)\n\nusage: mfsgetquota [-nhHkmg] dirname [dirname ...]\n");
			print_numberformat_options();
			break;
		case MFSSETQUOTA:
			fprintf(stderr,"set quota for given directory (directories)\n\nusage: mfssetquota [-nhHkmg] [-iI inodes] [-p grace_period] [-lL length] [-sS size] [-rR realsize] dirname [dirname ...]\n");
			print_numberformat_options();
			fprintf(stderr," -p - set grace period in seconds for soft quota\n");
			fprintf(stderr," -i/-I - set soft/hard limit for number of filesystem objects\n");
			fprintf(stderr," -l/-L - set soft/hard limit for sum of files lengths\n");
			fprintf(stderr," -s/-S - set soft/hard limit for sum of file sizes (chunk sizes)\n");
			fprintf(stderr," -r/-R - set soft/hard limit for estimated hdd usage (usually size multiplied by goal)\n");
			fprintf(stderr,"\nAll numbers can have decimal point and SI/IEC symbol prefix at the end\ndecimal (SI): (k - 10^3 , M - 10^6 , G - 10^9 , T - 10^12 , P - 10^15 , E - 10^18)\nbinary (IEC 60027): (Ki - 2^10 , Mi - 2^20 , Gi - 2^30 , Ti - 2^40 , Pi - 2^50 , Ei - 2^60 )\n");
			break;
		case MFSDELQUOTA:
			fprintf(stderr,"delete quota for given directory (directories)\n\nusage: mfsdelquota [-nhHkmgailsrAILSR] dirname [dirname ...]\n");
			print_numberformat_options();
			fprintf(stderr," -i/-I - delete inodes soft/hard quota\n");
			fprintf(stderr," -l/-L - delete length soft/hard quota\n");
			fprintf(stderr," -s/-S - delete size soft/hard quota\n");
			fprintf(stderr," -r/-R - delete real size soft/hard quota\n");
			fprintf(stderr," -a/-A - delete all soft/hard quotas\n");
			break;
		case MFSCOPYQUOTA:
			fprintf(stderr,"copy quota settings from one directory to another directory (directories)\n\nusage: mfscopyquota [-nhHkmg] srcdirname dstdirname [dstdirname ...]\n");
			print_numberformat_options();
			break;
		case MFSFILEPATHS:
			fprintf(stderr,"show all paths of given files or node numbers\n\nusage: mfsfilepaths name/inode [name/inode ...]\n");
			fprintf(stderr,"\nIn case of converting node to path, tool has to be run in mfs-mounted directory\n");
			break;
		case MFSCHKARCHIVE:
			fprintf(stderr,"checks if archive flag is set or not (when directory is specified then command will check it recursivelly)\n\nusage: mfschgarchive [-nhHkmg] name [name ...]\n");
			print_numberformat_options();
			break;
		case MFSSETARCHIVE:
			fprintf(stderr,"set archive flags in chunks (recursivelly for directories) - moves files to archive (use 'archive' goal/labels instead of 'keep' goal/labels)\n\nusage: mfssetarchive [-nhHkmg] name [name ...]\n");
			break;
		case MFSCLRARCHIVE:
			fprintf(stderr,"clear archive flags in chunks (recursivelly for directories) - moves files from archive (use 'keep' goal/labels instead of 'archive' goal/labels) - it also changes ctime, so files will move back to archive after time specified in mfssetgoal\n\nusage: mfsclrarchive [-nhHkmg] name [name ...]\n");
			break;
	}
	exit(1);
}

int main(int argc,char **argv) {
	int l,f,status;
	int i,j,found;
	int ch;
	int snapmode = 0;
	int rflag = 0;
	uint8_t dirinfomode = 0;
	uint8_t fileinfomode = 0;
	uint64_t v;
	uint8_t eattr = 0,goal = 1,smode = SMODE_SET,create_mode = CREATE_MODE_STD;
	uint32_t create_labelmasks[9][MASKORGROUP];
	uint32_t keep_labelmasks[9][MASKORGROUP];
	uint32_t arch_labelmasks[9][MASKORGROUP];
	uint8_t create_labelscnt = 0;
	uint8_t keep_labelscnt = 0;
	uint8_t arch_labelscnt = 0;
	uint16_t arch_delay = 0;
	uint32_t trashtime = 86400;
	uint32_t sinodes = 0,hinodes = 0;
	uint64_t slength = 0,hlength = 0,ssize = 0,hsize = 0,srealsize = 0,hrealsize = 0;
	uint32_t graceperiod = 0;
	uint8_t qflags = 0;
	char *appendfname = NULL;
	char *srcname = NULL;
	char *hrformat;
	char *p;

	strerr_init();

	l = strlen(argv[0]);
#define CHECKNAME(name) ((l==(int)(sizeof(name)-1) && strcmp(argv[0],name)==0) || (l>(int)(sizeof(name)-1) && strcmp((argv[0])+(l-sizeof(name)),"/" name)==0))

	if (CHECKNAME("mfstools")) {
		if (argc==2 && strcmp(argv[1],"create")==0) {
			fprintf(stderr,"create symlinks\n");
#define SYMLINK(name)	if (symlink(argv[0],name)<0) { \
				perror("error creating symlink '"name"'"); \
			}
			SYMLINK("mfsgetgoal")
			SYMLINK("mfssetgoal")
			SYMLINK("mfscopygoal")
			SYMLINK("mfsgettrashtime")
			SYMLINK("mfssettrashtime")
			SYMLINK("mfscopytrashtime")
			SYMLINK("mfscheckfile")
			SYMLINK("mfsfileinfo")
			SYMLINK("mfsappendchunks")
			SYMLINK("mfsdirinfo")
			SYMLINK("mfsfilerepair")
			SYMLINK("mfsmakesnapshot")
			SYMLINK("mfsrmsnapshot")
			SYMLINK("mfsgeteattr")
			SYMLINK("mfsseteattr")
			SYMLINK("mfsdeleattr")
			SYMLINK("mfscopyeattr")
			SYMLINK("mfsgetquota")
			SYMLINK("mfssetquota")
			SYMLINK("mfsdelquota")
			SYMLINK("mfscopyquota")
			SYMLINK("mfsfilepaths")
			SYMLINK("mfschkarchive")
			SYMLINK("mfssetarchive")
			SYMLINK("mfsclrarchive")
			// deprecated tools:
			SYMLINK("mfsrgetgoal")
			SYMLINK("mfsrsetgoal")
			SYMLINK("mfsrgettrashtime")
			SYMLINK("mfsrsettrashtime")
			return 0;
		} else if (argc==1) {
			fprintf(stderr,"mfs multi tool\n\nusage:\n\tmfstools create - create symlinks (mfs<toolname> -> %s)\n",argv[0]);
			fprintf(stderr,"\tmfstools mfs<toolname> ... - work as a given tool\n");
			fprintf(stderr,"\ntools:\n");
			fprintf(stderr,"\tmfsgetgoal\n\tmfssetgoal\n\tmfscopygoal\n");
			fprintf(stderr,"\tmfsgettrashtime\n\tmfssettrashtime\n\tmfscopytrashtime\n");
			fprintf(stderr,"\tmfsgeteattr\n\tmfsseteattr\n\tmfsdeleattr\n\tmfscopyeattr\n");
			fprintf(stderr,"\tmfsgetquota\n\tmfssetquota\n\tmfsdelquota\n\tmfscopyquota\n");
			fprintf(stderr,"\tmfscheckfile\n\tmfsfileinfo\n\tmfsappendchunks\n\tmfsdirinfo\n");
			fprintf(stderr,"\tmfsfilerepair\n\tmfsmakesnapshot\n\tmfsfilepaths\n");
			fprintf(stderr,"\tmfschkarchive\n\tmfssetarchive\n\tmfsclrarchive\n");
			return 1;
		}
		argv++;
		argc--;
		l = strlen(argv[0]);
	}
	if (CHECKNAME("mfsgetgoal")) {
		f=MFSGETGOAL;
	} else if (CHECKNAME("mfsrgetgoal")) {
		f=MFSGETGOAL;
		rflag=1;
		fprintf(stderr,"deprecated tool - use \"mfsgetgoal -r\"\n");
	} else if (CHECKNAME("mfssetgoal")) {
		f=MFSSETGOAL;
	} else if (CHECKNAME("mfsrsetgoal")) {
		f=MFSSETGOAL;
		rflag=1;
		fprintf(stderr,"deprecated tool - use \"mfssetgoal -r\"\n");
	} else if (CHECKNAME("mfscopygoal")) {
		f=MFSCOPYGOAL;
	} else if (CHECKNAME("mfsgettrashtime")) {
		f=MFSGETTRASHTIME;
	} else if (CHECKNAME("mfsrgettrashtime")) {
		f=MFSGETTRASHTIME;
		rflag=1;
		fprintf(stderr,"deprecated tool - use \"mfsgettrashtime -r\"\n");
	} else if (CHECKNAME("mfssettrashtime")) {
		f=MFSSETTRASHTIME;
	} else if (CHECKNAME("mfsrsettrashtime")) {
		f=MFSSETTRASHTIME;
		rflag=1;
		fprintf(stderr,"deprecated tool - use \"mfssettrashtime -r\"\n");
	} else if (CHECKNAME("mfscopytrashtime")) {
		f=MFSCOPYTRASHTIME;
	} else if (CHECKNAME("mfscheckfile")) {
		f=MFSCHECKFILE;
	} else if (CHECKNAME("mfsfileinfo")) {
		f=MFSFILEINFO;
	} else if (CHECKNAME("mfsappendchunks")) {
		f=MFSAPPENDCHUNKS;
	} else if (CHECKNAME("mfsdirinfo")) {
		f=MFSDIRINFO;
	} else if (CHECKNAME("mfsgeteattr")) {
		f=MFSGETEATTR;
	} else if (CHECKNAME("mfsseteattr")) {
		f=MFSSETEATTR;
	} else if (CHECKNAME("mfsdeleattr")) {
		f=MFSDELEATTR;
	} else if (CHECKNAME("mfscopyeattr")) {
		f=MFSCOPYEATTR;
	} else if (CHECKNAME("mfsgetquota")) {
		f=MFSGETQUOTA;
	} else if (CHECKNAME("mfssetquota")) {
		f=MFSSETQUOTA;
	} else if (CHECKNAME("mfsdelquota")) {
		f=MFSDELQUOTA;
	} else if (CHECKNAME("mfscopyquota")) {
		f=MFSCOPYQUOTA;
	} else if (CHECKNAME("mfsfilerepair")) {
		f=MFSFILEREPAIR;
	} else if (CHECKNAME("mfsmakesnapshot")) {
		f=MFSMAKESNAPSHOT;
	} else if (CHECKNAME("mfsrmsnapshot")) {
		f=MFSRMSNAPSHOT;
	} else if (CHECKNAME("mfsfilepaths")) {
		f=MFSFILEPATHS;
	} else if (CHECKNAME("mfschkarchive")) {
		f=MFSCHKARCHIVE;
	} else if (CHECKNAME("mfssetarchive")) {
		f=MFSSETARCHIVE;
	} else if (CHECKNAME("mfsclrarchive")) {
		f=MFSCLRARCHIVE;
	} else {
		fprintf(stderr,"unknown binary name\n");
		return 1;
	}
//	argc--;
//	argv++;

	hrformat = getenv("MFSHRFORMAT");
	if (hrformat) {
		if (hrformat[0]>='0' && hrformat[0]<='4') {
			humode=hrformat[0]-'0';
		}
		if (hrformat[0]=='h') {
			if (hrformat[1]=='+') {
				humode=3;
			} else {
				humode=1;
			}
		}
		if (hrformat[0]=='H') {
			if (hrformat[1]=='+') {
				humode=4;
			} else {
				humode=2;
			}
		}
	}

	// parse options
	switch (f) {
	case MFSMAKESNAPSHOT:
		while ((ch=getopt(argc,argv,"oc"))!=-1) {
			switch(ch) {
			case 'o':
				snapmode |= SNAPSHOT_MODE_CAN_OVERWRITE;
				break;
			case 'c':
				snapmode |= SNAPSHOT_MODE_CPLIKE_ATTR;
				break;
			}
		}
		argc -= optind;
		argv += optind;
		if (argc<2) {
			usage(f);
		}
		return make_snapshot(argv[argc-1],argv,argc-1,snapmode);
	case MFSRMSNAPSHOT:
		while ((ch=getopt(argc,argv,"f"))!=-1) {
			switch (ch) {
			case 'f':
				snapmode |= SNAPSHOT_MODE_FORCE_REMOVAL;
				break;
			}
		}
		argc -= optind;
		argv += optind;
		break;
	case MFSCOPYGOAL:
	case MFSCOPYTRASHTIME:
	case MFSCOPYEATTR:
		while ((ch=getopt(argc,argv,"rnhHkmg"))!=-1) {
			switch(ch) {
			case 'n':
				humode=0;
				break;
			case 'h':
				humode=1;
				break;
			case 'H':
				humode=2;
				break;
			case 'k':
				numbermode=1;
				break;
			case 'm':
				numbermode=2;
				break;
			case 'g':
				numbermode=3;
				break;
			case 'r':
				rflag=1;
				break;
			}
		}
		argc -= optind;
		argv += optind;
		if (argc<2) {
			usage(f);
		}
		srcname = *argv;
		argc--;
		argv++;
		break;
	case MFSCOPYQUOTA:
		while ((ch=getopt(argc,argv,"nhHkmg"))!=-1) {
			switch(ch) {
			case 'n':
				humode=0;
				break;
			case 'h':
				humode=1;
				break;
			case 'H':
				humode=2;
				break;
			case 'k':
				numbermode=1;
				break;
			case 'm':
				numbermode=2;
				break;
			case 'g':
				numbermode=3;
				break;
			}
		}
		argc -= optind;
		argv += optind;
		if (argc<2) {
			usage(f);
		}
		srcname = *argv;
		argc--;
		argv++;
		break;
	case MFSSETGOAL:
		while ((ch=getopt(argc,argv,"d:A:K:C:slrnhHkmg"))!=-1) {
			switch(ch) {
			case 'd':
				arch_delay = strtoul(optarg,NULL,10);
				break;
			case 'A':
				if (arch_labelscnt>0) {
					fprintf(stderr,"option '-A' defined more than once\n");
					usage(f);
				}
				if (parse_label_expr(optarg,&arch_labelscnt,arch_labelmasks)<0) {
					usage(f);
				}
				break;
			case 'K':
				if (keep_labelscnt>0) {
					fprintf(stderr,"option '-K' defined more than once\n");
					usage(f);
				}
				if (parse_label_expr(optarg,&keep_labelscnt,keep_labelmasks)<0) {
					usage(f);
				}
				break;
			case 'C':
				if (create_labelscnt>0) {
					fprintf(stderr,"option '-C' defined more than once\n");
					usage(f);
				}
				if (parse_label_expr(optarg,&create_labelscnt,create_labelmasks)<0) {
					usage(f);
				}
				break;
			case 'n':
				humode=0;
				break;
			case 'h':
				humode=1;
				break;
			case 'H':
				humode=2;
				break;
			case 'k':
				numbermode=1;
				break;
			case 'm':
				numbermode=2;
				break;
			case 'g':
				numbermode=3;
				break;
			case 'r':
				rflag=1;
				break;
			case 's':
				if (create_mode==CREATE_MODE_LOOSE) {
					fprintf(stderr,"flags '-l' and '-s' are mutually exclusive\n");
					usage(f);
				}
				create_mode = CREATE_MODE_STRICT;
				break;
			case 'l':
				if (create_mode==CREATE_MODE_STRICT) {
					fprintf(stderr,"flags '-l' and '-s' are mutually exclusive\n");
					usage(f);
				}
				create_mode = CREATE_MODE_LOOSE;
				break;
			}
		}
		argc -= optind;
		argv += optind;
		if (arch_labelscnt==0 && arch_delay>0) {
			fprintf(stderr,"option '-A' without '-d'\n");
			usage(f);
		}
		if (arch_delay==0 && arch_labelscnt>0) {
			fprintf(stderr,"option '-d' without '-A'\n");
			usage(f);
		}
		if (keep_labelscnt==0 && arch_labelscnt>0) {
			fprintf(stderr,"option '-A' without '-K'\n");
			usage(f);
		}
		if (keep_labelscnt==0 && create_labelscnt>0) {
			fprintf(stderr,"option '-C' without '-K'\n");
			usage(f);
		}
		if (arch_labelscnt==0 && keep_labelscnt>0) {
			arch_labelscnt = keep_labelscnt;
			for (i=0 ; i<create_labelscnt ; i++) {
				for (j=0 ; j<MASKORGROUP ; j++) {
					arch_labelmasks[i][j] = keep_labelmasks[i][j];
				}
			}
		}
		if (create_labelscnt==0 && keep_labelscnt>0) {
			create_labelscnt = keep_labelscnt;
			for (i=0 ; i<create_labelscnt ; i++) {
				for (j=0 ; j<MASKORGROUP ; j++) {
					create_labelmasks[i][j] = keep_labelmasks[i][j];
				}
			}
		}
		if (keep_labelscnt==0 && create_labelscnt==0 && arch_labelscnt==0) {
			if (argc==0) {
				usage(f);
			}
			p = argv[0];
			// [1-9] | [1-9]+ | [1-9]-
			if (p[0]>'0' && p[0]<='9' && (p[1]=='\0' || ((p[1]=='-' || p[1]=='+') && p[2]=='\0'))) {
				goal = p[0]-'0';
				if (p[1]=='-') {
					smode = SMODE_DECREASE;
				} else if (p[1]=='+') {
					smode = SMODE_INCREASE;
				}
			} else {
				if (parse_label_expr(p,&create_labelscnt,create_labelmasks)<0) {
					usage(f);
				}
				keep_labelscnt = create_labelscnt;
				arch_labelscnt = create_labelscnt;
				for (i=0 ; i<create_labelscnt ; i++) {
					for (j=0 ; j<MASKORGROUP ; j++) {
						arch_labelmasks[i][j] = keep_labelmasks[i][j] = create_labelmasks[i][j];
					}
				}
				goal = 0;
				smode = SMODE_SET;
			}
			argc--;
			argv++;
		} else {
			goal = 0;
			smode = SMODE_SET;
		}
		break;
	case MFSGETGOAL:
	case MFSGETTRASHTIME:
	case MFSSETTRASHTIME:
	case MFSGETEATTR:
	case MFSCHKARCHIVE:
	case MFSSETARCHIVE:
	case MFSCLRARCHIVE:
		while ((ch=getopt(argc,argv,"rnhHkmg"))!=-1) {
			switch(ch) {
			case 'n':
				humode=0;
				break;
			case 'h':
				humode=1;
				break;
			case 'H':
				humode=2;
				break;
			case 'k':
				numbermode=1;
				break;
			case 'm':
				numbermode=2;
				break;
			case 'g':
				numbermode=3;
				break;
			case 'r':
				rflag=1;
				break;
			}
		}
		argc -= optind;
		argv += optind;
		if (f==MFSSETTRASHTIME) {
			p = argv[0];
			if (argc==0) {
				usage(f);
			}
			if (p[0]>='0' && p[0]<='9') {
				trashtime = parse_period(p,&p);
			}
			while (p[0]==' ') {
				p++;
			}
			if (p[0]=='\0' || ((p[0]=='-' || p[0]=='+') && p[1]=='\0')) {
				if (p[0]=='-') {
					smode=SMODE_DECREASE;
				} else if (p[0]=='+') {
					smode=SMODE_INCREASE;
				}
			} else {
				fprintf(stderr,"trashtime should be given as number of seconds optionally folowed by '-' or '+'\n");
				usage(f);
			}
			argc--;
			argv++;
		}
		break;
	case MFSSETEATTR:
	case MFSDELEATTR:
		while ((ch=getopt(argc,argv,"rnhHkmgf:"))!=-1) {
			switch(ch) {
			case 'n':
				humode=0;
				break;
			case 'h':
				humode=1;
				break;
			case 'H':
				humode=2;
				break;
			case 'k':
				numbermode=1;
				break;
			case 'm':
				numbermode=2;
				break;
			case 'g':
				numbermode=3;
				break;
			case 'r':
				rflag=1;
				break;
			case 'f':
				found=0;
				for (i=0 ; found==0 && i<EATTR_BITS ; i++) {
					if (strcmp(optarg,eattrtab[i])==0) {
						found=1;
						eattr|=1<<i;
					}
				}
				if (!found) {
					fprintf(stderr,"unknown flag\n");
					usage(f);
				}
				break;
			}
		}
		argc -= optind;
		argv += optind;
		if (eattr==0 && argc>=1) {
			if (f==MFSSETEATTR) {
				fprintf(stderr,"no attribute(s) to set\n");
			} else {
				fprintf(stderr,"no attribute(s) to delete\n");
			}
			usage(f);
		}
		if (f==MFSSETEATTR) {
			smode = SMODE_INCREASE;
		} else {
			smode = SMODE_DECREASE;
		}
		break;
	case MFSFILEREPAIR:
	case MFSGETQUOTA:
	case MFSCHECKFILE:
		while ((ch=getopt(argc,argv,"nhHkmg"))!=-1) {
			switch(ch) {
			case 'n':
				humode=0;
				break;
			case 'h':
				humode=1;
				break;
			case 'H':
				humode=2;
				break;
			case 'k':
				numbermode=1;
				break;
			case 'm':
				numbermode=2;
				break;
			case 'g':
				numbermode=3;
				break;
			}
		}
		argc -= optind;
		argv += optind;
		break;
	case MFSFILEINFO:
		humode = 0;
		numbermode = 0;
		while ((ch=getopt(argc,argv,"qcs"))!=-1) {
			switch(ch) {
			case 'q':
				fileinfomode |= FILEINFO_QUICK;
				break;
			case 'c':
				fileinfomode |= FILEINFO_CRC;
				break;
			case 's':
				fileinfomode |= FILEINFO_SIGNATURE;
				break;
			}
		}
		argc -= optind;
		argv += optind;
		break;
	case MFSDIRINFO:
		while ((ch=getopt(argc,argv,"nhHkmgidfclsr"))!=-1) {
			switch(ch) {
			case 'n':
				humode=0;
				break;
			case 'h':
				humode=1;
				break;
			case 'H':
				humode=2;
				break;
			case 'k':
				numbermode=1;
				break;
			case 'm':
				numbermode=2;
				break;
			case 'g':
				numbermode=3;
				break;
			case 'i':
				dirinfomode |= DIRINFO_INODES;
				break;
			case 'd':
				dirinfomode |= DIRINFO_DIRS;
				break;
			case 'f':
				dirinfomode |= DIRINFO_FILES;
				break;
			case 'c':
				dirinfomode |= DIRINFO_CHUNKS;
				break;
			case 'l':
				dirinfomode |= DIRINFO_LENGTH;
				break;
			case 's':
				dirinfomode |= DIRINFO_SIZE;
				break;
			case 'r':
				dirinfomode |= DIRINFO_REALSIZE;
				break;
			}
		}
		argc -= optind;
		argv += optind;
		break;
	case MFSSETQUOTA:
		if (getuid()) {
			fprintf(stderr,"only root can change quota\n");
			usage(f);
		}
		while ((ch=getopt(argc,argv,"nhHkmgp:i:I:l:L:s:S:r:R:"))!=-1) {
			switch(ch) {
			case 'n':
				humode=0;
				break;
			case 'h':
				humode=1;
				break;
			case 'H':
				humode=2;
				break;
			case 'k':
				numbermode=1;
				break;
			case 'm':
				numbermode=2;
				break;
			case 'g':
				numbermode=3;
				break;
			case 'p':
				graceperiod = parse_period(optarg,&p);
				if (p[0]!='\0') {
					fprintf(stderr,"bad grace period\n");
					usage(f);
				}
				break;
			case 'i':
				if (my_get_number(optarg,&v,UINT32_MAX,0)<0) {
					fprintf(stderr,"bad inodes limit\n");
					usage(f);
				}
				if (qflags & QUOTA_FLAG_SINODES) {
					fprintf(stderr,"'soft inodes' quota defined twice\n");
					usage(f);
				}
				sinodes = v;
				qflags |= QUOTA_FLAG_SINODES;
				break;
			case 'I':
				if (my_get_number(optarg,&v,UINT32_MAX,0)<0) {
					fprintf(stderr,"bad inodes limit\n");
					usage(f);
				}
				if (qflags & QUOTA_FLAG_HINODES) {
					fprintf(stderr,"'hard inodes' quota defined twice\n");
					usage(f);
				}
				hinodes = v;
				qflags |= QUOTA_FLAG_HINODES;
				break;
			case 'l':
				if (my_get_number(optarg,&v,UINT64_MAX,1)<0) {
					fprintf(stderr,"bad length limit\n");
					usage(f);
				}
				if (qflags & QUOTA_FLAG_SLENGTH) {
					fprintf(stderr,"'soft length' quota defined twice\n");
					usage(f);
				}
				slength = v;
				qflags |= QUOTA_FLAG_SLENGTH;
				break;
			case 'L':
				if (my_get_number(optarg,&v,UINT64_MAX,1)<0) {
					fprintf(stderr,"bad length limit\n");
					usage(f);
				}
				if (qflags & QUOTA_FLAG_HLENGTH) {
					fprintf(stderr,"'hard length' quota defined twice\n");
					usage(f);
				}
				hlength = v;
				qflags |= QUOTA_FLAG_HLENGTH;
				break;
			case 's':
				if (my_get_number(optarg,&v,UINT64_MAX,1)<0) {
					fprintf(stderr,"bad size limit\n");
					usage(f);
				}
				if (qflags & QUOTA_FLAG_SSIZE) {
					fprintf(stderr,"'soft size' quota defined twice\n");
					usage(f);
				}
				ssize = v;
				qflags |= QUOTA_FLAG_SSIZE;
				break;
			case 'S':
				if (my_get_number(optarg,&v,UINT64_MAX,1)<0) {
					fprintf(stderr,"bad size limit\n");
					usage(f);
				}
				if (qflags & QUOTA_FLAG_HSIZE) {
					fprintf(stderr,"'hard size' quota defined twice\n");
					usage(f);
				}
				hsize = v;
				qflags |= QUOTA_FLAG_HSIZE;
				break;
			case 'r':
				if (my_get_number(optarg,&v,UINT64_MAX,1)<0) {
					fprintf(stderr,"bad real size limit\n");
					usage(f);
				}
				if (qflags & QUOTA_FLAG_SREALSIZE) {
					fprintf(stderr,"'soft realsize' quota defined twice\n");
					usage(f);
				}
				srealsize = v;
				qflags |= QUOTA_FLAG_SREALSIZE;
				break;
			case 'R':
				if (my_get_number(optarg,&v,UINT64_MAX,1)<0) {
					fprintf(stderr,"bad real size limit\n");
					usage(f);
				}
				if (qflags & QUOTA_FLAG_HREALSIZE) {
					fprintf(stderr,"'hard realsize' quota defined twice\n");
					usage(f);
				}
				hrealsize = v;
				qflags |= QUOTA_FLAG_HREALSIZE;
				break;
			}
		}
		if (qflags==0) {
			fprintf(stderr,"quota options not defined\n");
			usage(f);
		}
		argc -= optind;
		argv += optind;
		break;
	case MFSDELQUOTA:
		if (getuid()) {
			fprintf(stderr,"only root can change quota\n");
			usage(f);
		}
		while ((ch=getopt(argc,argv,"nhHkmgiIlLsSrRaA"))!=-1) {
			switch(ch) {
			case 'n':
				humode=0;
				break;
			case 'h':
				humode=1;
				break;
			case 'H':
				humode=2;
				break;
			case 'k':
				numbermode=1;
				break;
			case 'm':
				numbermode=2;
				break;
			case 'g':
				numbermode=3;
				break;
			case 'i':
				if (qflags & QUOTA_FLAG_SINODES) {
					fprintf(stderr,"'soft inodes' option given twice\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_SINODES;
				break;
			case 'I':
				if (qflags & QUOTA_FLAG_HINODES) {
					fprintf(stderr,"'hard inodes' option given twice\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_HINODES;
				break;
			case 'l':
				if (qflags & QUOTA_FLAG_SLENGTH) {
					fprintf(stderr,"'soft length' option given twice\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_SLENGTH;
				break;
			case 'L':
				if (qflags & QUOTA_FLAG_HLENGTH) {
					fprintf(stderr,"'hard length' option given twice\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_HLENGTH;
				break;
			case 's':
				if (qflags & QUOTA_FLAG_SSIZE) {
					fprintf(stderr,"'soft size' option given twice\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_SSIZE;
				break;
			case 'S':
				if (qflags & QUOTA_FLAG_HSIZE) {
					fprintf(stderr,"'hard size' option given twice\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_HSIZE;
				break;
			case 'r':
				if (qflags & QUOTA_FLAG_SREALSIZE) {
					fprintf(stderr,"'soft realsize' option given twice\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_SREALSIZE;
				break;
			case 'R':
				if (qflags & QUOTA_FLAG_HREALSIZE) {
					fprintf(stderr,"'hard realsize' option given twice\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_HREALSIZE;
				break;
			case 'a':
				if (qflags & QUOTA_FLAG_SALL) {
					fprintf(stderr,"'all soft quotas' defined together with other soft quota options\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_SALL;
				break;
			case 'A':
				if (qflags & QUOTA_FLAG_HALL) {
					fprintf(stderr,"'all hard quotas' defined together with other hard quota options\n");
					usage(f);
				}
				qflags |= QUOTA_FLAG_HALL;
				break;
			}
		}
		if (qflags==0) {
			fprintf(stderr,"quota options not defined\n");
			usage(f);
		}
		argc -= optind;
		argv += optind;
		break;
	default:
		while (getopt(argc,argv,"")!=-1);
		argc -= optind;
		argv += optind;
//		argc--;	// skip appname
//		argv++;
	}

	if (f==MFSAPPENDCHUNKS) {
		if (argc<=1) {
			usage(f);
		}
		appendfname = argv[0];
		i = open(appendfname,O_RDWR | O_CREAT,0666);
		if (i<0) {
			fprintf(stderr,"can't create/open file: %s\n",appendfname);
			return 1;
		}
		close(i);
		argc--;
		argv++;
	}

	if (argc<1) {
		usage(f);
	}
	status=0;
	if (f==MFSCOPYGOAL) {
		if (get_goal(srcname,&goal,&create_mode,&create_labelscnt,create_labelmasks,&keep_labelscnt,keep_labelmasks,&arch_labelscnt,arch_labelmasks,&arch_delay,GMODE_NORMAL)<0) {
			return 1;
		}
		smode = SMODE_SET;
	} else if (f==MFSCOPYTRASHTIME) {
		if (get_trashtime(srcname,&trashtime,GMODE_NORMAL)<0) {
			return 1;
		}
		smode = SMODE_SET;
	} else if (f==MFSCOPYEATTR) {
		if (get_eattr(srcname,&eattr,GMODE_NORMAL)<0) {
			return 1;
		}
		smode = SMODE_SET;
	} else if (f==MFSCOPYQUOTA) {
		if (quota_control(srcname,2,&qflags,&graceperiod,&sinodes,&slength,&ssize,&srealsize,&hinodes,&hlength,&hsize,&hrealsize)<0) {
			return 1;
		}
	}
	while (argc>0) {
		switch (f) {
		case MFSGETGOAL:
			if (get_goal(*argv,&goal,&create_mode,&create_labelscnt,create_labelmasks,&keep_labelscnt,keep_labelmasks,&arch_labelscnt,arch_labelmasks,&arch_delay,(rflag)?GMODE_RECURSIVE:GMODE_NORMAL)<0) {
				status=1;
			}
			break;
		case MFSSETGOAL:
		case MFSCOPYGOAL:
			if (set_goal(*argv,goal,create_mode,create_labelscnt,create_labelmasks,keep_labelscnt,keep_labelmasks,arch_labelscnt,arch_labelmasks,arch_delay,(rflag)?(smode | SMODE_RMASK):smode)<0) {
				status=1;
			}
			break;
		case MFSGETTRASHTIME:
			if (get_trashtime(*argv,&trashtime,(rflag)?GMODE_RECURSIVE:GMODE_NORMAL)<0) {
				status=1;
			}
			break;
		case MFSSETTRASHTIME:
		case MFSCOPYTRASHTIME:
			if (set_trashtime(*argv,trashtime,(rflag)?(smode | SMODE_RMASK):smode)<0) {
				status=1;
			}
			break;
		case MFSCHECKFILE:
			if (file_info(FILEINFO_QUICK,*argv)<0) {
				status=1;
			}
			break;
		case MFSFILEINFO:
			if (file_info(fileinfomode,*argv)<0) {
				status=1;
			}
			break;
		case MFSAPPENDCHUNKS:
			if (append_file(appendfname,*argv)<0) {
				status=1;
			}
			break;
		case MFSDIRINFO:
			if (dir_info(dirinfomode,*argv)<0) {
				status=1;
			}
			break;
		case MFSFILEREPAIR:
			if (file_repair(*argv)<0) {
				status=1;
			}
			break;
		case MFSGETEATTR:
			if (get_eattr(*argv,&eattr,(rflag)?GMODE_RECURSIVE:GMODE_NORMAL)<0) {
				status=1;
			}
			break;
		case MFSSETEATTR:
		case MFSDELEATTR:
		case MFSCOPYEATTR:
			if (set_eattr(*argv,eattr,(rflag)?(smode | SMODE_RMASK):smode)<0) {
				status=1;
			}
			break;
		case MFSGETQUOTA:
			if (quota_control(*argv,2,&qflags,&graceperiod,&sinodes,&slength,&ssize,&srealsize,&hinodes,&hlength,&hsize,&hrealsize)<0) {
				status=1;
			}
			break;
		case MFSSETQUOTA:
		case MFSCOPYQUOTA:
			if (quota_control(*argv,0,&qflags,&graceperiod,&sinodes,&slength,&ssize,&srealsize,&hinodes,&hlength,&hsize,&hrealsize)<0) {
				status=1;
			}
			break;
		case MFSDELQUOTA:
			if (quota_control(*argv,1,&qflags,&graceperiod,&sinodes,&slength,&ssize,&srealsize,&hinodes,&hlength,&hsize,&hrealsize)<0) {
				status=1;
			}
			break;
		case MFSFILEPATHS:
			if (file_paths(*argv)<0) {
				status=1;
			}
			break;
		case MFSCHKARCHIVE:
			if (archive_control(*argv,ARCHCTL_GET)<0) {
				status=1;
			}
			break;
		case MFSSETARCHIVE:
			if (archive_control(*argv,ARCHCTL_SET)<0) {
				status=1;
			}
			break;
		case MFSCLRARCHIVE:
			if (archive_control(*argv,ARCHCTL_CLR)<0) {
				status=1;
			}
			break;
		case MFSRMSNAPSHOT:
			if (remove_snapshot(*argv,snapmode)<0) {
				status=1;
			}
			break;
		}
		argc--;
		argv++;
	}
	return status;
}
