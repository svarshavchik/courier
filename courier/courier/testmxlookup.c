/*
** Copyright 1998 - 2011 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"courier.h"
#include	"rfc1035/config.h"
#include	"rfc1035/rfc1035.h"
#include	"rfc1035/rfc1035mxlist.h"
#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>
#include	<arpa/inet.h>


static void setns(const char *p, struct rfc1035_res *res)
{
RFC1035_ADDR ia[4];
int	i=0;
char	*q=malloc(strlen(p)+1), *r;

	strcpy(q, p);
	for (r=q; (r=strtok(r, ", ")) != 0; r=0)
		if (i < 4)
		{
			if (rfc1035_aton(r, &ia[i]) == 0)
			{
				++i;
			}
			else
			{
				fprintf(stderr, "%s: invalid IP address\n",
					r);
			}
		}
	rfc1035_init_ns(res, ia, i);
}

int main(int argc, char **argv)
{
	int	argn;
	const char *q_name;
	struct rfc1035_mxlist *mxlist, *p;
	struct rfc1035_res res;
	int rc;

	argn=1;
	srand(time(NULL));

	rfc1035_init_resolv(&res);
	while (argn < argc)
	{
		if (argv[argn][0] == '@')
		{
			setns(argv[argn]+1, &res);
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "--dnssec") == 0)
		{
			rfc1035_init_dnssec_enable(&res, 1);
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "--udpsize") == 0)
		{
			++argn;

			if (argn < argc)
			{
				rfc1035_init_edns_payload(&res,
							  atoi(argv[argn]));
				++argn;
			}
			continue;
		}

		break;
	}

	if (argn >= argc)	exit(0);

	q_name=argv[argn++];

	rc=rfc1035_mxlist_create_x(&res, q_name,
				   RFC1035_MX_AFALLBACK |
				   RFC1035_MX_IGNORESOFTERR,
				   &mxlist);
	rfc1035_destroy_resolv(&res);

	switch (rc)	{
	case	RFC1035_MX_OK:
		break;
	case	RFC1035_MX_SOFTERR:
		printf("%s: soft error.\n", q_name);
		exit(0);
	case	RFC1035_MX_HARDERR:
		printf("%s: hard error.\n", q_name);
		exit(0);
	case	RFC1035_MX_INTERNAL:
		printf("%s: internal error.\n", q_name);
		exit(0);
	case	RFC1035_MX_BADDNS:
		printf("%s: bad DNS records (recursive CNAME).\n", q_name);
		exit(0);
	}

	printf("Domain %s:\n", q_name);
	for (p=mxlist; p; p=p->next)
	{
	RFC1035_ADDR	addr;
	char	buf[RFC1035_NTOABUFSIZE];

		if (rfc1035_sockaddrip(&p->address, sizeof(p->address),
			&addr)<0)
			continue;
		rfc1035_ntoa(&addr, buf);

		printf("Relay: %s, Priority: %d, Address: %s%s%s%s\n",
		       p->hostname,
		       p->priority, buf,
		       config_islocal(p->hostname, NULL) ? " [ LOCAL ]":"",
		       strcmp(p->hostname, buf) ? "":" [ ERROR ]",
		       p->ad ? " (DNSSEC)":"");
	}

	rfc1035_mxlist_free(mxlist);
	return (0);
}
