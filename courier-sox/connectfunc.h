/*
**
** Copyright 2004 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef sox_connect_h
#define sox_connect_h

#include "courier_socks_config.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

/*
** Define a recycled function that resolves a hostname to IP addresses.
** Depending on the presence/absence of getaddrinfo &/| IPv6 it can get messy.
**
** Confine the mess to this header file.  Define two functions.
**
** 1.  "Initialize"
** 2.  "Return next address", piecemeal.
*/

/*
** This structure is filled in by INITFUNC, and used by NEXTFUNC.
*/

struct connect_info {

	int sin_family;

#if HAVE_GETADDRINFO
	struct addrinfo *res, *next;
#else
	int h_addrtype;
	int h_length;
	char **h_addr_list;
	int sin_port;
	int next;
	struct sockaddr_in sin;
#if HAVE_IPV6
	struct sockaddr_in6 sin6;
#endif

#endif
};

/*
** INITFUNC initializes a connect_info structure.
**
** Returns 0 if the hostname can be resolved, <0 on an error.
**
** If it returns an error, the connect_info can be discarded.
** Otherwise, NEXTFUNC _must_ be repeatedly called, to fetch all addresses,
** else memory will leak.
*/

static int INITFUNC(struct connect_info *ci,
		    int sin_family,
		    int sin_proto,
		    const char *service, /* The hostname */
		    const char *port)
{
#if HAVE_GETADDRINFO
	struct addrinfo hints;
	int rc;

	/*
	** If getaddrinfo is available, life is easy.
	*/

	memset(&hints, 0, sizeof(hints));
	hints.ai_family=PF_UNSPEC;
	hints.ai_socktype=sin_proto;
	hints.ai_flags=0;

	rc=getaddrinfo(service, port, &hints, &ci->res);

	if (rc < 0)
	{
		if ( 0
#ifdef EAI_NONAME
		     || rc == EAI_NONAME
#endif
#ifdef EAI_SERVICE
		     || rc == EAI_SERVICE
#endif
#ifdef EAI_ADDRFAMILY
		     || rc == EAI_ADDRFAMILY
#endif
#ifdef EAI_NODATA
		     || rc == EAI_NODATA
#endif
		     )
		{
#ifdef EADDRNOTAVAIL
			errno=EADDRNOTAVAIL;
#else
			errno=ENOENT;
#endif
		}

		else if (0
#ifdef EAI_FAMILY
			 || rc == EAI_FAMILY
#endif
#ifdef EAI_SOCKTYPE
			 || rc == EAI_SOCKTYPE
#endif
			 )
		{
#ifdef EADDRNOTAVAIL
			errno=EAFNOSUPPORT;
#else
			errno=ENOENT;
#endif
		}
		else if (0
#ifdef EAI_MEMORY
			 || rc == EAI_MEMORY
#endif
			 )
		{
			errno=ENOMEM;
		}
		else if (0
#ifdef EAI_AGAIN
			 || rc == EAI_AGAIN
#endif
			 )
		{
			errno=EAGAIN;
		}
		else
		{
#ifdef EFAULT
			errno=EFAULT;
#else
			errno=ENOMEM;
#endif
		}

		ci->next=NULL;
		ci->res=NULL;
		return -1;
	}

	ci->next=ci->res;
#else
	struct servent *se;

	ci->next=0;

	/*
	** Check if port is numeric.
	*/

	if ((ci->sin_port=atoi(port)) > 0)
	{
		ci->sin_port=htons(ci->sin_port);
	}
	else
	{
		/*
		** Look it up, otherwise.
		*/

		se=getservbyname(port, sin_proto == SOCK_STREAM ? "tcp":
				 "udp");
		if (!se)
		{
			errno=ENOENT;
			return -1;
		}
		ci->sin_port=se->s_port;
	}

	/*
	** If the hostname is a raw IP address fake-out the results of
	** gethostbyname()
	*/

	memset(&ci->sin, 0, sizeof(ci->sin));

	if (sin_family == AF_INET &&
	    inet_aton(service, &ci->sin.sin_addr))
	{
		if ((ci->h_addr_list=malloc(sizeof(char *)*2)) == NULL ||
		    (ci->h_addr_list[0]=malloc(sizeof(&ci->sin))) == NULL)
		{
			if (ci->h_addr_list)
				free(ci->h_addr_list);
			return -1;
		}
		ci->h_addrtype=AF_INET;
		ci->h_addr_list[1]=NULL;
		ci->h_length=sizeof(ci->sin);
	}
	else
	{
		int n;

		struct hostent *he=gethostbyname(service);

		/* Sanity checks follow */

		if (!he || he->h_addrtype != sin_family)
		{
			errno=ENOENT;
			return -1;
		}

		if (he->h_addrtype == AF_INET
		    && he->h_length <= sizeof(ci->sin))
			; /* We can dig that */
#if HAVE_IPV6
		else if (he->h_addrtype == AF_INET6
			 && he->h_length <= sizeof(ci->sin6))
			; /* We can dig this too */
#endif
		else
		{
			errno=EPROTO;
			return -1; /* Can't dig that */
		}

		/*
		** Allocate a buffer and copy over the addresses.  We should
		** do that since each invocation of gethostbyname() uses the
		** same buffer.
		*/

		for (n=0; he->h_addr_list[n]; ++n)
			;

		if ((ci->h_addr_list=malloc(sizeof(char *)*(n+1))) == NULL ||
		    (n>0 && (ci->h_addr_list[0]=
			     malloc(he->h_length*n*sizeof(char))) == NULL))
		{
			if (ci->h_addr_list)
				free(ci->h_addr_list);
			return -1;
		}

		for (n=0; he->h_addr_list[n]; n++)
		{
			ci->h_addr_list[n+1]=ci->h_addr_list[n]+he->h_length;
			memcpy(ci->h_addr_list[n],
			       he->h_addr_list[n], he->h_length);
		}
		ci->h_addrtype=he->h_addrtype;
		ci->h_addr_list[n]=NULL;
		ci->h_length=he->h_length;

	}
#endif
	return 0;
}

/*
** NEXTFUNC returns 0 if it initialized addr & addrlen to the next resolved
** address, and non-zero after all addresses are already returned.
**
** In all cases NEXTFUNC must be repeatedly called until a non-zero return,
** in order to release all allocated memory.
*/

static int NEXTFUNC(struct connect_info *ci,
		    const struct sockaddr **addr,
		    SOCKLEN_T *addrlen)
{
#if HAVE_GETADDRINFO
	struct addrinfo *addrinfo=ci->next;

	if (addrinfo == NULL)
	{
		if ( ci->res )
		{
			freeaddrinfo(ci->res);
			ci->res=NULL;
		}
		return -1;
	}

	ci->next=addrinfo->ai_next;

	*addr=(const struct sockaddr *)addrinfo->ai_addr;
	*addrlen=addrinfo->ai_addrlen;
	return 0;

#else

	if (ci->h_addr_list == NULL ||
	    ci->h_addr_list[ci->next] == NULL)
	{
		if (ci->h_addr_list && ci->h_addr_list[0])
			free(ci->h_addr_list[0]);
		if (ci->h_addr_list)
			free(ci->h_addr_list);
		ci->h_addr_list=NULL;
		return -1;
	}

	if (ci->h_addrtype == AF_INET)
	{
		memset(&ci->sin, 0, sizeof(ci->sin));
		ci->sin.sin_family=AF_INET;
		ci->sin.sin_port=ci->sin_port;
		memcpy(&ci->sin.sin_addr,
		       ci->h_addr_list[ci->next],
		       ci->h_length);
		++ci->next;
		*addr=(const struct sockaddr *)&ci->sin;
		*addrlen=sizeof(ci->sin);
		return 0;
	}
#if HAVE_IPV6
	else
	{
		memset(&ci->sin6, 0, sizeof(ci->sin6));
		ci->sin6.sin6_family=AF_INET6;
		ci->sin6.sin6_port=ci->sin_port;
		memcpy(&ci->sin6.sin6_addr, ci->h_addr_list[ci->next],
		       ci->h_length);
		++ci->next;
		*addr=(const struct sockaddr *)&ci->sin6;
		*addrlen=sizeof(ci->sin6);
		return(0);
	}
#endif
#endif
}

#endif
