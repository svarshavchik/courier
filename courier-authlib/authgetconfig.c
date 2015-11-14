/*
** Copyright 2012-2015 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	"auth.h"
#include	"courierauthdebug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

static const char *configfilename=0;
static char *configauth=0;
static size_t configauth_size=0;

#define err courier_auth_err

const char *authgetconfig(const char *filename, const char *env)
{
	size_t	i;
	char	*p=0;
	int	l=strlen(env);

	if (configfilename && strcmp(filename, configfilename))
	{
		if (configauth)
			free(configauth);
		configauth=0;
		configauth_size=0;
	}

	configfilename=filename;

	if (!configauth)
	{
		FILE	*f=fopen(filename, "r");
		struct	stat	buf;

		if (!f)	return (0);
		if (fstat(fileno(f), &buf) ||
			(configauth=malloc(buf.st_size+2)) == 0)
		{
			fclose(f);
			return (0);
		}
		if (fread(configauth, buf.st_size, 1, f) != 1)
		{
			free(configauth);
			configauth=0;
			fclose(f);
			return (0);
		}
		configauth[configauth_size=buf.st_size]=0;

		for (i=0; i<configauth_size; i++)
			if (configauth[i] == '\n')
			{	/* siefca@pld.org.pl */
				if (!i || configauth[i-1] != '\\')
				{
					configauth[i]='\0';
				}
				else
				{
					configauth[i]=configauth[i-1]= ' ';
				}
			}
		fclose(f);
	}

	for (i=0; i<configauth_size; )
	{
		p=configauth+i;
		if (strncmp(p, env, l) == 0 &&
			isspace((int)(unsigned char)p[l]))
		{
			p += l;
			while (*p && *p != '\n' &&
				isspace((int)(unsigned char)*p))
				++p;
			break;
		}

		while (i < configauth_size)
			if (configauth[i++] == 0)	break;
	}

	if (i < configauth_size)
		return (p);
	return (0);
}

/* siefca@pld.org.pl */
#define		MAX_SUBSTITUTION_LEN	32
#define		SV_BEGIN_MARK		"$("
#define		SV_END_MARK		")"
#define		SV_BEGIN_LEN		((sizeof(SV_BEGIN_MARK))-1)
#define		SV_END_LEN		((sizeof(SV_END_MARK))-1)

/* siefca@pld.org.pl */
struct var_data {
	const char *name;
	const char *value;
	const size_t size;
	size_t value_length;
	} ;

/* siefca@pld.org.pl */
typedef int (*parsefunc)(const char *, size_t, void *);

/* siefca@pld.org.pl */
static struct var_data *get_variable (const char *begin, size_t len,
				      struct var_data *vdt)
{
struct var_data *vdp;

	if (!begin || !vdt) /* should never happend */
	{
		err("get_variable: critical error while "
				 "parsing substitution variable");
		return NULL;
	}
	if (len < 1)
	{
		err("get_variable: unknown empty substitution "
				 "variable - aborting");
		return NULL;
	}
	if (len > MAX_SUBSTITUTION_LEN)
	{
		err("get_variable: variable name too long "
				 "while parsing substitution. "
				 "name begins with "
				 SV_BEGIN_MARK
				 "%.*s...", MAX_SUBSTITUTION_LEN, begin);
		return NULL;
	}

	for (vdp=vdt; vdp->name; vdp++)
		if (vdp->size == len+1 &&
		    !strncmp(begin, vdp->name, len))
		{
			if (!vdp->value)
				vdp->value = "";
			if (!vdp->value_length)		/* length cache */
				vdp->value_length = strlen (vdp->value);
			return vdp;
		}

	err("get_variable: unknown substitution variable "
			 SV_BEGIN_MARK
			 "%.*s"
			 SV_END_MARK
			 , (int)len, begin);

	return NULL;
}

/* siefca@pld.org.pl */
static int ParsePlugin_counter (const char *p, size_t length, void *vp)
{
	if (!p || !vp || length < 0)
	{
		err("get_variable: bad arguments while counting "
				 "query string");
		return -1;
	}

	*((size_t *)vp) += length;

	return 0;
}

/* siefca@pld.org.pl */
static int ParsePlugin_builder (const char *p, size_t length, void *vp)
{
char	**strptr = (char **) vp;

	if (!p || !vp || length < 0)
	{
		err("get_variable: bad arguments while building "
				 "query string");
		return -1;
	}

	if (!length) return 0;
	memcpy ((void *) *strptr, (void *) p, length);
	*strptr += length;

	return 0;
}

/* siefca@pld.org.pl */
static int parse_core  (const char *source, struct var_data *vdt,
			parsefunc outfn, void *result)
{
size_t	v_size		= 0,
	t_size		= 0;
const char	*p, *q, *e,
		*v_begin, *v_end,
		*t_begin, *t_end;
struct var_data	*v_ptr;

	if (!source)
		source = "";
	if (!result)
	{
		err("auth_parse: no memory allocated for result "
				 "while parser core was invoked");
		return -1;
	}
	if (!vdt)
	{
		err("auth_parse: no substitution table found "
				 "while parser core was invoked");
		return -1;
	}

	q = source;
	while ( (p=strstr(q, SV_BEGIN_MARK)) )
	{
		e = strstr (p, SV_END_MARK);
		if (!e)
		{
			err("auth_parse: syntax error in "
					 "substitution "
					 "- no closing symbol found! "
					 "bad variable begins with:"
					 "%.*s...", MAX_SUBSTITUTION_LEN, p);
			return -1;
		}

		/*
		 **
		 **	     __________sometext$(variable_name)_________
		 **	 	       |      |  |    	     |
		 **	        t_begin' t_end'  `v_begin    `v_end
		 **
		  */

		v_begin	= p+SV_BEGIN_LEN; /* variable field ptr		    */
		v_end	= e-SV_END_LEN;	  /* variable field last character  */
		v_size	= v_end-v_begin+1;/* variable field length	    */

		t_begin	= q;		  /* text field ptr		    */
		t_end	= p-1;		  /* text field last character	    */
		t_size	= t_end-t_begin+1;/* text field length		    */

		/* work on text */
		if ( (outfn (t_begin, t_size, result)) == -1 )
			return -1;

		/* work on variable */
		v_ptr = get_variable (v_begin, v_size, vdt);
		if (!v_ptr) return -1;

		if ( (outfn (v_ptr->value, v_ptr->value_length, result)) == -1 )
			return -1;

		q = e + 1;
	}

	/* work on last part of text if any */
	if (*q != '\0')
		if ( (outfn (q, strlen(q), result)) == -1 )
			return -1;

	return 0;
}

/* siefca@pld.org.pl */
static char *parse_string (const char *source, struct var_data *vdt)
{
struct var_data *vdp	= NULL;
char	*output_buf	= NULL,
	*pass_buf	= NULL;
size_t	buf_size	= 2;

	if (source == NULL || *source == '\0' ||
	    vdt == NULL    || vdt[0].name == NULL)
	{
		err("auth_parse: source clause is empty "
		       "- this is critical error");
		return NULL;
	}

	/* zero var_data length cache - important! */
	for (vdp=vdt; vdp->name; vdp++)
		vdp->value_length = 0;


	/* phase 1 - count and validate string */
	if ( (parse_core (source, vdt, &ParsePlugin_counter, &buf_size)) != 0)
		return NULL;

	/* phase 2 - allocate memory */
	output_buf = malloc (buf_size);
	if (!output_buf)
	{
		perror ("malloc");
		return NULL;
	}
	pass_buf = output_buf;

	/* phase 3 - build the output string */
	if ( (parse_core (source, vdt, &ParsePlugin_builder, &pass_buf)) != 0)
	{
		free (output_buf);
		return NULL;
	}
	*pass_buf = '\0';

	return output_buf;
}

static char *local_part_escaped(const char *username,
				char *(*escape_func)(const char *, size_t))
{
	const char *p=strchr(username, '@');
	size_t n=p ? p-username:strlen(username);

	return escape_func(username, n);
}

static char *domain_part_escaped(const char *username,
				 const char *defdomain,
				 char *(*escape_func)(const char *, size_t))
{
	const char *p=strchr(username, '@');
	size_t n;

	if (p)
		++p;
	else
		p=defdomain;

	n=strlen(p);

	return escape_func(p, n);
}

static int local_and_domain_part_escaped(char *(*escape_func)(const char *, size_t),
					 const char *username,
					 const char *defdomain,
					 char **local_ret,
					 char **domain_ret)
{
	if ((*local_ret=local_part_escaped(username, escape_func)) == NULL)
		return 0;

	if ((*domain_ret=domain_part_escaped(username, defdomain,
					     escape_func)) == NULL)
	{
		free(*local_ret);
		return 0;
	}

	return 1;
}

/* siefca@pld.org.pl */
char *auth_parse_select_clause (char *(*escape_func)(const char *, size_t),
				const char *clause, const char *username,
				const char *defdomain,
				const char *service)
{
	char *str;

	static struct var_data vd[]={
		{"local_part",	NULL,	sizeof("local_part"),	0},
		{"domain",		NULL,	sizeof("domain"),	0},
		{"service",		NULL,	sizeof("service"),	0},
		{NULL,		NULL,	0,			0}};

	char *l_part;
	char *d_part;

	if (clause == NULL || *clause == '\0' ||
	    !username || *username == '\0')
		return NULL;

	if (!local_and_domain_part_escaped(escape_func,
					   username, defdomain,
					   &l_part, &d_part))
		return NULL;

	vd[0].value=l_part;
	vd[1].value=d_part;
	vd[2].value     = service;

	str=parse_string (clause, vd);
	free(l_part);
	free(d_part);
	return str;
}

/* siefca@pld.org.pl */
char *auth_parse_chpass_clause (char *(*escape_func)(const char *, size_t),
				const char *clause, const char *username,
				const char *defdomain, const char *newpass,
				const char *newpass_crypt)
{
	char *str;

	static struct var_data vd[]={
		{"local_part",	NULL,	sizeof("local_part"),		0},
		{"domain",	NULL,	sizeof("domain"),		0},
		{"newpass",	NULL, 	sizeof("newpass"),		0},
		{"newpass_crypt", NULL,	sizeof("newpass_crypt"),	0},
		{NULL,		NULL,	0,				0}};
	char *l_part;
	char *d_part;

	if (clause == NULL || *clause == '\0'		||
	    !username || *username == '\0'		||
	    !newpass || *newpass == '\0'		||
	    !newpass_crypt || *newpass_crypt == '\0')	return NULL;

	if (!local_and_domain_part_escaped(escape_func,
					   username, defdomain,
					   &l_part, &d_part))
		return NULL;

	vd[0].value=l_part;
	vd[1].value=d_part;
	vd[2].value	= newpass;
	vd[3].value	= newpass_crypt;

	if (!vd[0].value || !vd[1].value ||
	    !vd[2].value || !vd[3].value)
	{
		free(l_part);
		free(d_part);
		return NULL;
	}

	str=parse_string (clause, vd);
	free(l_part);
	free(d_part);
	return str;
}
