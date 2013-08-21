/*
** Copyright 2000-2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/


#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<libpq-fe.h>
#include	<time.h>

#include	"authpgsql.h"
#include	"authpgsqlrc.h"
#include	"auth.h"
#include	"courierauthdebug.h"

#define err courier_auth_err

/* tom@minnesota.com */
#define		MAX_SUBSTITUTION_LEN	32
#define		SV_BEGIN_MARK		"$("
#define		SV_END_MARK		")"
#define		SV_BEGIN_LEN		((sizeof(SV_BEGIN_MARK))-1)
#define		SV_END_LEN		((sizeof(SV_END_MARK))-1)


/* tom@minnesota.com */
struct var_data {			
	const char *name;
	const char *value;
	const size_t size;
	} ;

/* tom@minnesota.com */
typedef int (*parsefunc)(const char *, size_t, void *);

static const char *read_env(const char *env)
{
static char *pgsqlauth=0;
static size_t pgsqlauth_size=0;
size_t	i;
char	*p=0;
int	l=strlen(env);

	if (!pgsqlauth)
	{
	FILE	*f=fopen(AUTHPGSQLRC, "r");
	struct	stat	buf;

		if (!f)	return (0);
		if (fstat(fileno(f), &buf) ||
			(pgsqlauth=malloc(buf.st_size+2)) == 0)
		{
			fclose(f);
			return (0);
		}
		if (fread(pgsqlauth, buf.st_size, 1, f) != 1)
		{
			free(pgsqlauth);
			pgsqlauth=0;
			fclose(f);
			return (0);
		}
		pgsqlauth[pgsqlauth_size=buf.st_size]=0;

		for (i=0; i<pgsqlauth_size; i++)
			if (pgsqlauth[i] == '\n')
			{	/* tom@minnesota.com */
				if (!i || pgsqlauth[i-1] != '\\')
				{
					pgsqlauth[i]='\0';
				}
				else
				{
					pgsqlauth[i]=pgsqlauth[i-1]= ' ';
				}
			}
		fclose(f);
	}

	for (i=0; i<pgsqlauth_size; )
	{
		p=pgsqlauth+i;
		if (memcmp(p, env, l) == 0 &&
			isspace((int)(unsigned char)p[l]))
		{
			p += l;
			while (*p && *p != '\n' &&
				isspace((int)(unsigned char)*p))
				++p;
			break;
		}

		while (i < pgsqlauth_size)
			if (pgsqlauth[i++] == 0)	break;
	}

	if (i < pgsqlauth_size)
		return (p);
	return (0);
}

static PGresult *pgresult=0;

static PGconn *pgconn=0;

/*
* session variables can be set once for the whole session
*/

static void set_session_options(void)
{
	const char *character_set=read_env("PGSQL_CHARACTER_SET"), *check;

	if (character_set)
	{
		PQsetClientEncoding(pgconn, character_set);
		check = pg_encoding_to_char(PQclientEncoding(pgconn));
		if (strcmp(character_set, check) != 0)
		{
			err("Cannot set Postgresql character set \"%s\", working with \"%s\"\n",
			    character_set, check);
		}
		else
		{
			DPRINTF("Install of a character set for Postgresql: %s", character_set);
		}
        }
}



/*
static FILE *DEBUG=0;
*/

static int do_connect()
{
const	char *server;
const	char *userid;
const	char *password;
const	char *database;
const 	char *server_port=0;
const 	char *server_opt=0;
/*
	if (!DEBUG) {
		DEBUG=fopen("/tmp/courier.debug","a");
	}
	fprintf(DEBUG,"Apro il DB!\n");
	fflush(DEBUG);
*/

/*
** Periodically detect dead connections.
*/
	if (pgconn)
	{
		static time_t last_time=0;
		time_t t_check;

		time(&t_check);

		if (t_check < last_time)
			last_time=t_check;	/* System clock changed */

		if (t_check < last_time + 60)
			return (0);

		last_time=t_check;
			
		if (PQstatus(pgconn) == CONNECTION_OK) return (0);

		DPRINTF("authpgsqllib: PQstatus failed, connection lost");
		PQfinish(pgconn);
		pgconn=0;
	}

	server=read_env("PGSQL_HOST");
	server_port=read_env("PGSQL_PORT");
	userid=read_env("PGSQL_USERNAME");
	password=read_env("PGSQL_PASSWORD");
	database=read_env("PGSQL_DATABASE");
	server_opt=read_env("PGSQL_OPT");

/*
	fprintf(DEBUG,"Letti i parametri!\n");
	fflush(DEBUG);
*/

	if (!userid)
	{
		err("authpgsql: PGSQL_USERNAME not set in "
			AUTHPGSQLRC ".");
		return (-1);
	}

	if (!database)
	{
		err("authpgsql: PGSQL_DATABASE not set in "
			AUTHPGSQLRC ".");
		return (-1);
	}

/*
	fprintf(DEBUG,"Connecting to db:%s:%s\%s user=%s pass=%s\n",server,server_port,database,userid,password);
	fflush(DEBUG);
*/
	pgconn = PQsetdbLogin(server, server_port, server_opt, NULL , database,userid,password);

	if (PQstatus(pgconn) == CONNECTION_BAD)
    	{
       		err("Connection to server '%s' userid '%s' database '%s' failed.",
       			server ? server : "<null>",
       			userid ? userid : "<null>",
       			database);
        	err("%s", PQerrorMessage(pgconn));
		PQfinish(pgconn);
		pgconn=0;
		return -1;
    	}
/*
	fprintf(DEBUG,"Connected!\n");
	fflush(DEBUG);
*/

	set_session_options();
	return 0;

}

void auth_pgsql_cleanup()
{
	if (pgconn)
	{
		PQfinish(pgconn);
		pgconn=0;
	}
}

static struct authpgsqluserinfo ui={0, 0, 0, 0, 0, 0, 0, 0};

static char *get_username_escaped(const char *username,
				  const char *defdomain)
{
	char *username_escaped;
	int *error = NULL;

	if (!defdomain)
		defdomain="";

        username_escaped=malloc(strlen(username)*2+2+strlen(defdomain));

	if (!username_escaped)
	{
		perror("malloc");
		return 0;
 	}

	PQescapeStringConn(pgconn, username_escaped, username, strlen(username), error);

	if (strchr(username, '@') == 0 && *defdomain)
		strcat(strcat(username_escaped, "@"), defdomain);

	return username_escaped;
}

/* tom@minnesota.com */
static struct var_data *get_variable (const char *begin, size_t len,
				      struct var_data *vdt)
{
struct var_data *vdp;

	if (!begin || !vdt) /* should never happend */
	{
		err("authpgsql: critical error while "
				 "parsing substitution variable");
		return NULL;
	}
	if (len < 1)
	{
		err("authpgsql: unknown empty substitution "
				 "variable - aborting");
		return NULL;
	}
	if (len > MAX_SUBSTITUTION_LEN)
	{
		err("authpgsql: variable name too long "
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
			return vdp;
		}
	
	err("authpgsql: unknown substitution variable "
			 SV_BEGIN_MARK
			 "%.*s"
			 SV_END_MARK
			 , (int)len, begin);
	
	return NULL;
}

/* tom@minnesota.com */
static int ParsePlugin_counter (const char *p, size_t length, void *vp)
{
	if (!p || !vp || length < 0)
	{
		err("authpgsql: bad arguments while counting "
				 "query string");
		return -1;
	}
	
	*((size_t *)vp) += length;
   
	return 0;
}

/* tom@minnesota.com */
static int ParsePlugin_builder (const char *p, size_t length, void *vp)
{
char	**strptr = (char **) vp;

	if (!p || !vp || length < 0)
	{
		err("authpgsql: bad arguments while building "
				 "query string");
		return -1;
	}
	
	if (!length) return 0;
	memcpy ((void *) *strptr, (void *) p, length);
	*strptr += length;
	
	return 0;
}

/* tom@minnesota.com */
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
		err("authpgsql: no memory allocated for result "
				 "while parser core was invoked");
		return -1;
	}
	if (!vdt)
	{
		err("authpgsql: no substitution table found "
				 "while parser core was invoked");
		return -1;
	}
	
	q = source;
	while ( (p=strstr(q, SV_BEGIN_MARK)) )
	{
		char *enc;

		e = strstr (p, SV_END_MARK);
		if (!e)
		{
			err("authpgsql: syntax error in "
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

		enc=malloc(strlen(v_ptr->value)*2+1);

		if (!enc)
			return -1;

		PQescapeStringConn(pgconn, enc, v_ptr->value,
				   strlen(v_ptr->value), NULL);

		if ( (outfn (enc, strlen(enc), result)) == -1 )
		{
			free(enc);
			return -1;
		}
		free(enc);

		q = e + 1;
	}

	/* work on last part of text if any */
	if (*q != '\0')
		if ( (outfn (q, strlen(q), result)) == -1 )
			return -1;

	return 0;
}

/* tom@minnesota.com */
static char *parse_string (const char *source, struct var_data *vdt)
{
char	*output_buf	= NULL,
	*pass_buf	= NULL;
size_t	buf_size	= 2;

	if (source == NULL || *source == '\0' || 
	    vdt == NULL    || vdt[0].name == NULL)
	{
		err("authpgsql: source clause is empty "
				 "- this is critical error");
		return NULL;
	}

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

static char *get_localpart (const char *username)
{
	char *p=strdup(username);
	char *q;

	if (!p)
		return 0;

	q=strchr(p, '@');

	if (q)
		*q=0;

	return p;
}

static const char *get_domain (const char *username, const char *defdomain)
{
	const char *p=strchr(username, '@');

	if (p)
		return p+1;

	return defdomain;
}

/* tom@minnesota.com */
static char *parse_select_clause (const char *clause, const char *username,
				  const char *defdomain,
				  const char *service)
{
	char *localpart, *ret;
	static struct var_data vd[]={
		{"local_part",	NULL,	sizeof("local_part")},
		{"domain",	NULL,	sizeof("domain")},
		{"service",	NULL,	sizeof("service")},
		{NULL,		NULL,	0}};

	if (clause == NULL || *clause == '\0' ||
	    !username || *username == '\0')
		return NULL;
	
	localpart=get_localpart(username);
	if (!localpart)
		return NULL;

	vd[0].value	= localpart;
	vd[1].value	= get_domain (username, defdomain);

	if (!vd[1].value)
	{
		free(localpart);
		return NULL;
	}
	vd[2].value	= service;

	ret=parse_string (clause, vd);
	free(localpart);
	return ret;
}

/* tom@minnesota.com */
static char *parse_chpass_clause (const char *clause, const char *username,
				  const char *defdomain, const char *newpass,
				  const char *newpass_crypt)
{
	char *localpart, *ret;

	static struct var_data vd[]={
		{"local_part",	NULL,	sizeof("local_part")},
		{"domain",	NULL,	sizeof("domain")},
		{"newpass",	NULL, 	sizeof("newpass")},
		{"newpass_crypt", NULL,	sizeof("newpass_crypt")},
		{NULL,		NULL,	0}};

	if (clause == NULL || *clause == '\0'		||
	    !username || *username == '\0'		||
	    !newpass || *newpass == '\0'		||
	    !newpass_crypt || *newpass_crypt == '\0')	return NULL;

	localpart=get_localpart(username);
	if (!localpart)
		return NULL;

	vd[0].value	= localpart;
	vd[1].value	= get_domain (username, defdomain);
	vd[2].value	= newpass;
	vd[3].value	= newpass_crypt;
	
	if (!vd[1].value || !vd[2].value || !vd[3].value)
	{
		free(localpart);
		return NULL;
	}

	ret=parse_string (clause, vd);
	free(localpart);
	return ret;
}

static void initui()
{

	if (ui.username)
		free(ui.username);
	if (ui.cryptpw)
		free(ui.cryptpw);
	if (ui.clearpw)
		free(ui.clearpw);
	if (ui.home)
		free(ui.home);
	if (ui.maildir)
		free(ui.maildir);
	if (ui.quota)
		free(ui.quota);
	if (ui.fullname)
		free(ui.fullname);
	if (ui.options)
		free(ui.options);
	memset(&ui, 0, sizeof(ui));
}

struct authpgsqluserinfo *auth_pgsql_getuserinfo(const char *username,
						 const char *service)
{
	const char *defdomain, *select_clause;
	char	*querybuf;
	size_t query_size;
	char dummy_buf[1];

#define SELECT_QUERY "SELECT %s, %s, %s, %s, %s, %s, %s, %s, %s, %s FROM %s WHERE %s = '%s' %s%s%s", \
		login_field, crypt_field, clear_field, \
		uid_field, gid_field, home_field, maildir_field, \
		quota_field, \
		name_field, \
		options_field, \
		user_table, login_field, username_escaped, \
		where_pfix, where_clause, where_sfix

	if (do_connect())	return (0);

	initui();

/*
	fprintf(DEBUG,"1Leggo parametri\n");
	fflush(DEBUG);
*/
	select_clause=read_env("PGSQL_SELECT_CLAUSE");
	defdomain=read_env("DEFAULT_DOMAIN");
	if (!defdomain)	defdomain="";

	if (!select_clause) /* tom@minnesota.com */
	{
		const char	*user_table,
				*crypt_field,
				*clear_field,
				*name_field,
			       	*uid_field,
			       	*gid_field,
				*login_field,
			       	*home_field,
			       	*maildir_field,
			       	*quota_field,
				*options_field,
			       	*where_clause;

		const char *where_pfix, *where_sfix;
		char *username_escaped;

		user_table=read_env("PGSQL_USER_TABLE");

		if (!user_table)
		{
			err("authpgsql: PGSQL_USER_TABLE not set in "
				AUTHPGSQLRC ".");
			return (0);
		}

		crypt_field=read_env("PGSQL_CRYPT_PWFIELD");
		clear_field=read_env("PGSQL_CLEAR_PWFIELD");
		name_field=read_env("PGSQL_NAME_FIELD");

		if (!crypt_field && !clear_field)
		{
			err("authpgsql: PGSQL_CRYPT_PWFIELD and "
				"PGSQL_CLEAR_PWFIELD not set in " AUTHPGSQLRC ".");
			return (0);
		}
		if (!crypt_field) crypt_field="''";
		if (!clear_field) clear_field="''";
		if (!name_field) name_field="''";

		uid_field = read_env("PGSQL_UID_FIELD");
		if (!uid_field) uid_field = "uid";

		gid_field = read_env("PGSQL_GID_FIELD");
		if (!gid_field) gid_field = "gid";

		login_field = read_env("PGSQL_LOGIN_FIELD");
		if (!login_field) login_field = "id";

		home_field = read_env("PGSQL_HOME_FIELD");
		if (!home_field) home_field = "home";

		maildir_field=read_env(service && strcmp(service, "courier")==0
				       ? "PGSQL_DEFAULTDELIVERY"
				       : "PGSQL_MAILDIR_FIELD");
		if (!maildir_field) maildir_field="''";

		quota_field=read_env("PGSQL_QUOTA_FIELD");
		if (!quota_field) quota_field="''"; 

		options_field=read_env("PGSQL_AUXOPTIONS_FIELD");
		if (!options_field) options_field="''"; 

		where_clause=read_env("PGSQL_WHERE_CLAUSE");
		if (!where_clause) where_clause = "";

		where_pfix=where_sfix="";

		if (strcmp(where_clause, ""))
		{
			where_pfix=" AND (";
			where_sfix=")";
		}

		username_escaped=get_username_escaped(username, defdomain);

		if (!username_escaped)
			return 0;

		query_size=snprintf(dummy_buf, 1, SELECT_QUERY);

		querybuf=malloc(query_size+1);

		if (!querybuf)
		{
			free(username_escaped);
			perror("malloc");
			return 0;
		}

		snprintf(querybuf, query_size+1, SELECT_QUERY);
		free(username_escaped);
	}
	else
	{
		/* tom@minnesota.com */
		querybuf=parse_select_clause (select_clause, username,
					      defdomain, service);
		if (!querybuf)
		{
			DPRINTF("authpgsql: parse_select_clause failed (DEFAULT_DOMAIN not defined?)");
			return 0;
		}
	}

	DPRINTF("SQL query: %s", querybuf);
	pgresult = PQexec(pgconn, querybuf);
    	if (!pgresult || PQresultStatus(pgresult) != PGRES_TUPLES_OK)
    	{
		DPRINTF("PQexec failed, reconnecting: %s", PQerrorMessage(pgconn));
        	if (pgresult) PQclear(pgresult);
	
		/* <o.blasnik@nextra.de> */

		auth_pgsql_cleanup();

		if (do_connect())
		{
			free(querybuf);
			return (0);
		}

		pgresult = PQexec(pgconn, querybuf);
    		if (!pgresult || PQresultStatus(pgresult) != PGRES_TUPLES_OK)
		{
			DPRINTF("PQexec failed second time, giving up: %s", PQerrorMessage(pgconn));
        		if (pgresult) PQclear(pgresult);
			free(querybuf);
			auth_pgsql_cleanup();
			/* Server went down, that's OK,
			** try again next time.
			*/
			return (0);
		}
	}
	free(querybuf);

		if (PQntuples(pgresult)>0)
		{
		char *t, *endp;
		int	num_fields = PQnfields(pgresult);

			if (num_fields < 6)
			{
				DPRINTF("incomplete row, only %d fields returned",
					num_fields);
				PQclear(pgresult);
				return 0;
			}

			t=PQgetvalue(pgresult,0,0);
			if (t && t[0]) ui.username=strdup(t);
			t=PQgetvalue(pgresult,0,1);
			if (t && t[0]) ui.cryptpw=strdup(t);
			t=PQgetvalue(pgresult,0,2);
			if (t && t[0]) ui.clearpw=strdup(t);
			t=PQgetvalue(pgresult,0,3);
			if (!t || !t[0] ||
			   (ui.uid=strtol(t, &endp, 10), endp[0] != '\0'))
			{
				DPRINTF("invalid value for uid: '%s'",
					t ? t : "<null>");
				PQclear(pgresult);
				return 0;
			}
			t=PQgetvalue(pgresult,0,4);
			if (!t || !t[0] ||
			   (ui.gid=strtol(t, &endp, 10), endp[0] != '\0'))
			{
				DPRINTF("invalid value for gid: '%s'",
					t ? t : "<null>");
				PQclear(pgresult);
				return 0;
			}
			t=PQgetvalue(pgresult,0,5);
			if (t && t[0])
				ui.home=strdup(t);
			else
			{
				DPRINTF("required value for 'home' (column 6) is missing");
				PQclear(pgresult);
				return 0;
			}
			t=num_fields > 6 ? PQgetvalue(pgresult,0,6) : 0;
			if (t && t[0]) ui.maildir=strdup(t);
			t=num_fields > 7 ? PQgetvalue(pgresult,0,7) : 0;
			if (t && t[0]) ui.quota=strdup(t);
			t=num_fields > 8 ? PQgetvalue(pgresult,0,8) : 0;
			if (t && t[0]) ui.fullname=strdup(t);
			t=num_fields > 9 ? PQgetvalue(pgresult,0,9) : 0;
			if (t && t[0]) ui.options=strdup(t);
		}
		else
		{
			DPRINTF("zero rows returned");
			PQclear(pgresult);
			return (&ui);
		}
        	PQclear(pgresult);

	return (&ui);
}

int auth_pgsql_setpass(const char *user, const char *pass,
		       const char *oldpass)
{
	char *newpass_crypt;
	char *sql_buf;
	size_t sql_buf_size;
	char dummy_buf[1];
	int rc=0;

	char *clear_escaped;
	char *crypt_escaped;
	int  *error = NULL;

	char *username_escaped;

	const char *clear_field=NULL;
	const char *crypt_field=NULL;
	const char *defdomain=NULL;
	const char *where_clause=NULL;
	const char *user_table=NULL;
	const char *login_field=NULL;
	const char *chpass_clause=NULL; /* tom@minnesota.com */

	if (!pgconn)
		return (-1);


	if (!(newpass_crypt=authcryptpasswd(pass, oldpass)))
		return (-1);

	clear_escaped=malloc(strlen(pass)*2+1);

        if (!clear_escaped)
        {
                perror("malloc");
                free(newpass_crypt);
                return -1;
        }

        crypt_escaped=malloc(strlen(newpass_crypt)*2+1);

        if (!crypt_escaped)
        {
                perror("malloc");
                free(clear_escaped);
                free(newpass_crypt);
                return -1;
        }

        PQescapeStringConn(pgconn, clear_escaped, pass, strlen(pass), error);
        PQescapeStringConn(pgconn, crypt_escaped,
                                 newpass_crypt, strlen(newpass_crypt), error);



	/* tom@minnesota.com */
	chpass_clause=read_env("PGSQL_CHPASS_CLAUSE");
	defdomain=read_env("DEFAULT_DOMAIN");
	user_table=read_env("PGSQL_USER_TABLE");
	if (!chpass_clause)
	{
		login_field = read_env("PGSQL_LOGIN_FIELD");
		if (!login_field) login_field = "id";
		crypt_field=read_env("PGSQL_CRYPT_PWFIELD");
		clear_field=read_env("PGSQL_CLEAR_PWFIELD");
		where_clause=read_env("PGSQL_WHERE_CLAUSE");

		username_escaped=get_username_escaped(user, defdomain);

		if (!username_escaped)
			return -1;

		if (!where_clause)
			where_clause="";

		if (!crypt_field)
			crypt_field="";

		if (!clear_field)
			clear_field="";

#define DEFAULT_SETPASS_UPDATE \
		"UPDATE %s SET %s%s%s%s %s %s%s%s%s WHERE %s='%s' %s%s%s", \
			user_table,					\
			*clear_field ? clear_field:"",			\
			*clear_field ? "='":"",				\
			*clear_field ? clear_escaped:"",		\
			*clear_field ? "'":"",				\
									\
			*clear_field && *crypt_field ? ",":"",		\
									\
			*crypt_field ? crypt_field:"",			\
			*crypt_field ? "='":"",				\
			*crypt_field ? crypt_escaped:"",		\
			*crypt_field ? "'":"",				\
									\
			login_field, username_escaped,			\
			*where_clause ? " AND (":"", where_clause,	\
			*where_clause ? ")":""


		sql_buf_size=snprintf(dummy_buf, 1, DEFAULT_SETPASS_UPDATE);

		sql_buf=malloc(sql_buf_size+1);

		if (sql_buf)
			snprintf(sql_buf, sql_buf_size+1,
				 DEFAULT_SETPASS_UPDATE);

		free(username_escaped);
	}
	else
	{
		sql_buf=parse_chpass_clause(chpass_clause,
					    user,
					    defdomain,
					    pass,
					    newpass_crypt);
	}

	if (!sql_buf)
	{
		free(clear_escaped);
		free(newpass_crypt);
		return (-1);
	}
	if (courier_authdebug_login_level >= 2)
	{
		DPRINTF("setpass SQL: %s", sql_buf);
	}
	pgresult=PQexec (pgconn, sql_buf);
	if (!pgresult || PQresultStatus(pgresult) != PGRES_COMMAND_OK)
	{
		DPRINTF("setpass SQL failed");
		rc= -1;
		auth_pgsql_cleanup();
	}
	PQclear(pgresult);
	free(clear_escaped);
	free(crypt_escaped);
	free(newpass_crypt);
	free(sql_buf);
	return (rc);
}

void auth_pgsql_enumerate( void(*cb_func)(const char *name,
					  uid_t uid,
					  gid_t gid,
					  const char *homedir,
					  const char *maildir,
					  const char *options,
					  void *void_arg),
			   void *void_arg)
{
	const char *select_clause, *defdomain;
	char	*querybuf;

	int i,n;

	if (do_connect())
	{
		(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
		return;
	}

	initui();

	select_clause=read_env("PGSQL_ENUMERATE_CLAUSE");
	defdomain=read_env("DEFAULT_DOMAIN");
	if (!defdomain || !defdomain[0])
		defdomain="*"; /* otherwise parse_select_clause fails */

	if (!select_clause) /* tom@minnesota.com */
	{
		const char	*user_table,
				*uid_field,
				*gid_field,
				*login_field,
				*home_field,
				*maildir_field,
				*options_field,
				*where_clause;
		char dummy_buf[1];
		size_t query_len;

		user_table=read_env("PGSQL_USER_TABLE");

		if (!user_table)
		{
			err("authpgsql: PGSQL_USER_TABLE not set in "
			       AUTHPGSQLRC ".");
			return;
		}

		uid_field = read_env("PGSQL_UID_FIELD");
		if (!uid_field) uid_field = "uid";

		gid_field = read_env("PGSQL_GID_FIELD");
		if (!gid_field) gid_field = "gid";

		login_field = read_env("PGSQL_LOGIN_FIELD");
		if (!login_field) login_field = "id";

		home_field = read_env("PGSQL_HOME_FIELD");
		if (!home_field) home_field = "home";

		maildir_field=read_env("PGSQL_MAILDIR_FIELD");
		if (!maildir_field) maildir_field="''";

		options_field=read_env("PGSQL_AUXOPTIONS_FIELD");
		if (!options_field) options_field="''";

		where_clause=read_env("PGSQL_WHERE_CLAUSE");
		if (!where_clause) where_clause = "";

#define DEFAULT_ENUMERATE_QUERY \
		"SELECT %s, %s, %s, %s, %s, %s FROM %s %s%s",\
			login_field, uid_field, gid_field,		\
			home_field, maildir_field,			\
			options_field, user_table,			\
			*where_clause ? " WHERE ":"",			\
			where_clause


		query_len=snprintf(dummy_buf, 1, DEFAULT_ENUMERATE_QUERY);

		querybuf=malloc(query_len+1);

		if (!querybuf)
		{
			perror("malloc");
			return;
		}

		snprintf(querybuf, query_len+1, DEFAULT_ENUMERATE_QUERY);
	}
	else
	{
		/* tom@minnesota.com */
		querybuf=parse_select_clause (select_clause, "*",
					      defdomain, "enumerate");
		if (!querybuf)
		{
			DPRINTF("authpgsql: parse_select_clause failed");
			return;
		}
	}
	DPRINTF("authpgsql: enumerate query: %s", querybuf);

    	if (PQsendQuery(pgconn, querybuf) == 0)
    	{
		DPRINTF("PQsendQuery failed, reconnecting: %s",PQerrorMessage(pgconn));

		auth_pgsql_cleanup();

		if (do_connect())
		{
			free(querybuf);
			return;
		}

    		if (PQsendQuery(pgconn, querybuf) == 0)
		{
			DPRINTF("PQsendQuery failed second time, giving up: %s",PQerrorMessage(pgconn));
			free(querybuf);
			auth_pgsql_cleanup();
			return;
		}
	}
	free(querybuf);

	while ((pgresult = PQgetResult(pgconn)) != NULL)
	{
		if (PQresultStatus(pgresult) != PGRES_TUPLES_OK)
		{
			DPRINTF("pgsql error during enumeration: %s",PQerrorMessage(pgconn));
			PQclear(pgresult);
			return;
		}

		for (n=PQntuples(pgresult), i=0; i<n; i++)
		{
			const char *username;
			uid_t uid;
			gid_t gid;
			const char *homedir;
			const char *maildir;
			const char *options;

			username=PQgetvalue(pgresult,i,0);
			uid=atol(PQgetvalue(pgresult,i,1));
			gid=atol(PQgetvalue(pgresult,i,2));
			homedir=PQgetvalue(pgresult,i,3);
			maildir=PQgetvalue(pgresult,i,4);
			options=PQgetvalue(pgresult,i,5);

			if (!username || !*username || !homedir || !*homedir)
				continue;

			if (maildir && !*maildir)
				maildir=NULL;

			(*cb_func)(username, uid, gid, homedir,
				   maildir, options, void_arg);

		}
		PQclear(pgresult);
	}
	/* FIXME: is it possible that a NULL result from PQgetResult could
	   indicate an error rather than EOF? The documentation is not clear */
	(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
}
