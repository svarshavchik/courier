/*
** Copyright 2012 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<time.h>

#include	"authsqlite.h"
#include	"authsqliterc.h"
#include	"auth.h"
#include	"courierauthdebug.h"

#define err courier_auth_err
#define GET(c) ((c) < (n) && columns[(c)] ? columns[(c)]:"")

static const char *read_env(const char *env)
{
	return authgetconfig(AUTHSQLITERC, env);
}

static sqlite3 *dbh=0;

sqlite3 *do_connect()
{
	const char *p;

	if (dbh)
		return dbh;

	p=read_env("SQLITE_DATABASE");

	if (!p)
		return 0;

	if (access(p, R_OK))
		return 0;

	if (sqlite3_open_v2(p, &dbh, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
	{
		if (dbh)
		{
			err("sqllite3_open(%s): %s", p, sqlite3_errmsg(dbh));
			sqlite3_close(dbh);
			dbh=0;
		}
		return 0;
	}

	return dbh;
}

void auth_sqlite_cleanup()
{
	if (dbh)
	{
		sqlite3_close(dbh);
		dbh=0;
	}
}

static char *escape_str(const char *c, size_t n)
{
	char *p=malloc(n+1), *q;

	if (!p)
	{
		perror("malloc");
		return 0;
	}

	memcpy(p, c, n);
	p[n]=0;

	q=sqlite3_mprintf("%q", p);
	free(p);

	p=strdup(q);
	sqlite3_free(q);
	if (!p)
	{
		perror("malloc");
		return 0;
	}
	return p;
}

static struct authsqliteuserinfo ui={0, 0, 0, 0, 0, 0, 0, 0};
static int ui_cnt;

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

static int select_callback(void *dummy, int n, char **columns, char **names)
{
	if (ui_cnt++)
	{
		err("Multiple rows returned");
		return -1;
	}

	initui();

	if (n < 6)
	{
		err("Query came back with fewer than 6 columns");
		return -1;
	}

	if ((ui.username=strdup(GET(0))) == NULL
	    || (ui.cryptpw=strdup(GET(1))) == NULL
	    || (ui.clearpw=strdup(GET(2))) == NULL
	    || (ui.home=strdup(GET(5))) == NULL
	    || (ui.maildir=strdup(GET(6))) == NULL
	    || (ui.quota=strdup(GET(7))) == NULL
	    || (ui.fullname=strdup(GET(8))) == NULL
	    || (ui.options=strdup(GET(9))) == NULL)
	{
		initui();
		return 0;
	}

	ui.uid=atoi(GET(3));
	ui.gid=atoi(GET(4));

	return 0;
}

struct authsqliteuserinfo *auth_sqlite_getuserinfo(const char *username,
						   const char *service)
{
	const char *defdomain	=NULL;
	char	*querybuf;
	char		*errmsg;
	const char	*select_clause;

#define DEFAULT_SELECT_QUERY "SELECT %s, %s, %s, %s, %s, %s, %s, %s, %s, %s FROM %s WHERE %s = '%s%s%s' %s%s%s", \
		login_field, crypt_field, clear_field, uid_field,\
		gid_field, home_field, maildir_field, quota_field,\
		name_field, options_field, user_table, login_field,\
		username_escaped,\
		has_domain || !*defdomain ? "":"@", has_domain ? "":defdomain, \
		*where_clause ? " AND (":"", where_clause,\
		*where_clause ? ")":""

	initui();

	if (!do_connect())
		return &ui;

	select_clause=read_env("SQLITE_SELECT_CLAUSE");
	defdomain=read_env("DEFAULT_DOMAIN");	
	if (!defdomain)	defdomain="";
	
	if (!select_clause) /* siefca@pld.org.pl */
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
		char		*username_escaped;
		size_t	query_size;
		char dummy_buf[1];
		int has_domain;

		user_table=read_env("SQLITE_USER_TABLE");

		if (!user_table)
		{
			err("authsqlite: SQLITE_USER_TABLE not set in "
				AUTHSQLITERC ".");
			return (0);
		}

		crypt_field=read_env("SQLITE_CRYPT_PWFIELD");
		clear_field=read_env("SQLITE_CLEAR_PWFIELD");
		name_field=read_env("SQLITE_NAME_FIELD");

		if (!crypt_field && !clear_field)
		{
			err("authsqlite: SQLITE_CRYPT_PWFIELD and "
				"SQLITE_CLEAR_PWFIELD not set in " AUTHSQLITERC ".");
			return (0);
		}
		if (!crypt_field) crypt_field="\"\"";
		if (!clear_field) clear_field="\"\"";
		if (!name_field) name_field="\"\"";

		uid_field = read_env("SQLITE_UID_FIELD");
		if (!uid_field) uid_field = "uid";

		gid_field = read_env("SQLITE_GID_FIELD");
		if (!gid_field) gid_field = "gid";

		login_field = read_env("SQLITE_LOGIN_FIELD");
		if (!login_field) login_field = "id";

		home_field = read_env("SQLITE_HOME_FIELD");
		if (!home_field) home_field = "home";

		maildir_field=read_env(service && strcmp(service, "courier")==0
				       ? "SQLITE_DEFAULTDELIVERY"
				       : "SQLITE_MAILDIR_FIELD");
		if (!maildir_field) maildir_field="\"\"";

		quota_field=read_env("SQLITE_QUOTA_FIELD");
		if (!quota_field) quota_field="\"\""; 

		options_field=read_env("SQLITE_AUXOPTIONS_FIELD");
		if (!options_field) options_field="\"\"";

		where_clause=read_env("SQLITE_WHERE_CLAUSE");
		if (!where_clause) where_clause = "";

		username_escaped=escape_str(username, strlen(username));

		if (!username_escaped)
		{
			perror("malloc");
			return (0);
		}

		has_domain=strchr(username, '@') != NULL;

		query_size=snprintf(dummy_buf, 1, DEFAULT_SELECT_QUERY);

		querybuf=malloc(query_size+1);

		if (!querybuf)
		{
			free(username_escaped);
			perror("malloc");
			return(0);
		}

		snprintf(querybuf, query_size+1, DEFAULT_SELECT_QUERY);
		free(username_escaped);

	}
	else
	{
		/* siefca@pld.org.pl */
		querybuf=auth_parse_select_clause (escape_str,
						   select_clause, username,
						   defdomain, service);
		if (!querybuf)
		{
			DPRINTF("parse_select_clause failed (DEFAULT_DOMAIN not set?)");
			return 0;
		}
	}

	DPRINTF("SQL query: %s", querybuf);
	errmsg=0;
	ui_cnt=0;
	if (sqlite3_exec(dbh, querybuf, select_callback,
			 NULL, &errmsg) != SQLITE_OK)
	{
		if (errmsg)
		{
			err(errmsg);
			sqlite3_free(errmsg);
		}

		free(querybuf);
		return 0;
	}

	free(querybuf);
	if (errmsg)
	{
		err(errmsg);
		sqlite3_free(errmsg);
	}

	return &ui;
}

static int dummy_callback(void *dummy, int n, char **columns, char **names)
{
	return 0;
}

int auth_sqlite_setpass(const char *user, const char *pass,
		       const char *oldpass)
{
	char *newpass_crypt;
	char *sql_buf;
	int rc=0;
	char *errmsg;
	char *clear_escaped;
	char *crypt_escaped;

	const char  *clear_field	=NULL,
		    *crypt_field	=NULL,
		    *defdomain		=NULL,
		    *where_clause	=NULL,
		    *user_table		=NULL,
		    *login_field	=NULL,
		    *chpass_clause	=NULL; /* siefca@pld.org.pl */

	if (!do_connect())	return (-1);

	if (!(newpass_crypt=authcryptpasswd(pass, oldpass)))
		return (-1);

	clear_escaped=escape_str(pass, strlen(pass));

	if (!clear_escaped)
	{
		perror("malloc");
		free(newpass_crypt);
		return -1;
	}

	crypt_escaped=escape_str(newpass_crypt, strlen(newpass_crypt));

	if (!crypt_escaped)
	{
		perror("malloc");
		free(clear_escaped);
		free(newpass_crypt);
		return -1;
	}

	/* siefca@pld.org.pl */
	chpass_clause=read_env("SQLITE_CHPASS_CLAUSE");
	defdomain=read_env("DEFAULT_DOMAIN");
	user_table=read_env("SQLITE_USER_TABLE");
	if (!chpass_clause)
	{
		int has_domain=strchr(user, '@') != NULL;
		char *username_escaped;
		char dummy_buf[1];
		size_t sql_buf_size;

		username_escaped=escape_str(user, strlen(user));

		if (!username_escaped)
		{
			perror("malloc");
			free(clear_escaped);
			free(crypt_escaped);
			free(newpass_crypt);
			return -1;
		}

		login_field = read_env("SQLITE_LOGIN_FIELD");
		if (!login_field) login_field = "id";
		crypt_field=read_env("SQLITE_CRYPT_PWFIELD");
		clear_field=read_env("SQLITE_CLEAR_PWFIELD");
		where_clause=read_env("SQLITE_WHERE_CLAUSE");

		if (!where_clause)
			where_clause="";

		if (!crypt_field)
			crypt_field="";

		if (!clear_field)
			clear_field="";

		if (!defdomain)
			defdomain="";

#define DEFAULT_SETPASS_UPDATE \
		"UPDATE %s SET %s%s%s%s %s %s%s%s%s WHERE %s='%s%s%s' %s%s%s", \
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
			login_field,					\
			username_escaped,				\
			has_domain || !*defdomain ? "":"@",		\
			has_domain ? "":defdomain,			\
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
		sql_buf=auth_parse_chpass_clause(escape_str, chpass_clause,
						 user,
						 defdomain,
						 clear_escaped,
						 crypt_escaped);
	}
	
	free(clear_escaped);
	free(crypt_escaped);
	free(newpass_crypt);

	if (courier_authdebug_login_level >= 2)
	{
		DPRINTF("setpass SQL: %s", sql_buf);
	}

	errmsg=NULL;

	if (sqlite3_exec(dbh, sql_buf, dummy_callback,
			 NULL, &errmsg) != SQLITE_OK)
	{
		rc=-1;
	}
	else
	{
		if (sqlite3_changes(dbh) > 0)
		{
			DPRINTF("authsqllite: password updated");
		}
		else
		{
			rc=-1;
			DPRINTF("authsqllite: password not updated");
		}
	}

	if (errmsg)
	{
		err(errmsg);
		sqlite3_free(errmsg);
	}

	free(sql_buf);
	return (rc);
}

struct enumerate_user_cb {

	void (*cb_func)(const char *name,
			uid_t uid,
			gid_t gid,
			const char *homedir,
			const char *maildir,
			const char *options,
			void *void_arg);
	void *void_arg;
};

static int enumerate_callback(void *closure,
			      int n, char **columns, char **names)
{
	struct enumerate_user_cb *cb=(struct enumerate_user_cb *)closure;

	const char *username;
	uid_t uid;
	gid_t gid;
	const char *homedir;
	const char *maildir;
	const char *options;

	username=GET(0);
	uid=atol(GET(1)); /* FIXME use strtol to validate */
	gid=atol(GET(2));
	homedir=GET(3);
	maildir=GET(4);
	options=GET(5);

	if (maildir && !*maildir)
		maildir=NULL;

	if (options && !*options)
		options=NULL;

	(*cb->cb_func)(username, uid, gid, homedir,
		       maildir, options, cb->void_arg);
	return 0;
}

void auth_sqlite_enumerate( void(*cb_func)(const char *name,
					   uid_t uid,
					   gid_t gid,
					   const char *homedir,
					   const char *maildir,
					   const char *options,
					   void *void_arg),
			    void *void_arg)
{
	const char *defdomain, *select_clause;
	char	*querybuf;
	char    *errmsg;
	struct enumerate_user_cb cb;

	cb.cb_func=cb_func;
	cb.void_arg=void_arg;

	if (!do_connect())	return;

	initui();

	select_clause=read_env("SQLITE_ENUMERATE_CLAUSE");
	defdomain=read_env("DEFAULT_DOMAIN");
	if (!defdomain || !defdomain[0])
		defdomain="*"; /* otherwise parse_select_clause fails */
	
	if (!select_clause)
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

		user_table=read_env("SQLITE_USER_TABLE");

		if (!user_table)
		{
			err("authsqlite: SQLITE_USER_TABLE not set in "
				AUTHSQLITERC ".");
			return;
		}

		uid_field = read_env("SQLITE_UID_FIELD");
		if (!uid_field) uid_field = "uid";

		gid_field = read_env("SQLITE_GID_FIELD");
		if (!gid_field) gid_field = "gid";

		login_field = read_env("SQLITE_LOGIN_FIELD");
		if (!login_field) login_field = "id";

		home_field = read_env("SQLITE_HOME_FIELD");
		if (!home_field) home_field = "home";

		maildir_field=read_env("SQLITE_MAILDIR_FIELD");
		if (!maildir_field) maildir_field="\"\"";

		options_field=read_env("SQLITE_AUXOPTIONS_FIELD");
		if (!options_field) options_field="\"\"";

		where_clause=read_env("SQLITE_WHERE_CLAUSE");
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
		/* siefca@pld.org.pl */
		querybuf=auth_parse_select_clause (escape_str,
						   select_clause, "*",
						   defdomain, "enumerate");
		if (!querybuf)
		{
			DPRINTF("authsqlite: parse_select_clause failed");
			return;
		}
	}
	DPRINTF("authsqlite: enumerate query: %s", querybuf);

	errmsg=NULL;

	sqlite3_exec(dbh, querybuf, enumerate_callback, &cb, &errmsg);

	if (errmsg)
	{
		err(errmsg);
		sqlite3_free(errmsg);
	}
	free(querybuf);

	(*cb.cb_func)(NULL, 0, 0, NULL, NULL, NULL, cb.void_arg);
}
