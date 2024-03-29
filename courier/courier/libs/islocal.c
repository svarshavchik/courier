/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"dbobj.h"
#include	<string.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<idn2.h>

#ifndef ISLOCAL_MAX_DOT_COUNT
#define ISLOCAL_MAX_DOT_COUNT 5
#endif


/*
	local.islocal: If the given address is a local one, strip off the '@'
	part, otherwise remove the entire address.
*/

static const char *control_locals=0;

/* Return config locals file */

static const char *get_control_locals()
{
	if (!control_locals)
	{
	char *filename=config_localfilename("locals");
	char *buf=readfile(filename, 0);

		free(filename);
		if (!buf)
			control_locals=config_me();
		else
		{
			removecomments(buf);
			control_locals=buf;
		}
	}
	return (control_locals);
}

static int isinit=0;
static struct dbobj db;

int config_islocal(const char *address, char **domainp)
{
	char	*k;
	char	*v;
	size_t	vl;
	size_t	kl;

	const char *p;
	size_t	pl;
	char	*lcaddress;
	int	dotcount;
	char *address_utf8;
	char *hlocal;

	if (domainp)	*domainp=0;

	if (config_is_indomain(address, get_control_locals()))
		return (1);

	if (!isinit)
	{
	char    *filename;

		dbobj_init(&db);
		filename=config_localfilename("hosteddomains.dat");
		isinit=1;
		dbobj_open(&db, filename, "R");
		free(filename);
        }

	if (!dbobj_isopen(&db))
		return (0);

	if (idna_to_unicode_8z8z(address, &address_utf8, 0) != IDNA_SUCCESS)
		address_utf8=courier_strdup(address);

	k=ualllower(address_utf8);
	free(address_utf8);

	kl=strlen(k);

	lcaddress = k;
	dotcount = 0;

	hlocal=config_is_gethostname(k);

	if (hlocal &&
	    (v=dbobj_fetch(&db, hlocal, strlen(hlocal), &vl, "")) != NULL)
		;
	else
	{
		while ( (v=dbobj_fetch(&db, k, kl, &vl, "")) == NULL &&
			(k = strchr(k+1,'.')) != NULL &&
			dotcount++ < ISLOCAL_MAX_DOT_COUNT )
			kl = strlen(k);
	}

	if (hlocal)
		free(hlocal);
	if (k) address += k-lcaddress;
	free(lcaddress);
	if (!v)	return (0);

	/* Default entry is "1", so punt it to k*/

	p=v;
	pl=vl;

	if (vl <= 1)
	{
		p=address;
		pl=kl;
	}

	if (domainp)
	{
		*domainp=courier_malloc(pl+1);
		memcpy( *domainp, p, pl);
		(*domainp)[pl]=0;
	}
	free(v);
	return (1);
}

static int config_is_indomainutf8(const char *address, const char *localp)
{
	while (*localp)
	{
	unsigned i;

		for (i=0; localp[i] && localp[i] != '\n' && localp[i] != '\r';
			++i)
			;

		if ( i > 0 && localp[0] == '!' && *address != '!' &&
		     config_domaincmp(address, localp+1, i-1) == 0)
			return (0);

		if ( config_domaincmp(address, localp, i) == 0)
			return (1);
		localp += i;
		if (*localp)	++localp;
	}
	return (0);
}

int config_is_indomain(const char *address, const char *localp)
{
	char *address_utf8;
	char *l;
	char *hlocal;
	int rc;

	if (idna_to_unicode_8z8z(address, &address_utf8, 0) != IDNA_SUCCESS)
		address_utf8=courier_strdup(address);

	l=ualllower(address_utf8);
	free(address_utf8);

	hlocal=config_is_gethostname(l);
	if (hlocal &&
	    config_is_indomainutf8(hlocal, localp))
		rc=1;
	else
		rc=config_is_indomainutf8(l, localp);
	if (hlocal)
		free(hlocal);
	free(l);
	return rc;
}
