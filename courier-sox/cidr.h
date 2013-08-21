/*
**
** Copyright 2004 Double Precision, Inc.
** See COPYING for distribution information.
*/

/*
** Convenience functions to process CIDRs
*/
#include "courier_socks_config.h"

typedef struct {

#if HAVE_IPV6
	struct in6_addr addr;
#else
	struct in_addr addr;
#endif
	int pfix;
} CIDR;

/*
** Parse addr="ip/pfix" into c.
*/

static int tocidr(CIDR *c, char *addr)
{
	struct in_addr i;

	char *pfix=strchr(addr, '/');

	if (pfix) *pfix++=0;

#if HAVE_IPV6
	if (strchr(addr, ':')) /* IPv6 address */
	{
		if (inet_pton(AF_INET6, addr, &c->addr) <= 0)
		{
			errno=EINVAL;
			return -1;
		}

		if (pfix)
		{
			c->pfix=atoi(pfix);
		}
		else
		{
			/* Count words to figure out the right prefix */

			c->pfix=16;
			while (*addr)
			{
				if (*addr++ == ':')
				{
					/*
					** ip:: ends the prefix right here,
					** but ip::ip is a full IPv6 address.
					*/

					if (*addr == ':')
					{
						if (addr[1])
							c->pfix=sizeof(struct in6_addr)
								*8;
						break;
					}
					if (*addr == 0)
						break;
					c->pfix += 16;
				}
			}
		}

		if (c->pfix < 0 || c->pfix > sizeof(struct in6_addr)*8)
		{
			errno=EINVAL;
			return -1;
		}
		return 0;
	}
#endif

	{
		char b[32];
		char *c;

		/*
		** If a.b.c is given, make it a.b.c.0 so that inet_addr does
		** the right thing (ditto for a.b and a).
		*/

		if (strlen(addr) >  sizeof(b)-sizeof(".0.0.0"))
		{
			errno=EINVAL;
			return -1;
		}

		strcpy(b, addr);

		if ((c=strchr(b, '.')) == NULL)
			strcat(b, ".0.0.0");
		else if ((c=strchr(c+1, '.')) == NULL)
			strcat(b, ".0.0");
		else if ((c=strchr(c+1, '.')) == NULL)
			strcat(b, ".0");

#if HAVE_INET_PTON
		if (inet_pton(AF_INET, b, &i) <= 0)
		{
			errno=EINVAL;
			return -1;
		}
#else

		if ((i.s_addr=inet_addr(b)) == INADDR_NONE)
		{
			errno=EINVAL;
			return -1;
		}
#endif
	}

	if (pfix)
	{
		c->pfix=atoi(pfix);
	}
	else
	{
		/* Similarly figure out the prefix if not specified */

		c->pfix=8;
		while (*addr)
		{
			if (*addr++ == '.')
			{
				if (*addr == 0)
					break;
				c->pfix += 8;
			}
		}
	}

	if (c->pfix < 0 || c->pfix > sizeof(i)*8)
	{
		errno=EINVAL;
		return -1;
	}

#if HAVE_IPV6

	/*
	** If we have IPv6 the IPv4 address must be up-converted to an IPv6
	** address, and the prefix adjusted accordingly.
	*/

	{
		char b[INET6_ADDRSTRLEN];

		strcat(strcpy(b, "::ffff:"), inet_ntoa(i));

		if (inet_pton(AF_INET6, b, &c->addr) < 0)
			return -1;

		if (i.s_addr == INADDR_ANY)
		{
			c->addr=in6addr_any;

			if (c->pfix == 0)
				return 0;
		}

		c->pfix += (sizeof(struct in6_addr)-sizeof(struct in_addr))
			*8;
	}
#else
	c->addr=i;
#endif
	return 0;
}

/*
** Return 0 if socket address in 'ss' is in CIDR 'c'.
**
** Return non-zero if not.

*/

static int incidr(const CIDR *c, const SOCKADDR_STORAGE *ss)
{
#if HAVE_IPV6
	struct in6_addr in6;
#endif
	struct in_addr i;
	unsigned char *ptr;
	unsigned char *b;
	int n;
	int mask;

	switch (((const struct sockaddr_in *)ss)->sin_family) {
#if HAVE_IPV6
	case AF_INET6:

		in6=((const struct sockaddr_in6 *)ss)->sin6_addr;
		ptr=(unsigned char *)&in6;
		break;
#endif
	case AF_INET:
		i=((const struct sockaddr_in *)ss)->sin_addr;

#if HAVE_IPV6
		/* Must up-convert to IPv6 */

		{
			char b[INET6_ADDRSTRLEN];

			strcat(strcpy(b, "::ffff:"), inet_ntoa(i));

			if (inet_pton(AF_INET6, b, &in6) < 0)
				return -1;
			ptr=(unsigned char *)&in6;

			if (i.s_addr == INADDR_ANY)
				in6=in6addr_any;
		}
#else
		ptr=(unsigned char *)&i;
#endif
		break;
	default:
		return -1;
	}

	n=c->pfix;
	b=(unsigned char *)&c->addr;

	while (n >= 8)
	{
		if (*ptr != *b)
			return -1;
		++ptr;
		++b;
		n -= 8;
	}

	if (n)
	{
		mask= ((~0) << (8-n)) & 255;

		if ( (*ptr ^ *b) & mask )
			return -1;
	}

	return 0;
}

