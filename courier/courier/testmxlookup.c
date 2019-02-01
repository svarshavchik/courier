/*
** Copyright 1998 - 2019 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"courier.h"
#include	"rfc1035/config.h"
#include	"rfc1035/rfc1035.h"
#include	"rfc1035/rfc1035mxlist.h"
#include	"comsts.h"
#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>
#include	<unistd.h>
#include	"numlib/numlib.h"
#include	<arpa/inet.h>
#include	<sys/types.h>
#include	<sys/stat.h>

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
	int sts=0, sts_expire_opt=0;
	struct sts_id domain_sts_id;
	enum sts_mode domain_sts_mode;

	argn=1;
	srand(time(NULL));

	if (geteuid() == 0)
		libmail_changeuidgid(MAILUID, MAILGID);

	umask(022);

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

		if (strcmp(argv[argn], "--sts") == 0)
		{
			sts=1;
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "--sts-expire") == 0)
		{
			sts_expire_opt=1;
			++argn;
			continue;
		}
		break;
	}

	if (sts_expire_opt)
	{
		sts_expire();
		exit(0);
	}
	if (argn >= argc)	exit(0);

	q_name=argv[argn++];

	sts_init(&domain_sts_id, q_name);

	if (sts)
	{
		if (!domain_sts_id.id)
		{
			fprintf(stderr, "%s: STS policy not found.\n",
				q_name);
			exit(1);
		}

		printf("STS Policy ID: %s%s\n", domain_sts_id.id,
		       (domain_sts_id.cached ? " (cached)":""));
		printf("Timestamp    : %s", ctime(&domain_sts_id.timestamp));
		printf("Expiration   : %s\n", ctime(&domain_sts_id.expiration));
		printf("%s", domain_sts_id.policy);
		sts_deinit(&domain_sts_id);
		exit(0);
	}

	rc=rfc1035_mxlist_create_x(&res, q_name,
				   RFC1035_MX_AFALLBACK |
				   RFC1035_MX_IGNORESOFTERR,
				   &mxlist);
	rfc1035_destroy_resolv(&res);

	if (rc != RFC1035_MX_OK)
		sts_deinit(&domain_sts_id);

	switch (rc)	{
	case	RFC1035_MX_OK:
		break;
	case	RFC1035_MX_SOFTERR:
		printf("%s: soft error.\n", q_name);
		exit(1);
	case	RFC1035_MX_HARDERR:
		printf("%s: hard error.\n", q_name);
		exit(1);
	case	RFC1035_MX_INTERNAL:
		printf("%s: internal error.\n", q_name);
		exit(1);
	case	RFC1035_MX_BADDNS:
		printf("%s: bad DNS records (recursive CNAME).\n", q_name);
		exit(1);
	}

	printf("Domain %s:\n", q_name);
	domain_sts_mode=get_sts_mode(&domain_sts_id);

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

		if (domain_sts_mode != sts_mode_none &&
		    sts_mx_validate(&domain_sts_id, p->hostname))
			printf("ERROR: STS Policy verification failed: %s\n",
			       p->hostname);
	}

	rfc1035_mxlist_free(mxlist);

	switch (domain_sts_mode) {
	case sts_mode_none:
		break;
	case sts_mode_testing:
		printf("STS: testing\n");
		break;
	case sts_mode_enforce:
		printf("STS: enforcing\n");
		break;
	}
	sts_deinit(&domain_sts_id);
	return (0);
}
