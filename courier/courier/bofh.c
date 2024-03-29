/*
** Copyright 2001-2018 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"bofh.h"
#include	"courier.h"
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<idn2.h>

unsigned max_bofh=100;
int max_bofh_ishard=0;


static struct bofh_list *bofh_freemail, *bofh_spamtrap, *bofh_badmx,
	*bofh_badfrom;
static struct bofh_list **freemailp, **spamtrapp, **badmxp, **badfromp;

static void addbofh(struct bofh_list ***, char *(*)(const char *), int);

static char *uaddress_lower(const char *p)
{
	char *s=ulocallower(p);
	char *r=udomainutf8(s);
	free(s);
	return r;
}

static void bofh_free(struct bofh_list *p)
{
	struct bofh_list *q;

	while ((q=p->aliases) != NULL)
	{
		p->aliases=q->next;
		free(q->name);
		free(q);
	}

	free(p->name);
	free(p);
}

void bofh_init()
{
	struct bofh_list *p;
	char buf[BUFSIZ];

	char *f=config_localfilename("bofh");
	FILE    *fp=fopen(f, "r");

	free(f);

	/* Just in case */
	while ((p=bofh_freemail) != 0)
	{
		bofh_freemail=p->next;
		bofh_free(p);
	}
	freemailp= &bofh_freemail;

	while ((p=bofh_spamtrap) != 0)
	{
		bofh_spamtrap=p->next;
		bofh_free(p);
	}
	spamtrapp= &bofh_spamtrap;

	while ((p=bofh_badmx) != 0)
	{
		bofh_badmx=p->next;
		bofh_free(p);
	}
	badmxp= &bofh_badmx;

	while ((p=bofh_badfrom) != 0)
	{
		bofh_badfrom=p->next;
		bofh_free(p);
	}
	badfromp= &bofh_badfrom;

	if (!fp)
		return;

	while (fgets(buf, sizeof(buf), fp))
	{
		char *p=strchr(buf, '\n');

		if (p) *p=0;

		p=strchr(buf, '#');
		if (p) *p=0;

		p=strtok(buf, " \t\r");

		if (!p)
			continue;

		if (strcasecmp(p, "freemail") == 0)
		{
			addbofh(&freemailp, &ualllower, 1);
		}
		else if (strcasecmp(p, "spamtrap") == 0)
		{
			addbofh(&spamtrapp, &uaddress_lower, 0);
		}
		else if (strcasecmp(p, "badmx") == 0)
		{
			addbofh(&badmxp, NULL, 0);
		}
		else if (strcasecmp(p, "badfrom") == 0)
		{
			addbofh(&badfromp, &uaddress_lower, 0);
		}
		else if (strcasecmp(p, "maxrcpts") == 0)
		{
			char *q=strtok(NULL, " \t\r");

			if (q)
			{
				unsigned n=atoi(q);

				if (n <= 0)
					n=max_bofh;

				max_bofh=n;
				q=strtok(NULL, " \t\r");
			}

			if (q)
				max_bofh_ishard=strcasecmp(q, "hard") == 0;
		}
		else if (strcasecmp(p, "opt") == 0)
		{
			char *env=strtok(NULL, " \t\r");
			char *var;

			if (!env)
				continue;

			var=courier_malloc(strlen(env)+1);
			strcpy(var, env);

			env=strchr(var, '=');
			if (!env)
			{
				free(var);
				continue;
			}

			*env=0;

			if (getenv(var))	/* Variable already set */
			{
				free(var);
				continue;
			}
			*env='=';
			putenv(var);
		}
	}
	fclose(fp);
}

static void addbofh(struct bofh_list ***p, char *(*func)(const char *),
		    int moreflag)
{
	char *q=strtok(NULL, " \t\r");
	struct bofh_list *b;

	if (!q)
		return;

	b=(struct bofh_list *)courier_malloc(sizeof(struct bofh_list));
	b->name=courier_strdup(q);
	if (func)
	{
		char *p=(*func)(b->name);
		free(b->name);
		b->name=p;
	}
	b->next=NULL;
	b->aliases=NULL;
	**p=b;
	*p= &b->next;

	if (moreflag)
		while ((q=strtok(NULL, " \t\r")) != NULL)
		{
			struct bofh_list *bb=(struct bofh_list *)
				courier_malloc(sizeof(struct bofh_list));

			bb->name=courier_strdup(q);
			if (func)
			{
				char *p=(*func)(bb->name);

				free(bb->name);
				bb->name=p;
			}

			bb->next=b->aliases;
			b->aliases=bb;
		}
}

static int chkbadlist(const char *, struct bofh_list *);

int bofh_chkbadfrom(const char *pp)
{
	return (chkbadlist(pp, bofh_badfrom));
}

int bofh_chkspamtrap(const char *pp)
{
	return (chkbadlist(pp, bofh_spamtrap));
}

static int chkusersubdom(const char *p, const char *d, const char *name)
{
	const char *dn;
	int lp, ln;
	if (p == NULL || d == NULL || d <= p || d[1] == '.'
	    || name == NULL || (dn = strrchr(name, '@')) == NULL)
	{
		return (0);
	}

	lp = d - p;
	ln = dn - name;
	if (lp != ln || strncmp(p, name, ln) != 0)
	{
		return (0);
	}

	++dn;
	++d;

	if (*dn == '.')
	{
		if (strlen(d) <= strlen(dn))
			return 0;

		d=d+strlen(d)-strlen(dn);

	}

	return strcmp(d, dn) == 0;
}

static int chkbadlist(const char *pp, struct bofh_list *b)
{
	char *p=uaddress_lower(pp);
	const char *d;
	int l, ll;

	d=strrchr(p, '@');
	l=d ? strlen(d):0;

	for (; b; b=b->next)
	{
		if ((d && strcmp(d, b->name) == 0)
		    || /* Entire domain with user ID */
		    chkusersubdom(p, d, b->name)
		    || /* Entire domain without user ID */
		    strcmp(p, b->name) == 0
		    || (d && strncmp(b->name, "@.", 2) == 0
			&& (ll=strlen(b->name+1)) < l
			&& strcmp(d+l - ll, b->name+1) == 0))
		{
			free(p);
			return (1);	/* Entire domain matches */
		}
	}
	free(p);
	return (0);
}

struct bofh_list *bofh_chkfreemail(const char *email)
{
	char *s=uaddress_lower(email);
	char *pp;
	struct bofh_list *b;

	pp=strrchr(s, '@');
	if (!pp)
	{
		free(s);
		return (NULL);
	}
	++pp;

	for (b=bofh_freemail; b; b=b->next)
	{
		if (strcmp(b->name, pp) == 0)
		{
			free(s);
			return (b);
		}
	}
	free(s);
	return (NULL);
}

int bofh_chkbadmx(const char *p)
{
	struct bofh_list *b;

	if (strncasecmp(p, "::ffff:", 7) == 0)
		p += 7;

	for (b=bofh_badmx; b; b=b->next)
	{
		if (strcmp(b->name, p) == 0)
			return (1);
	}
	return (0);
}

/*
** Check if an SPF BOFH setting contains the keyword.
*/

int bofh_checkspf(const char *envvarname,
		  const char *defaultValue, const char *keyword)
{
	char buf[256];
	const char *p=getenv(envvarname);
	char *s;

	if (!p || !*p) p=defaultValue;

	buf[0]=0;
	strncat(buf, p, sizeof(buf)-1);

	for (s=buf; (s=strtok(s, ",")) != NULL; s=NULL)
		if (strcmp(s, keyword) == 0)
			return 1;
	return 0;
}

/*
** "all" means any status.
*/

int bofh_checkspf_status(const char *envvarname,
			 const char *defaultValue, const char *keyword)
{
	if (bofh_checkspf(envvarname, defaultValue, "all") ||
	    bofh_checkspf(envvarname, defaultValue, keyword))
		return 1;
	return 0;
}
