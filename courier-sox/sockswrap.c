/*
**
** Copyright 2004 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "courier_socks_config.h"
#include "socks.h"

#define GET_SOCKADDR_UNION(a) ((a).__sockaddr__)
#define GET_SOCKADDR_DIRECT(a) (a)

#define GET_SOCKADDR(a) GET_SOCKADDR_PTR(a)

/*
** libsockswrap intercepts the system functions by forwarding them to
** the socksing version in libsocks
*/

int connect(int fd, SOCKADDR_CONST_ARG_PTR addr, SOCKLEN_T addrlen)
{
	return Rconnect(fd, GET_SOCKADDR(addr), addrlen);
}

int bind(int fd, SOCKADDR_CONST_ARG_PTR addr, SOCKLEN_T addrlen)
{
	return Rbind(fd, GET_SOCKADDR(addr), addrlen);
}

int close(int fd)
{
	return Rclose(fd);
}

int fclose(FILE *fp)
{
	return Rfclose(fp);
}

int select(int n, fd_set *r, fd_set *w, fd_set *e,
	   struct timeval *tv)
{
	return Rselect(n, r, w, e, tv);
}

int poll(struct pollfd *ufds, NFDS_T nfds, int timeout)
{
	return Rpoll(ufds, nfds, timeout);
}

int listen(int s, int backlog)
{
	return Rlisten(s, backlog);
}

int dup(int fd)
{
	return Rdup(fd);
}

int dup2(int fd, int fdNew)
{
	return Rdup2(fd, fdNew);
}

int accept(int s, SOCKADDR_ARG_PTR addr, SOCKLEN_T *addrlen)
{
	return Raccept(s, GET_SOCKADDR(addr), addrlen);
}

int getsockopt(int  s, int level, int optname, void *optval,
	       SOCKLEN_T *optlen)
{
	return Rgetsockopt(s, level, optname, optval, optlen);
}

int getsockname(int s, SOCKADDR_ARG_PTR name, SOCKLEN_T *namelen)
{
	return Rgetsockname(s, GET_SOCKADDR(name), namelen);
}

int getpeername(int s, SOCKADDR_ARG_PTR name, SOCKLEN_T *namelen)
{
	return Rgetpeername(s, GET_SOCKADDR(name), namelen);
}

