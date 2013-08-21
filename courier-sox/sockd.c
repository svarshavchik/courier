/*
**
** Copyright 2004-2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "courier_socks_config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif
#if HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#if HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <gdbm.h>
#include <courierauth.h>
#include	"printaddr.h"
#include	"toipv4.h"
#include	"cidr.h"
#include	"soxwrap/mksocket.h"

static int debug_level=0;
static int nprocs=5;

static int preforkmasterpipefd[2];
static int *preforkedchildpipes;
static struct pollfd *preforkpollfd;
static int npreforkedchildpipes;

#define DEBUG_CONFIG 1
#define DEBUG_CONNECT 2
#define DEBUG_CONNECTED 16
#define DEBUG_BLACKLIST 32

#define D(n) if (debug_level & (n))

static int sighup_received=0;

static void sigexit(int n)
{
	kill( -getpid(), SIGTERM);
	_exit(0);
}

static void sighup(int n)
{
	sighup_received=1;

	signal(SIGHUP, sighup);
}

struct portinfo {
	struct portinfo *next;
	int fd1, fd2;
};

struct conninfo {
	int clientfd;
	const char *authuserid;
	SOCKADDR_STORAGE clientaddr;
	SOCKADDR_STORAGE myaddr;
	SOCKADDR_STORAGE remoteaddr;
};

struct accesslist {
	struct accesslist *next;
	CIDR from, to;
	SOCKADDR_STORAGE bindAs;
	int bindAsFlag;
	int authenticated;

	char *block4filename;

};

static struct accesslist *accesslist_first=NULL,
	**accesslist_last=&accesslist_first;

static struct portinfo *portlist=NULL;

static int search_old_sockets(int(*callback_func)(int, void *),
			      void *callback_arg, void *voidarg)
{
	struct portinfo *p=(struct portinfo *)voidarg;

	while (p)
	{
		int rc;

		if (p->fd1 >= 0 &&
		    (rc=(*callback_func)(p->fd1, callback_arg)) != 0)
			return rc;
		if (p->fd2 >= 0 &&
		    (rc=(*callback_func)(p->fd2, callback_arg)) != 0)
			return rc;
		p=p->next;
	}
	return 0;
}

static void createsocket(const char *address,
			 const char *port,
			 struct portinfo *oldportinfo)
{
	struct portinfo *p=malloc(sizeof(struct portinfo));

	if (!p)
	{
		perror("CRIT: malloc");
		return;
	}

	D(DEBUG_CONFIG)
		{
			fprintf(stderr,
				"DEBUG: Creating socket on %s, port %s\n",
				address ? address:"*",
				port);
			fflush(stderr);
		}

	if (mksocket(address, port, SOCK_STREAM, &p->fd1, &p->fd2,
		     &search_old_sockets, oldportinfo))
	{
		fprintf(stderr,
			"CRIT: Cannot create socket on %s, port %s: %s\n",
			address ? address:"*",
			port, strerror(errno));
		fflush(stderr);
		return;
	}

	p->next=portlist;
	portlist=p;
}

static int startserver(int *pipefd);

static int startserver_orbust(int *pipefd)
{
	int rc;

	while ((rc=startserver(pipefd)) < 0)
	{
		sleep(5);
	}
	return rc;
}

static int startserver(int *pipefd)
{
	pid_t p1;
	pid_t p2;
	int newpipefd[2];

	if (pipe(newpipefd) < 0)
	{
		perror("CRIT: pipe");
		return (-1);

	}

	p1=fork();
	if (p1 < 0)
	{
		perror("CRIT: fork");
		return -1;
	}

	if (p1 == 0)
	{
		/*
		** First child process starts a second child process,
		** then exits.
		*/

		p2=fork();

		if (p2 < 0)
		{
			perror("ERR: fork");
			exit(1);
		}

		if (p2 == 0)
		{
			int n;

			/*
			** Second child closes the read side of the child
			** pipe, closes pipes from all other children,
			** then returns the read side of the pipe.
			*/

			close(newpipefd[0]);
			close(preforkmasterpipefd[1]);

			for (n=0; n<npreforkedchildpipes; n++)
				if (preforkedchildpipes[n] >= 0)
					close(preforkedchildpipes[n]);
			*pipefd=newpipefd[1];
			return 0;
		}
		exit(0);
	}

	/*
	** Parent waits for the first child process to exit.
	*/
	close(newpipefd[1]);

	while ((p2=wait(NULL)) != p1)
		;
	*pipefd=newpipefd[0];
	return 1;
}

static void writefd(struct conninfo *ci,
		    const void *voidptr,
		    size_t len)
{
	time_t timeout=time(NULL)+30;
	struct pollfd pfd;
	const char *ptr=(const char *)voidptr;

	while (len)
	{
		time_t now=time(NULL);
		int n;

		if (now >= timeout)
			break;

		pfd.fd=ci->clientfd;
		pfd.events=POLLOUT;
		poll(&pfd, 1, (timeout-now)*1000);

		n=write(ci->clientfd, ptr, len);

		if (n < 0)
		{
			fprintf(stderr, "ERR: ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, " - error: %s\n", strerror(errno));
			fflush(stderr);
			exit(0);
		}

		if (n == 0)
			break;
		ptr += n;
		len -= n;
	}

	if (len)
	{
		fprintf(stderr, "ERR: ");
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, " - timeout\n");
		fflush(stderr);
		exit(0);
	}
}

static void readfd(struct conninfo *ci,
		   void *voidptr,
		   size_t len)
{
	time_t timeout=time(NULL)+30;
	struct pollfd pfd;
	char *ptr=(char *)voidptr;

	while (len)
	{
		time_t now=time(NULL);
		int n;

		if (now >= timeout)
			break;

		pfd.fd=ci->clientfd;
		pfd.events=POLLIN;
		poll(&pfd, 1, (timeout-now)*1000);

		if ((pfd.revents & POLLIN) == 0)
			break;

		n=read(ci->clientfd, ptr, len);

		if (n < 0)
		{
			fprintf(stderr, "ERR: ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, " - error: %s\n", strerror(errno));
			fflush(stderr);
			exit(0);
		}

		if (n == 0)
		{
			fprintf(stderr, "ERR: ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, " - connection closed by remote host\n");
			fflush(stderr);
			exit(0);
		}
		ptr += n;
		len -= n;
	}

	if (len)
	{
		fprintf(stderr, "ERR: ");
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, " - timeout\n");
		fflush(stderr);
		exit(0);
	}
}

static int auth_cb(struct authinfo *dummy, void *dummy2)
{
	return 0;
}

static int validateuseridpw(const char *uid,
			    const char *pw)
{
	return auth_login("socks", uid, pw, auth_cb, NULL);
}

static char *authenticate(struct conninfo *ci)
{
	char buf[1024];
	int n;
	int noauth=0;
	int uidpw=0;

	D(DEBUG_CONNECT)
		{
			fprintf(stderr, "DEBUG: Connection from ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, "\n");
			fflush(stderr);
		}

	readfd(ci, buf, 2);

	if (buf[0] != 5)
	{
		fprintf(stderr, "ERR: ");
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, " - received unknown protocol version: %d\n",
			(int)(unsigned char)buf[1]);
		fflush(stderr);
		exit(0);
	}

	readfd(ci, buf+2, (int)(unsigned char)buf[1]);

	for (n=0; n<(int)(unsigned char)buf[1]; n++)
		switch (buf[2+n]) {
		case 0:
			noauth=1;
			break;
		case 2:
			uidpw=1;
			break;
		}

	if (uidpw)
	{
		int uidl;
		int pwdl;
		char *pw;

		buf[0]=5;
		buf[1]=2;
		writefd(ci, buf, 2);

		readfd(ci, buf, 2);
		if (buf[0] != 1)
		{
			fprintf(stderr, "ERR: ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, " - received unknown userid/password authentication version: %d\n",
				(int)(unsigned char)buf[0]);
			fflush(stderr);
			exit(0);
		}
		uidl=(int)(unsigned char)buf[1];

		readfd(ci, buf, uidl+1);

		pw=buf+uidl;
		pwdl=(int)(unsigned char)*pw;
		*pw++=0;
		readfd(ci, pw, pwdl);
		pw[pwdl]=0;

		if (buf[0] == 0)
		{
			/* Empty userid - no authentication */

			buf[0]=5;
			buf[1]=0;
			writefd(ci, buf, 2);

			D(DEBUG_CONNECT)
				{
					fprintf(stderr,"DEBUG: Empty userid - unauthenticated connection from ");
					printaddr(stderr, &ci->clientaddr);
					fprintf(stderr, "\n");
					fflush(stderr);
				}
			return NULL;
		}

		if (validateuseridpw(buf, pw))
		{
			buf[0]=5;
			buf[1]=1;
			writefd(ci, buf, 2);
			fprintf(stderr, "ERR: ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, " - userid/password invalid.\n");
			fflush(stderr);
			fcntl(ci->clientfd, F_SETFL, 0);
			close(ci->clientfd);
			exit(0);
		}

		pw=strdup(buf);

		buf[0]=5;
		buf[1]=0;
		writefd(ci, buf, 2);

		D(DEBUG_CONNECT)
			{
				fprintf(stderr, "DEBUG: Connection from ");
				printaddr(stderr, &ci->clientaddr);
				fprintf(stderr, " authenticated as %s\n",
					buf);
				fflush(stderr);
			}

		if (!pw)
		{
			perror("ERR: malloc");
			exit(0);
		}
		return pw;
	}
	else if (noauth)
	{
		D(DEBUG_CONNECT)
			{
				fprintf(stderr,"DEBUG: Unauthenticated connection from ");
				printaddr(stderr, &ci->clientaddr);
				fprintf(stderr, "\n");
				fflush(stderr);
			}
		buf[0]=5;
		buf[1]=0;
		writefd(ci, buf, 2);
	}
	else
	{
		fprintf(stderr,"DEBUG: Client from ");
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, " did not provide a valid authentication mechanism.\n");
		buf[0]=5;
		buf[1]=(char)255;
		writefd(ci, buf, 2);
		fcntl(ci->clientfd, F_SETFL, 0);
		close(ci->clientfd);
		exit(0);
	}

	return NULL;
}

static int getrequest(struct conninfo *ci,
		      SOCKADDR_STORAGE **reqaddr,
		      int *reqcnt)
{
	char buf[512];
	union {
		SOCKADDR_STORAGE ss;
#if HAVE_IPV6
		struct sockaddr_in6 in6;
#endif
		struct sockaddr_in in;
	} sas;

	socklen_t saslen;
	int cmd;

	readfd(ci, buf, 4);

	if (buf[0] != 5)
	{
		fprintf(stderr,"DEBUG: Unknown command version %d from ",
			buf[0]);
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, "\n");
		fflush(stderr);
		exit(0);
	}

	if (buf[2])
	{
		fprintf(stderr,"DEBUG: Unknown command subversion %d from ",
			buf[2]);
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, "\n");
		fflush(stderr);
		exit(0);
	}

	cmd=buf[1];

#if HAVE_IPV6
	if (buf[3] == 4)
	{
		struct sockaddr_in6 *in6;

		readfd(ci, buf, 18);

		in6=&sas.in6;

		memset(in6, 0, sizeof(*in6));
		in6->sin6_family=AF_INET6;
		memcpy(&in6->sin6_addr, buf, 16);
		memcpy(&in6->sin6_port, buf+16, 2);
		saslen=sizeof(*in6);
	}
	else
#endif
		if (buf[3] == 1)
		{
			struct sockaddr_in *in;

			readfd(ci, buf, 6);

			in=&sas.in;

			memset(in, 0, sizeof(*in));
			in->sin_family=AF_INET;
			memcpy(&in->sin_addr, buf, 4);
			memcpy(&in->sin_port, buf+4, 2);
			saslen=sizeof(*in);
		}
		else if (buf[3] == 3)
		{
#if HAVE_GETADDRINFO
			struct addrinfo hints;
			int len;
			char service[10];
			int errcode;
			struct addrinfo *res, *ptr;
			int cnt;

			readfd(ci, buf, 1);

			len=(int)(unsigned char)buf[0];

			readfd(ci, buf, len);
			buf[len]=0;
			readfd(ci, buf+len+1, 2);

			sprintf(service, "%d",
				(int)(unsigned char)buf[len+1] * 256 +
				(unsigned char)buf[len+2]);

			D(DEBUG_CONNECT)
				{
					fprintf(stderr,
						"DEBUG: Connection from ");
					printaddr(stderr, &ci->clientaddr);
					fprintf(stderr,
						" resolution via getaddrinfo() for %s(%s)\n",
						buf, service);
					fflush(stderr);
				}

			memset(&hints, 0, sizeof(hints));

			hints.ai_family=PF_UNSPEC;
			hints.ai_socktype=SOCK_STREAM;

			if ((errcode=getaddrinfo(buf, service, &hints, &res)))
			{
				fprintf(stderr,
					"DEBUG: address resolution from ");
				printaddr(stderr, &ci->clientaddr);
				fprintf(stderr,
					" for %s, port %s, failed: %s\n",
					buf, service, gai_strerror(errcode));
				fflush(stderr);
				exit(0);
			}

			for (ptr=res, cnt=0; ptr; ptr=ptr->ai_next)
				++cnt;

			*reqcnt=cnt;
			if ((*reqaddr=malloc(cnt * sizeof(SOCKADDR_STORAGE)))
			    == NULL)
			{
				perror("ERR: malloc");
				exit(0);
			}

			memset(*reqaddr, 0, cnt * sizeof(SOCKADDR_STORAGE));

			for (ptr=res, cnt=0; ptr; ptr=ptr->ai_next, ++cnt)
				memcpy( (*reqaddr)+cnt,
					ptr->ai_addr,
					ptr->ai_addrlen);

			freeaddrinfo(res);
			return (cmd);
#else
			struct hostent *he;
			int len;
			int service;
			int cnt;

			readfd(ci, buf, 1);

			len=(int)(unsigned char)buf[0];

			readfd(ci, buf, len);
			buf[len]=0;
			readfd(ci, buf+len+1, 2);

			service=(int)(unsigned char)buf[len+1] * 256 +
				(unsigned char)buf[len+2];

			D(DEBUG_CONNECT)
				{
					fprintf(stderr,
						"DEBUG: Connection from ");
					printaddr(stderr, &ci->clientaddr);
					fprintf(stderr,
						" resolution via gethostbyname() for %s(%d)\n",
						buf, service);
					fflush(stderr);
				}
			he=gethostbyname(buf);
			if (he == NULL || he->h_addr_list[0] == NULL)
			{
				fprintf(stderr,
					"DEBUG: address resolution from ");
				printaddr(stderr, &ci->clientaddr);
				fprintf(stderr,
					" for %s, port %d, failed\n",
					buf, service);
				fflush(stderr);
				exit(0);
			}

			for (cnt=0; he->h_addr_list[cnt]; ++cnt)
				;

			*reqcnt=cnt;
			if ((*reqaddr=malloc(cnt * sizeof(SOCKADDR_STORAGE)))
			    == NULL)
			{
				perror("ERR: malloc");
				exit(0);
			}

			memset(*reqaddr, 0, cnt * sizeof(SOCKADDR_STORAGE));


			for (cnt=0; he->h_addr_list[cnt]; ++cnt)
#ifdef HAVE_IPV6
				if (he->h_addrtype == AF_INET6)
				{
					struct sockaddr_in6 *sin6=
						(struct sockaddr_in6 *)
						(*reqaddr) + cnt;

					sin6->sin6_family=AF_INET6;
					memcpy(&sin6->sin6_addr,
					       he->h_addr_list[cnt],
					       sizeof(sin6->sin6_addr));
					sin6->sin6_port=htons(service);
				}
				else
#endif
				{
					struct sockaddr_in *sin=
						(struct sockaddr_in *)
						(*reqaddr) + cnt;

					sin->sin_family=AF_INET;
					memcpy(&sin->sin_addr,
					       he->h_addr_list[cnt],
					       sizeof(sin->sin_addr));
					sin->sin_port=htons(service);
				}

			endhostent();
			return cmd;
#endif
		}
		else
		{
			fprintf(stderr,
				"DEBUG: Unknown address format %d from ",
				buf[3]);
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, "\n");
			fflush(stderr);
			exit(0);
		}

	if ((*reqaddr=(SOCKADDR_STORAGE *)malloc(saslen)) == NULL)
	{
		perror("ERR: malloc");
		exit(0);
	}

	memcpy(*reqaddr, &sas, saslen);
	*reqcnt=1;
	return (cmd);
}

static void reply(struct conninfo *ci,
		  int rep,
		  const SOCKADDR_STORAGE *sa)
{
	char buf[256];
	int len;

	buf[0]=5;
	buf[1]=(char)rep;
	buf[2]=0;

#if HAVE_IPV6
	if (sa && ((const struct sockaddr_in6 *)sa)->sin6_family == AF_INET6)
	{
		const struct sockaddr_in6 *sin6=
			((const struct sockaddr_in6 *)sa);

		buf[3]=4;
		memcpy(buf+4, &sin6->sin6_addr, 16);
		memcpy(buf+20, &sin6->sin6_port, 2);
		len=22;
	}
	else
#endif
	{
		const struct sockaddr_in *sin=
			((const struct sockaddr_in *)sa);

		buf[3]=1;
		if (sa == NULL)
			memset(buf+4, 0, 6);
		else
		{
			memcpy(buf+4, &sin->sin_addr, 4);
			memcpy(buf+8, &sin->sin_port, 2);
		}
		len=10;
	}
	writefd(ci, buf, len);
}

static int confsocket(int fd, struct conninfo *ci)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK))
	{
		fprintf(stderr, "DEBUG: fcntl(O_NONBLOCK) failed for ");
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, ": %s\n", strerror(errno));
		fflush(stderr);
		close(fd);
		return (-1);
	}

	{
		int dummy=1;

		setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
			   (const char *)&dummy, sizeof(dummy));
	}
	{
		int dummy=1;

		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			   (const char *)&dummy, sizeof(dummy));
	}
	return (fd);
}

static struct accesslist *accessallowed(struct conninfo *ci,
					const SOCKADDR_STORAGE *sa)
{
	struct accesslist *a;
	struct accesslist *found=NULL;
	GDBM_FILE gdbm;
	struct sockaddr_in in4;

	for (a=accesslist_first; a; a=a->next)
	{
		if (incidr(&a->from, &ci->clientaddr) == 0 &&
		    incidr(&a->to, sa) == 0)
		{
			if (a->authenticated && !ci->authuserid)
				continue;

			if (found == NULL)
			{
				found=a;
			}
			else
			{
				/*
				** Rules:
				**
				** Rules with a larger "from" prefix take
				** precendence over rules with a smaller "from"
				** prefix.
				**
				** Rules with the same "from" prefix and a
				** larger "to" prefix take precendence over
				** rules with the same "from" prefix and a
				** smaller "to" prefix.
				*/

				if (a->from.pfix < found->from.pfix)
					continue;

				if (a->from.pfix > found->from.pfix)
				{
					found=a;
					continue;
				}

				if (a->to.pfix < found->to.pfix)
					continue;

				found=a;
			}
		}
	}

	if (found && found->block4filename &&
	    toipv4(&in4, sa) == 0 &&
	    (gdbm=gdbm_open(found->block4filename, 0,
			    GDBM_READER, 0644, 0)))
	{
		const char *filename=found->block4filename;
		unsigned char *addrp=(unsigned char *)&in4.sin_addr.s_addr;
		datum   dkey, dval;

		D(DEBUG_BLACKLIST)
			{
				fprintf(stderr, "DEBUG: Looking up ");
				printaddr(stderr, (const SOCKADDR_STORAGE *)
					  &in4);
				fprintf(stderr, " in %s\n", filename);
				fflush(stderr);
			}

		dkey.dptr=(char *)addrp;
		dkey.dsize=2;

		dval=gdbm_fetch(gdbm, dkey);

		if (dval.dptr)
		{
			int i;

			for (i=0; i<dval.dsize; i += 3)
			{
				struct in_addr addr2;
				unsigned char *addr2_p=
					(unsigned char *)&addr2.s_addr;
				char tempbuf[100];
				CIDR c;

				memcpy(addr2_p, addrp, 2);
				memcpy(addr2_p+2, dval.dptr + i, 2);

				strcpy(tempbuf, inet_ntoa(addr2));
				sprintf(tempbuf+strlen(tempbuf), "/%d",
					(int)(unsigned char)
					dval.dptr[i+2]);

				D(DEBUG_BLACKLIST)
					{
						fprintf(stderr, "DEBUG: Checking if ");
						printaddr(stderr, (const SOCKADDR_STORAGE *)
							  &in4);
						fprintf(stderr, " is in %s\n",
							tempbuf);
						fflush(stderr);
					}

				if (tocidr(&c, tempbuf) == 0 &&
				    incidr(&c, sa) == 0)
				{
					found=NULL;
					break;
				}

			}
			free(dval.dptr);
		}
		gdbm_close(gdbm);

		if (found == NULL)
		{
			fprintf(stderr, "INFO: Connection to ");
			printaddr(stderr, sa);
			fprintf(stderr, " denied by %s\n", filename);
			fflush(stderr);
		}
	}

	if (found)
	{
		return found;
	}

	fprintf(stderr, "INFO: Permission denied for %s connection from ",
		ci->authuserid ? "authenticated":"anonymous");
	printaddr(stderr, &ci->clientaddr);
	fprintf(stderr, " to ");
	printaddr(stderr, sa);
	fprintf(stderr, "\n");
	fflush(stderr);
	return NULL;
}

static int newsocket(struct conninfo *ci,
		     SOCKADDR_STORAGE *sa)
{
	int fd=socket(((struct sockaddr *)sa)->sa_family, SOCK_STREAM, 0);

	if (fd < 0)
	{
		fprintf(stderr, "DEBUG: Cannot create socket for ");
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, ": %s\n", strerror(errno));
		fflush(stderr);
		return (-1);
	}

	return confsocket(fd, ci);
}

static void mkreplyerrno(struct conninfo *ci,
			 SOCKADDR_STORAGE *p)
{
	D(DEBUG_CONNECT)
		{
			fprintf(stderr,
				"DEBUG: Connection attempt from ");
			printaddr(stderr, &ci->clientaddr);
			if (p)
			{
				fprintf(stderr, " to ");
				printaddr(stderr, p);
			}
			fprintf(stderr, " failed.\n");
			fflush(stderr);
		}

	switch (errno) {
	case EPERM:
		reply(ci, 0x02, p);
		break;
#ifdef EACCESS
	case EACCESS:
		reply(ci, 0x02, p);
		break;
#endif
#ifdef ENETRESET
	case ENETRESET:
		reply(ci, 0x03, p);
		break;
#endif
#ifdef ENETUNREACH
	case ENETUNREACH:
		reply(ci, 0x03, p);
		break;
#endif
#ifdef ENETDOWN
	case ENETDOWN:
		reply(ci, 0x03, p);
		break;
#endif
#ifdef EHOSTUNREACH
	case EHOSTUNREACH:
		reply(ci, 0x04, p);
		break;
#endif
#ifdef ECONNREFUSED
	case ECONNREFUSED:
		reply(ci, 0x05, p);
		break;
#endif
#ifdef ETIMEDOUT
	case ETIMEDOUT:
		reply(ci, 0x06, p);
		break;
#endif
#ifdef EAFNOSUPPORT
	case EAFNOSUPPORT:
		reply(ci, 0x08, p);
		break;
#endif
#ifdef ENOPROTOOPT
	case ENOPROTOOPT:
		reply(ci, 0x08, p);
		break;
#endif
	default:
		reply(ci, 0x01, p);
	}
}

static int proxyconnect(struct conninfo *ci,
			SOCKADDR_STORAGE *addrs,
			int rsacnt)
{
	int i;
	int fd= -1;
	SOCKADDR_STORAGE *p=NULL;
	socklen_t myaddr_len;

	for (i=0; i<rsacnt; i++)
	{
		struct pollfd pfd[2];
		int dummy;
		socklen_t dummylen;

		p=addrs+i;

		if (accessallowed(ci, p) == NULL)
			continue;

		D(DEBUG_CONNECT)
			{
				fprintf(stderr,
					"DEBUG: Trying to open a proxy for ");
				printaddr(stderr, &ci->clientaddr);
				fprintf(stderr, " to ");
				printaddr(stderr, p);

				if (ci->authuserid)
					fprintf(stderr,
						" (authenticated as %s)",
						ci->authuserid);

				fprintf(stderr, "...\n");
				fflush(stderr);
			}
		if ((fd=newsocket(ci, p)) < 0)
			return (-1);

		if (connect(fd, (const struct sockaddr *)p,
#if HAVE_IPV6
			    ((const struct sockaddr *)p)->sa_family
			    == AF_INET6 ? sizeof(struct sockaddr_in6):
#endif
			    sizeof(struct sockaddr_in)) == 0)
			break;

		if (errno == EINPROGRESS)
		{
			pfd[0].fd=fd;
			pfd[0].events=POLLOUT;

			pfd[1].fd=ci->clientfd;
			pfd[1].events=POLLIN;

			poll(pfd, 2, -1);

			if (!(pfd[0].revents & POLLOUT))
			{
				close(fd);
				D(DEBUG_CONNECT)
					{
						fprintf(stderr,
							"DEBUG: Connection attempt from ");
						printaddr(stderr, &ci->clientaddr);
						fprintf(stderr, " cancelled by client.\n");
						fflush(stderr);
					}
				return (-1);
				/* Client socket became readable - client
				** aborted?
				*/
			}

			dummylen=sizeof(dummy);

			if (getsockopt(fd, SOL_SOCKET, SO_ERROR,
				       &dummy, &dummylen)<0)
				continue;
			if (dummy == 0)
				break;

			errno=dummy;
		}

		close(fd);
		fd= -1;

		D(DEBUG_CONNECT)
			{
				fprintf(stderr,
					"DEBUG: Connection attempt from ");
				printaddr(stderr, &ci->clientaddr);
				fprintf(stderr, " to ");
				printaddr(stderr, p);
				fprintf(stderr, " failed: %s\n",
					strerror(errno));
				fflush(stderr);
			}
	}

	if (fd < 0)
	{
		mkreplyerrno(ci, p);
		close(fd);
		return (-1);
	}

	myaddr_len=sizeof(ci->myaddr);
	if (getsockname(fd, (struct sockaddr *)&ci->myaddr, &myaddr_len) < 0)
	{
		fprintf(stderr,
			"DEBUG: getsockname failed from ");
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, " to ");
		printaddr(stderr, p);
		fprintf(stderr, ": %s\n", strerror(errno));
		mkreplyerrno(ci, p);
		fflush(stderr);
		close(fd);
		return (-1);
	}
	reply(ci, 0x00, &ci->myaddr);

	D(DEBUG_CONNECTED)
		{
			fprintf(stderr,
				"DEBUG: Proxy connection established: ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, "->");
			printaddr(stderr, p);
			if (ci->authuserid)
				fprintf(stderr,	" (authenticated as %s)",
					ci->authuserid);
			fprintf(stderr, "\n");
			fflush(stderr);
		}
	ci->remoteaddr= *p;
	return fd;
}

static int getportnum(SOCKADDR_STORAGE *sa)
{
	int portnum;

	switch (sa->SS_family)
	{
#ifdef HAVE_IPV6
	case AF_INET6:
		{
			struct sockaddr_in6 sin6;

			memcpy(&sin6, sa, sizeof(sin6));

			portnum=ntohs(sin6.sin6_port);
		}
		break;
#endif

	case AF_INET:
		{
			struct sockaddr_in sin;

			memcpy(&sin, sa, sizeof(sin));

			portnum=ntohs(sin.sin_port);
		}
		break;

	default:
		portnum= -1;
	}

	return portnum;
}

static void setportnum(SOCKADDR_STORAGE *sa, int portnum)
{
	switch (sa->SS_family) {
#ifdef HAVE_IPV6
	case AF_INET6:
		{
			struct sockaddr_in6 sin6;

			memcpy(&sin6, sa, sizeof(sin6));

			sin6.sin6_port=htons(portnum);

			memcpy(sa, &sin6, sizeof(sin6));
		}
		break;
#endif
	case AF_INET:
		{
			struct sockaddr_in sin;

			memcpy(&sin, sa, sizeof(sin));

			sin.sin_port=htons(portnum);

			memcpy(sa, &sin, sizeof(sin));
		}
	}
}

static int trybind(struct conninfo *ci,
		   SOCKADDR_STORAGE *p,
		   struct accesslist *a)
{
	int fd;
	socklen_t myaddr_len;
	SOCKADDR_STORAGE addrBuf;

	if ((fd=newsocket(ci, p)) < 0)
		return (-1);

	if (a->bindAsFlag)
	{
		int portnum;

		addrBuf=a->bindAs;

		portnum=getportnum(p);

		setportnum(&addrBuf, portnum);

#if HAVE_IPV6

		/*
		** If the original socket is an IPv4 socket,
		** we can only bind to an IPv4 address.
		*/

		if (p->SS_family == AF_INET && addrBuf.SS_family == AF_INET6)
		{
			char tmpbuf[INET6_ADDRSTRLEN+1];
			struct sockaddr_in sin;

			if (inet_ntop(AF_INET6,
				      &((struct sockaddr_in6 *)&addrBuf)
				      ->sin6_addr, tmpbuf, sizeof(tmpbuf))
			    == NULL)
			{
				close(fd);
				return -1;
			}

			memset(&sin, 0, sizeof(sin));
			sin.sin_family=AF_INET;

			if (strncmp(tmpbuf, "::ffff:", 7) ||
			    inet_pton(AF_INET, tmpbuf+7, &sin.sin_addr) <= 0)
			{
				errno=EAFNOSUPPORT;
				close(fd);
				return -1;
			}

			memcpy(&addrBuf, &sin, sizeof(sin));
			setportnum(&addrBuf, portnum);
		}
#endif

		p=&addrBuf;
	}

	myaddr_len=sizeof(ci->myaddr);

	if (bind(fd, (const struct sockaddr *)p,
#if HAVE_IPV6
		 ((const struct sockaddr *)p)->sa_family
		 == AF_INET6 ? sizeof(struct sockaddr_in6):
#endif
		 sizeof(struct sockaddr_in)) >= 0 &&
	    listen(fd,
#ifdef SOMAXCONN
		   SOMAXCONN
#else
		   5
#endif
		   ) >= 0 &&
	    getsockname(fd, (struct sockaddr *)&ci->myaddr,
			&myaddr_len) >= 0)
		return (fd);

	close(fd);
	return -1;
}

static int proxybind(struct conninfo *ci,
		     SOCKADDR_STORAGE *addrs,
		     int rsacnt)
{
	int i;
	int fd= -1;
	SOCKADDR_STORAGE *p=NULL;
	socklen_t myaddr_len;

	int remotePort=getportnum(&ci->clientaddr);

	for (i=0; i<rsacnt; i++)
	{
		struct pollfd pfd[2];
		int bindport;
		struct accesslist *a;

		p=addrs+i;
		if ((a=accessallowed(ci, p)) == NULL)
			continue;

		D(DEBUG_CONNECT)
			{
				fprintf(stderr,
					"DEBUG: Trying to bind a proxy for ");
				printaddr(stderr, &ci->clientaddr);
				fprintf(stderr, " to ");
				printaddr(stderr, p);
				fprintf(stderr, "...\n");
				fflush(stderr);
			}

		bindport=getportnum(p);

		if (bindport < 0 ||
		    (bindport > 0 && bindport < 1024 && remotePort >= 1024))
		{
			fprintf(stderr,
				"DEBUG: Remote client ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, " not authorized to bind port %d\n",
				bindport);
			fflush(stderr);
			errno=EPERM;
			continue;
		}

		fd=trybind(ci, p, a);

		if (fd < 0 && bindport > 0 && bindport < 1024)
		{
			D(DEBUG_CONNECT)
				{
					fprintf(stderr,
						"DEBUG: Bind attempt from ");
					printaddr(stderr, &ci->clientaddr);
					fprintf(stderr, " to ");
					printaddr(stderr, p);
					fprintf(stderr,
						" failed, trying a non-privileged port.\n");
					fflush(stderr);
				}

			setportnum(p, 0);
			fd=trybind(ci, p, a);
		}

		if (fd >= 0)
		{
			int fd2;

			D(DEBUG_CONNECT)
				{
					fprintf(stderr,
						"DEBUG: Connection attempt from ");
					printaddr(stderr, &ci->clientaddr);
					fprintf(stderr, " listening on ");
					printaddr(stderr, &ci->myaddr);
					fprintf(stderr, "\n");
					fflush(stderr);
				}

			reply(ci, 0x00, &ci->myaddr);

			pfd[0].fd=fd;
			pfd[0].events=POLLIN;

			pfd[1].fd=ci->clientfd;
			pfd[1].events=POLLIN;

			poll(pfd, 2, -1);

			if (!(pfd[0].revents & POLLIN))
			{
				close(fd);
				D(DEBUG_CONNECT)
					{
						fprintf(stderr,
							"DEBUG: Connection attempt from ");
						printaddr(stderr, &ci->clientaddr);
						fprintf(stderr, " cancelled by client.\n");
						fflush(stderr);
					}

				return (-1);
				/* Client socket became readable - client
				** aborted?
				*/
			}

			myaddr_len=sizeof(ci->remoteaddr);
			fd2=accept(fd, (struct sockaddr *)&ci->remoteaddr,
				   &myaddr_len);
			if (fd2 < 0)
				break;
			if (confsocket(fd2, ci) >= 0)
			{
				close(fd);
				fd=fd2;
				break;
			}
			close(fd);
			fd= -1;
		}

		D(DEBUG_CONNECT)
			{
				fprintf(stderr,
					"DEBUG: Bind attempt from ");
				printaddr(stderr, &ci->clientaddr);
				fprintf(stderr, " to ");
				printaddr(stderr, p);
				fprintf(stderr, " failed: %s\n",
					strerror(errno));
				fflush(stderr);
			}
	}

	if (fd < 0)
	{
		mkreplyerrno(ci, p);
		close(fd);
		return (-1);
	}

	reply(ci, 0x00, &ci->remoteaddr);

	D(DEBUG_CONNECTED)
		{
			fprintf(stderr,
				"DEBUG: Proxy connection established: ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, "->");
			printaddr(stderr, &ci->remoteaddr);
			if (ci->authuserid)
				fprintf(stderr,	" (authenticated as %s)",
					ci->authuserid);
			fprintf(stderr, "\n");
			fflush(stderr);
		}

	return fd;
}

#if 0
static void proxyudp(struct conninfo *ci,
		     SOCKADDR_STORAGE *addrs,
		     int rsacnt)
{
	int i;

	for (i=0; i<rsacnt; i++)
	{
		D(DEBUG_CONNECT)
			{
				fprintf(stderr,
					"DEBUG: UDP associate attempt from ");
				printaddr(stderr, &ci->clientaddr);
				fprintf(stderr, " for ");
				printaddr(stderr, addrs+i);
				fprintf(stderr, "...\n");
				fflush(stderr);
			}
	}
}
#endif

static void proxycopy(int fd, struct conninfo *ci)
{
	char rbuf[BUFSIZ], wbuf[BUFSIZ];
	char *rptr=NULL, *wptr=NULL;
	size_t rlen=0, wlen=0;
	struct pollfd pfd[2];
	int rc;
	unsigned long nread=0;
	unsigned long nwritten=0;
	for (;;)
	{
		pfd[0].fd=fd;
		pfd[0].events=0;
		pfd[1].fd=ci->clientfd;
		pfd[1].events=0;

		if (rlen == 0)
			pfd[0].events |= POLLIN;
		else
			pfd[1].events |= POLLOUT;

		if (wlen == 0)
			pfd[1].events |= POLLIN;
		else
			pfd[0].events |= POLLOUT;


		if ((rc=poll(pfd, 2, -1)) < 0)
		{
			fprintf(stderr,
				"DEBUG: poll() failed for ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, "->");
			printaddr(stderr, &ci->remoteaddr);
			fprintf(stderr, ": %s\n", strerror(errno));
			fflush(stderr);
			break;
		}

		if (rlen == 0)
		{
			if (pfd[0].revents & (POLLIN|POLLHUP|POLLERR))
			{
				rptr=rbuf;

				rc=read(fd, rbuf, sizeof(rbuf));

				if (rc == 0)
				{
					D(DEBUG_CONNECT) {
						fprintf(stderr,
							"DEBUG: proxy for ");
						printaddr(stderr, &ci->clientaddr);
						fprintf(stderr, "->");
						printaddr(stderr, &ci->remoteaddr);
						fprintf(stderr,
							" closed by remote host.\n");
						fflush(stderr);
					}
					break;
				}

				if (rc < 0)
				{
					D(DEBUG_CONNECT) {
						fprintf(stderr,
							"DEBUG: proxy for ");
						printaddr(stderr, &ci->clientaddr);
						fprintf(stderr, "->");
						printaddr(stderr, &ci->remoteaddr);
						fprintf(stderr,
							": %s\n", strerror(errno));
						fflush(stderr);
					}
					return;
				}
				rlen=rc;
				nread += rc;
			}
		}
		else
		{
			if (pfd[1].revents & (POLLOUT|POLLHUP|POLLERR))
			{
				rc=write(ci->clientfd, rptr, rlen);

				if (rc <= 0)
				{
					D(DEBUG_CONNECT)
						{
							fprintf(stderr,
								"DEBUG: proxy for ");
							printaddr(stderr, &ci->clientaddr);
							fprintf(stderr, "->");
							printaddr(stderr, &ci->remoteaddr);
							fprintf(stderr,
								": %s\n", strerror(errno));
							fflush(stderr);
						}
					return;
				}

				rptr += rc;
				rlen -= rc;
			}
		}


		if (wlen == 0)
		{
			if (pfd[1].revents & (POLLIN|POLLHUP|POLLERR))
			{
				wptr=wbuf;

				rc=read(ci->clientfd, wbuf, sizeof(wbuf));

				if (rc == 0)
				{
					D(DEBUG_CONNECT) {
						fprintf(stderr,
							"DEBUG: proxy for ");
						printaddr(stderr, &ci->clientaddr);
						fprintf(stderr, "->");
						printaddr(stderr, &ci->remoteaddr);
						fprintf(stderr,
							" closed.\n");
						fflush(stderr);
					}
					break;
				}

				if (rc < 0)
				{
					D(DEBUG_CONNECT) {
						fprintf(stderr,
							"DEBUG: proxy for ");
						printaddr(stderr, &ci->clientaddr);
						fprintf(stderr,
							": %s\n", strerror(errno));
						fflush(stderr);
					}
					return;
				}
				wlen=rc;
			}
		}
		else
		{
			if (pfd[0].revents & (POLLOUT|POLLHUP|POLLERR))
			{
				rc=write(fd, wptr, wlen);

				if (rc <= 0)
				{
					D(DEBUG_CONNECT) {
						fprintf(stderr,
							"DEBUG: proxy for ");
						printaddr(stderr, &ci->clientaddr);
						fprintf(stderr,
							", remote: %s\n",
							strerror(errno));
						fflush(stderr);
					}
					break;
				}

				wptr += rc;
				wlen -= rc;
				nwritten += rc;
			}
		}
	}

	if (rlen)
		writefd(ci, rptr, rlen);

	if (wlen)
	{
		int save_fd=ci->clientfd;

		ci->clientfd=fd;
		writefd(ci, wptr, wlen);
		ci->clientfd=save_fd;
		nwritten += wlen;
	}

	D(DEBUG_CONNECTED)
		{
			fprintf(stderr,
				"DEBUG: Proxy connection closed: ");
			printaddr(stderr, &ci->clientaddr);
			fprintf(stderr, "->");
			printaddr(stderr, &ci->remoteaddr);
			fprintf(stderr, " (%lu/%lu bytes transferred",
				nread, nwritten);
			if (ci->authuserid)
				fprintf(stderr,
					", authenticated as %s)",
					ci->authuserid);
			fprintf(stderr, ")\n");
			fflush(stderr);
		}
}

static void runserver(struct conninfo *ci)
{
	char *userid=authenticate(ci);
	SOCKADDR_STORAGE *rsa;
	int rsacnt;
	int reqtype;
	struct pollfd pfd;
	int fd;

	ci->authuserid=userid;

	reqtype=getrequest(ci, &rsa, &rsacnt);

	switch (reqtype) {
	case 1:
		if ((fd=proxyconnect(ci, rsa, rsacnt)) >= 0)
		{
			proxycopy(fd, ci);
			close(fd);
			shutdown(ci->clientfd, SHUT_WR);
		}
		break;
	case 2:
		if ((fd=proxybind(ci, rsa, rsacnt)) >= 0)
		{
			proxycopy(fd, ci);
			close(fd);
			shutdown(ci->clientfd, SHUT_WR);
		}
		break;
#if 0
	case 3:
		proxyudp(ci, rsa, rsacnt);
		break;
#endif
	default:
		reply(ci, 0x07, NULL);
		fprintf(stderr,
			"DEBUG: Unknown request type %d from ", reqtype);
		printaddr(stderr, &ci->clientaddr);
		fprintf(stderr, "\n");
		fflush(stderr);
		break;
	}
	free(rsa);
	free(userid);

	/* Give the client 10 seconds to close the socket */

	pfd.fd=ci->clientfd;
	pfd.events=POLLIN;
	poll(&pfd, 1, 10 * 1000);
	close(ci->clientfd);
}

static void parsefile(FILE *fp, const char *fn, void *voidarg);

static void (*parsefileptr)(FILE *, const char *, void *);

#define PARSE(fp, fn) (*parsefileptr)(fp, fn, voidarg);
#define sys_fclose fclose
#define DEBUG_PREFIX "DEBUG"

#include	"parseconfigfile.h"
#undef PARSE
#undef sys_fclose
#undef DEBUG_PREFIX

static void freeaccesslist(struct accesslist *a)
{
	if (a->block4filename)
		free(a->block4filename);
}

static int initaccesslist(struct accesslist *a)
{
	char *p;

	a->block4filename=NULL;

	p=strtok(NULL, " \t\r");
	errno=EINVAL;
	if (p == NULL || tocidr(&a->from, p))
		return -1;

	p=strtok(NULL, " \t\r");
	if (p == NULL)
		return 0;

	if (tocidr(&a->to, p))
		return -1;

	while ((p=strtok(NULL, " \t\r")) != NULL)
	{
		if (strncmp(p, "bind=", 5) == 0)
		{
			CIDR c;

			memset(&c, 0, sizeof(c));

			if (tocidr(&c, p+5))
				return -1;

			a->bindAsFlag=1;
#if HAVE_IPV6
			{
				struct sockaddr_in6 sin6;

				memset(&sin6, 0, sizeof(sin6));
				sin6.sin6_family=AF_INET6;
				sin6.sin6_addr=c.addr;
				memcpy(&a->bindAs, &sin6, sizeof(sin6));
			}
#else
			{
				struct sockaddr_in sin;

				sin.sin_family=AF_INET;
				sin.sin_addr=c.addr;
				memcpy(&a->bindAs, &sin, sizeof(sin));
			}
#endif

			continue;
		}
		if (strncmp(p, "block4=", 7) == 0 && a->block4filename == NULL)
		{
			if ((a->block4filename=strdup(p+7)) == NULL)
				return -1;
			continue;
		}

		return -1;
	}
	return 0;
}

static void parsefile(FILE *fp, const char *fn, void *voidarg)
{
	char linebuf[1024];
	int linenum=0;
	struct portinfo *oldportinfo=(struct portinfo *)voidarg;

	while (fgets(linebuf, sizeof(linebuf), fp))
	{
		char *p=strchr(linebuf, '\n');
		char *action;

		++linenum;

		if (p) *p=0;

		p=strchr(linebuf, '#');
		if (p) *p=0;

		action=strtok(linebuf, " \t\r");
		if (!action)
			continue;
		if (!knownconfigword(action))
		{
			fprintf(stderr, "ERR: %s(%d): \"%s\" not recognized\n",
				fn, linenum, action);
			fflush(stderr);
		}

		if (strcmp(action, "port") == 0)
		{
			p=strtok(NULL, " \t\r");
			if (p)
			{
				char *port=strchr(p, ':');
				char *host;

				if (port)
				{
					*port++=0;
					host=p;
				}
				else
				{
					port="socks";
					host=p;
				}

				createsocket(host, port, oldportinfo);
				continue;
			}

			fprintf(stderr, "ERR: %s(%d): Invalid port setting\n",
				fn, linenum);
			fflush(stderr);
			continue;
		}

		if (strcmp(action, "anonproxy") == 0 ||
		    strcmp(action, "authproxy") == 0)
		{
			int authenticated=strcmp(action, "authproxy") == 0;
			struct accesslist *a=malloc(sizeof(struct accesslist));

			if (!a)
			{
				perror("CRIT: malloc");
				exit(1);
			}

			memset(a, 0, sizeof(*a));

			a->authenticated=authenticated;

			if (initaccesslist(a) == 0)
			{
				*accesslist_last=a;
				accesslist_last=&a->next;
				continue;
			}

			free(a);
			fprintf(stderr,
				"ERR: %s(%d): Invalid proxy command: %s\n",
				fn, linenum, strerror(errno));
			fflush(stderr);
			continue;
		}

		if (strcmp(action, "prefork") == 0)
		{
			int n;

			p=strtok(NULL, " \t\r");
			if (p && (n=atoi(p)) > 0)
			{
				nprocs=n;
				continue;
			}
			fprintf(stderr,
				"ERR: %s(%d): Invalid prefork setting.\n",
				fn, linenum);
			fflush(stderr);
			continue;
		}
	}
}

static int getfdcount()
{
	struct portinfo *p;
	int cnt;

	for (p=portlist, cnt=0; p; p=p->next)
	{
		if (p->fd1 >= 0)
			++cnt;
		if (p->fd2 >= 0)
			++cnt;
	}
	return cnt;
}

static int runchild(int childfd)
{
	struct portinfo *p;
	int cnt, i;
	struct pollfd *pi;

	cnt=getfdcount();

	pi=malloc((cnt+1)*sizeof(struct pollfd));

	if (!pi)
	{
		perror("ERR: malloc");
		return (1);
	}

	for (p=portlist, cnt=0; p; p=p->next)
	{
		if (p->fd1 >= 0)
			pi[cnt++].fd=p->fd1;
		if (p->fd2 >= 0)
			pi[cnt++].fd=p->fd2;
	}

	pi[cnt++].fd=preforkmasterpipefd[0];

	for (;;)
	{
		for (i=0; i<cnt; i++)
			pi[i].events=POLLIN;

		if (poll(pi, cnt, -1) < 0)
		{
			if (errno == EINTR)
				continue;

			perror("ERR: poll");
			sleep(30);
			continue;
		}

		for (i=0; i<cnt; i++)
		{
			struct conninfo conninfo;
			socklen_t peeraddr_len;

			memset(&conninfo, 0, sizeof(conninfo));

			if (!(pi[i].revents & (POLLIN|POLLHUP|POLLERR)))
				continue;

			if (pi[i].fd == preforkmasterpipefd[0])
			{
				/* Signal for all children to terminate */

				for (i=0; i<cnt; i++)
					close(pi[i].fd);
				exit(0);
			}

			if ((conninfo.clientfd
			     =accept(pi[i].fd,
				     (struct sockaddr *)&conninfo.clientaddr,
				     ((peeraddr_len=
				       sizeof(conninfo.clientaddr)),
				      &peeraddr_len))) >= 0)
			{
				int dummy=1;

				close(childfd);

				for (i=0; i<cnt; i++)
					close(pi[i].fd);

				setsockopt(conninfo.clientfd,
					   SOL_SOCKET,
					   SO_KEEPALIVE,
					   (const char *)&dummy,
					   sizeof(dummy));

				runserver(&conninfo);
				exit(0);
			}
		}
	}

}

static void process_new_configuration()
{
	struct accesslist *a;
	struct portinfo *p, *q;
	int newcontrolpipefd[2];
	int *newchildpipes;
	struct pollfd *newpollfd;
	int n;

	sighup_received=0;

	/* Remove old access list */

	while ((a=accesslist_first) != NULL)
	{
		accesslist_first=a->next;
		freeaccesslist(a);
		free(a);
	}
	accesslist_last= &accesslist_first;

	/*
	** Save current port list, close old sockets after new
	** sockets are created.
	*/

	p=portlist;
	portlist=NULL;

	parsefileptr=parsefile;
	parseconfigfile(p);

	for (q=portlist; q; q=q->next)
	{
		if (q->fd1 >= 0 ||
		    q->fd2 >= 0)
			break;
	}

	if (!q && p) /* No sockets created */
	{
		portlist=p;
		p=NULL;

		fprintf(stderr, "ERR: No sockets created for new configuration, keeping previous sockets.\n");
		fflush(stderr);
	}

	while ((q=p) != NULL)
	{
		p=q->next;
		if (q->fd1 >= 0)
			close(q->fd1);
		if (q->fd2 >= 0)
			close(q->fd2);
	}

	if (pipe(newcontrolpipefd) < 0)
	{
		fprintf(stderr, "CRIT: pipe: %s\n", strerror(errno));
		fflush(stderr);
		return;
	}

	if ((newchildpipes=malloc(sizeof(int)*nprocs)) == NULL)
	{
		close(newcontrolpipefd[0]);
		close(newcontrolpipefd[1]);
		fprintf(stderr, "CRIT: malloc: %s\n", strerror(errno));
		fflush(stderr);
		return;
	}

	if ((newpollfd=malloc(sizeof(struct pollfd)*nprocs)) == NULL)
	{
		free(newchildpipes);
		close(newcontrolpipefd[0]);
		close(newcontrolpipefd[1]);
		fprintf(stderr, "CRIT: malloc: %s\n", strerror(errno));
		fflush(stderr);
		return;
	}

	if (preforkmasterpipefd[0] >= 0)
		close(preforkmasterpipefd[0]);
	if (preforkmasterpipefd[1] >= 0)
		close(preforkmasterpipefd[1]);
	for (n=0; n<npreforkedchildpipes; n++)
		close(preforkedchildpipes[n]);

	if (preforkedchildpipes)
		free(preforkedchildpipes);
	if (preforkpollfd)
		free(preforkpollfd);

	preforkmasterpipefd[0]=newcontrolpipefd[0];
	preforkmasterpipefd[1]=newcontrolpipefd[1];
	npreforkedchildpipes=nprocs;
	preforkpollfd=newpollfd;
	preforkedchildpipes=newchildpipes;
	for (n=0; n<npreforkedchildpipes; n++)
		preforkedchildpipes[n]= -1;

	if (getfdcount() == 0)
		fprintf(stderr, "CRIT: %s does not define any listening sockets, or all sockets are unavailable.\n", CONFIGFILE);

	fprintf(stderr, "DEBUG: Creating %d child processes...\n",
		npreforkedchildpipes);
	fflush(stderr);

	for (n=0; n<npreforkedchildpipes; n++)
	{
		int fd;

		if (startserver_orbust(&fd) == 0)
			runchild(fd);
		preforkedchildpipes[n]=fd;
	}
	fprintf(stderr, "DEBUG: Created %d child processes...\n",
		npreforkedchildpipes);
	fflush(stderr);
}

static void respawn_children()
{
	int n;

	while (!sighup_received)
	{
		for (n=0; n<npreforkedchildpipes; n++)
		{
			preforkpollfd[n].fd=preforkedchildpipes[n];
			preforkpollfd[n].events=POLLIN;
		}
		if (poll(preforkpollfd, n, -1) < 0)
		{
			if (errno == EINTR)
				continue;

			perror("ERR: poll");
			sleep(30);
			continue;
		}
		for (n=0; n<npreforkedchildpipes; n++)
		{
			if (preforkpollfd[n].revents & (POLLIN|POLLHUP|POLLERR))
			{
				int fd;

				close(preforkedchildpipes[n]);
				preforkedchildpipes[n]= -1;

				if (startserver_orbust(&fd) == 0)
					runchild(fd);
				preforkedchildpipes[n]=fd;
			}
		}
	}
}

static int start()
{
	signal(SIGINT, sigexit);
	signal(SIGHUP, sighup);
	signal(SIGTERM, sigexit);
	signal(SIGCHLD, SIG_DFL);

	preforkmasterpipefd[0]= -1;
	preforkmasterpipefd[1]= -1;
	npreforkedchildpipes=0;

	do
	{
		process_new_configuration();
		if (npreforkedchildpipes <= 0)
		{
			fprintf(stderr,
				"CRIT: Cannot start child processes.\n");
			fflush(stderr);
			break;
		}
		respawn_children();
	} while (sighup_received);
	return (0);
}

static uid_t config_uid=0;
static gid_t config_gid=0;
static int config_debuglevel=0;

static void parseuidgid(FILE *fp, const char *fn, void *voidarg)
{
	char linebuf[1024];
	int linenum=0;

	while (fgets(linebuf, sizeof(linebuf), fp))
	{
		char *p=strchr(linebuf, '\n');

		++linenum;

		if (p) *p=0;

		p=strchr(linebuf, '#');
		if (p) *p=0;

		p=strtok(linebuf, " \t\r");
		if (!p)
			continue;

		if (strcmp(p, "debug") == 0)
		{
			p=strtok(NULL, " \t\r");

			if (p == NULL)
			{
				fprintf(stderr,
					"%s(%d): Invalid debug level\n",
					fn, linenum);
				fflush(stderr);
				continue;
			}
			config_debuglevel=atoi(p);
			continue;
		}

		if (strcmp(p, "user") == 0)
		{
			struct passwd *pw;

			p=strtok(NULL, " \t\r");

			if (p == NULL)
			{
				fprintf(stderr,
					"%s(%d): Invalid user command\n",
					fn, linenum);
				fflush(stderr);
				continue;
			}

			if ((pw=getpwnam(p)) == NULL)
			{
				fprintf(stderr,
					"%s(%d): Invalid user: %s\n",
					fn, linenum, p);
				fflush(stderr);
				exit(1);
			}

			config_uid=pw->pw_uid;
			config_gid=pw->pw_gid;
		}
	}
}

int main(int argc, char **argv)
{
	int argn;

	for (argn=1; argn<argc; argn++)
	{
		if (strncmp(argv[argn], "-debug=", 7) == 0)
		{
			debug_level=atoi(argv[argn]+7);
			continue;
		}

		if (strcmp(argv[argn], "-uidgid") == 0 ||
		    strcmp(argv[argn], "-opts") == 0)
		{
			parsefileptr=parseuidgid;
			parseconfigfile(NULL);

			if (config_uid == 0 && config_gid == 0)
			{
				struct passwd *pwd=getpwnam("nobody");

				if (pwd == NULL)
				{
					fprintf(stderr,
						"ERROR: Invalid user: nobody\n");
					fflush(stderr);
					exit(1);
				}

				config_uid=pwd->pw_uid;
				config_gid=pwd->pw_gid;
			}

			if (argv[argn][1] == 'o')
			{
				printf("-debug=%d\n", config_debuglevel);

			}
			else
			{
				printf("-user=%lu -group=%lu\n",
				       (unsigned long)config_uid,
				       (unsigned long)config_gid);
			}
			exit(0);
		}
	}
		
	exit(start());
	return (0);
}
