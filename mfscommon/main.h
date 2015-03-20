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

#ifndef _MAIN_H_
#define _MAIN_H_

#if defined(_THREAD_SAFE) || defined(_REENTRANT) || defined(_USE_PTHREADS)
#  define USE_PTHREADS 1
#endif

#include <poll.h>
#include <sys/types.h>
#include <inttypes.h>
#ifdef USE_PTHREADS
#include <pthread.h>
#endif

void main_destruct_register (void (*fun)(void));
void main_canexit_register (int (*fun)(void));
void main_wantexit_register (void (*fun)(void));
void main_reload_register (void (*fun)(void));
void main_info_register (void (*fun)(void));
void main_chld_register (pid_t pid,void (*fun)(int));
void main_keepalive_register (void (*fun)(void));
void main_poll_register (void (*desc)(struct pollfd *,uint32_t *),void (*serve)(struct pollfd *));
void main_eachloop_register (void (*fun)(void));
void* main_msectime_register (uint32_t mseconds,uint32_t offset,void (*fun)(void));
int main_msectime_change(void* x,uint32_t mseconds,uint32_t offset);
void* main_time_register (uint32_t seconds,uint32_t offset,void (*fun)(void));
int main_time_change(void *x,uint32_t seconds,uint32_t offset);
void main_exit(void);
uint32_t main_time(void);
void main_keep_alive(void);
#ifdef USE_PTHREADS
int main_thread_create(pthread_t *th,const pthread_attr_t *attr,void *(*fn)(void *),void *arg);
int main_minthread_create(pthread_t *th,uint8_t detached,void *(*fn)(void *),void *arg);
#endif

#endif
