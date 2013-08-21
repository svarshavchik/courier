/*
**
** Copyright 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

/*
** Convenience function to convert address to IPV4.
*/
#include "courier_socks_config.h"

	/* Convert IPv4-mapped IPv6 addresses to IPv4 */

static int toipv4(struct sockaddr_in *sin,
		  const SOCKADDR_STORAGE *addrptr)
{
	if (((const struct sockaddr_in *)addrptr)->sin_family == AF_INET)
	{
		*sin=*(const struct sockaddr_in *)addrptr;
		return 0;
	}

#if HAVE_IPV6

	if (((const struct sockaddr_in6 *)addrptr)->sin6_family == AF_INET6 &&
	    (IN6_IS_ADDR_V4MAPPED( &((const struct sockaddr_in6 *)addrptr)
				  ->sin6_addr) ||
	     IN6_IS_ADDR_V4COMPAT( &((const struct sockaddr_in6 *)addrptr)
				   ->sin6_addr)))
	{
		memset(sin, 0, sizeof(*sin));
		sin->sin_family=AF_INET;

		memcpy(&sin->sin_addr,
		       (char *)&((const struct sockaddr_in6 *)addrptr)
		       ->sin6_addr + 12, 4);
		sin->sin_port=((const struct sockaddr_in6 *)addrptr)
			->sin6_port;
		return 0;
	}
#endif
	errno=EINVAL;
	return -1;
}
