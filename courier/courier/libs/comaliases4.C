/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"aliases.h"
#include	"courier.h"
#include	"libexecdir.h"
#include	"sysconfdir.h"
#include	"rw.h"
#include	<stdio.h>
#include	<errno.h>
#include	<string.h>

#if	HAVE_LDAP
#include	"ldapaliasdrc.h"
#endif


AliasSearch::AliasSearch()
{
	module_record=&module_alias;
	local_record=&local_alias;
}

void AliasSearch::Open( struct rw_transport *module )
{
	Open( (const char *)(module ? module->name:0));
}

void AliasSearch::Open( const char *module )
{
char	*p;

	modulename="local"; 

	if (module)
	{
		modulename=SYSCONFDIR "/aliases-";

		modulename += module;
		modulename += ".dat";
		if (module_alias.Open(modulename, "R") && errno != ENOENT)
			clog_msg_errno();
		modulename=module;
	}

	p=config_localfilename("aliases.dat");

	if (local_alias.Open(p, "R") && errno != ENOENT)
		clog_msg_errno();
	free(p);
}

int	AliasSearch::Search(const char *address, AliasHandler &h)
{
char	*p=0;
const char *q;
char	*hostdomain=0;

	q=strrchr(address, '@');

	/* islocal checks both local domains, and hosted domains,
	** we want to replace domain with 'me' only for local domains.
	*/
	if (!q || (config_islocal(q+1, &hostdomain) && hostdomain == 0))
					/*
					** Make sure we look for aliases for
					** "me"
					*/
	{
	const char *me=config_defaultdomain();

		if (!q)
			q=address+strlen(address);

		p=(char *)courier_malloc(q-address+2+strlen(me));
		if (!p)	clog_msg_errno();
		memcpy(p, address, q-address);
		strcat(strcpy(&p[q-address], "@"), me);
		address=p;
		domainlower(p);	/* For stupid people */
	}
	if (hostdomain)	free(hostdomain);

	if (Try(module_record, address, h) == 0 &&
		TryVirtual(module_record, address, &h) == 0 &&
		Try(local_record, address, h) == 0 &&
		TryVirtual(local_record, address, &h) == 0)
	{
		int rc;

#if HAVE_LDAP
		if (p)
			*strrchr(p, '@')=0;
		// Strip local domain from addresses passed to aliasd

		rc=TryAliasD(address, &h);
#else
		rc=1;
#endif
		if (p)	free(p);
		return (rc);
	}
	if (p)	free(p);
	return (0);
}

struct ldapaliaslist {
	struct ldapaliaslist *next;
	std::string alias;
} ;

#if HAVE_LDAP

// Send the address to courierldapaliasd, for processing.

int	AliasSearch::TryAliasD(const char *address, AliasHandler *h)
{
	static int aliasd_enabled=0;
	FILE *fp;
	char buf[BUFSIZ];

	struct ldapaliaslist *list;

	// First time in check if ldap aliasing is enabled

	if (!aliasd_enabled)
	{
		const char *p=ldapaliasd_config("LDAP_ALIAS");

		aliasd_enabled= !p || atoi(p) == 0 ? -1:1;
	}

	if (aliasd_enabled < 0)
		return (1);

	fp=ldapaliasd_connect();

	if (!fp)
		return (-1);

	list=0;
	try
	{
		fprintf(fp, "%s %s\n", modulename.c_str(), address);
		fflush(fp);

		struct ldapaliaslist *a;

		const char *me=config_defaultdomain();

		// Read the response, which must be dot-terminated
		// If no dot-termination, this is a soft failure.

		for (;;)
		{
			char *p;

			if (fgets(buf, sizeof(buf), fp) == NULL)
			{
				fclose(fp);
				while ((a=list) != 0)
				{
					list=a->next;
					delete a;
				}
				return (-1);
			}

			if ((p=strchr(buf, '\n')) != 0) *p=0;

			if (strcmp(buf, ".") == 0)
				break;

			if ((p=strchr(buf, ':')) == 0) continue;

			*p++=0;
			while (*p == ' ')
				++p;
			if (strcmp(buf, "maildrop"))
				continue;

			a=new struct ldapaliaslist;

			if (!a)
			{
				perror("malloc");
				throw;
			}
			a->next=list;
			list=a;
			a->alias=p;
			if (strchr(p, '@') == 0)
				// Local -- append defaultdomain
			{
				a->alias += '@';
				a->alias += me;
			}
		}
		fclose(fp);

		if (!list)
			return (1);	// Not found

		while ((a=list) != 0)
		{
			h->Alias(a->alias.c_str()); // TODO
			list=a->next;
			delete a;
		}

	}
	catch (...)
	{
		fclose(fp);

		struct ldapaliaslist *a;

		while ((a=list) != 0)
		{
			list=a->next;
			delete a;
		}
		throw;
	}

	return (0);
}
#endif

int	AliasSearch::Found(const char *address)
{
	if (TrySearch(module_record, address) ||
		TryVirtual(module_record, address, 0) ||
		TrySearch(local_record, address) ||
		TryVirtual(local_record, address, 0))
		return (1);
	return (0);
}

int	AliasSearch::Try(AliasRecord &as, const char *address, AliasHandler &h)
{
	std::string	s;

	as.Init(address);
	if (as.Init())	return (0);

	as.StartForEach();
	while ( (s=as.NextForEach()).size() )
		h.Alias(s.c_str()); // TODO
	return (1);
}

int	AliasSearch::TryVirtual(AliasRecord &as, const char *address,
				AliasHandler *h)
{
	std::string s;

	const char *p;
	const	char *me=config_me();
	std::string newaddrbuf;

	for (p=address; *p; p++)
	{
		if (*p == '@')	break;
		if (*p == '!' || *p == '%')	return (0);
	}
	if (!*p)	return (0);

	as.Init(p);
	if (as.Init())	return (0);

	as.StartForEach();
	while ( (s=as.NextForEach()).size() )
	{
		int	sl=s.size();

		newaddrbuf=s;

		if (sl && s[sl-1] != '!')
			newaddrbuf += "-";

		newaddrbuf += std::string(address, p);

		newaddrbuf += "@";
		newaddrbuf += me;

		if (h)
			h->Alias(newaddrbuf.c_str()); // TODO
	}
	return (1);
}

int	AliasSearch::TrySearch(AliasRecord &as, const char *address)
{
	as.Init(address);
	if (as.Init())	return (0);
	return (1);
}
