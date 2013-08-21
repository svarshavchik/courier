/*
**
** Copyright 2004 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stubfuncs_h
#define stubfuncs_h

#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int (*ptr_connect)(int fd, const struct sockaddr *addr,
			  SOCKLEN_T addrlen);

static int (*ptr_bind)(int fd, const struct sockaddr *addr, SOCKLEN_T addrlen);

static int (*ptr_select)(int n, fd_set *r, fd_set *w, fd_set *e,
			 struct timeval *tv);

static int (*ptr_getsockopt)(int  s, int level, int optname, void *optval,
			     SOCKLEN_T *optlen);
static int (*ptr_close)(int fd);
static int (*ptr_fclose)(FILE *fp);
static int (*ptr_listen)(int s, int backlog);
static int (*ptr_accept)(int s, struct sockaddr *addr, SOCKLEN_T *addrlen);
static int (*ptr_getsockname)(int s, struct sockaddr *name,
			      SOCKLEN_T *namelen);
static int (*ptr_getpeername)(int s, struct sockaddr *name,
			      SOCKLEN_T *namelen);
static int (*ptr_dup)(int fd);
static int (*ptr_dup2)(int fd, int fdto);

#if HAVE_POLL
static int (*ptr_poll)(struct pollfd *ufds, unsigned int nfds, int timeout);
#endif

#endif
