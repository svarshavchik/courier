/*
** Copyright 2000-2010 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<mysql.h>
#include	<time.h>

#include	"authmysql.h"
#include	"authmysqlrc.h"
#include	"auth.h"
#include	"courierauthdebug.h"

#define err courier_auth_err

static const char *read_env(const char *env)
{
	return authgetconfig(AUTHMYSQLRC, env);
}

static MYSQL mysql_buf;

static MYSQL *mysql=0;

static char *escape_str(const char *c, size_t n)
{
	char *buf=malloc(n*2+1);

	if (!buf)
	{
		perror("malloc");
		return NULL;
	}

	mysql_real_escape_string(mysql, buf, c, n);
	return buf;
}

static void set_session_options(void)
/*
* session variables can be set once for the whole session
*/
{
/* Anton Dobkin <anton@viansib.ru>, VIAN, Ltd. */
#if MYSQL_VERSION_ID >= 41000    
	const char *character_set=read_env("MYSQL_CHARACTER_SET"), *check;
    
        if(character_set){

            /*
            * This function works like the SET NAMES statement, but also sets
            * the value of mysql->charset, and thus affects the character set
            * used by mysql_real_escape_string()
            *
            * (return value apparently work the opposite of what is documented)
            */
            mysql_set_character_set(mysql, character_set);
            check = mysql_character_set_name(mysql);
            if (strcmp(character_set, check) != 0)
            {
                err("Cannot set MySQL character set \"%s\", working with \"%s\"\n",
                    character_set, check);
            }
            else
            {
                DPRINTF("Install of a character set for MySQL: %s", character_set);
            }
        }
#endif /* 41000 */
}

static int do_connect()
{
const	char *server;
const	char *userid;
const	char *password;
const	char *database;
const	char *server_socket=0;
unsigned int server_port=0;
unsigned int server_opt=0;
const	char *p;

const	char *sslkey;
const	char *sslcert;
const	char *sslcacert;
const	char *sslcapath;
const	char *sslcipher;
unsigned int  use_ssl=0;

/*
** Periodically detect dead connections.
*/
	if (mysql)
	{
		static time_t last_time=0;
		time_t t_check;

		time(&t_check);

		if (t_check < last_time)
			last_time=t_check;	/* System clock changed */

		if (t_check < last_time + 60)
			return (0);

		last_time=t_check;
			
		if (mysql_ping(mysql) == 0) return (0);

		DPRINTF("authmysqllib: mysql_ping failed, connection lost");
		mysql_close(mysql);
		mysql=0;
	}

	server=read_env("MYSQL_SERVER");
	userid=read_env("MYSQL_USERNAME");
	password=read_env("MYSQL_PASSWORD");
	database=read_env("MYSQL_DATABASE");

#if MYSQL_VERSION_ID >= 32200
	sslkey=read_env("MYSQL_SSL_KEY");
	sslcert=read_env("MYSQL_SSL_CERT");
	sslcacert=read_env("MYSQL_SSL_CACERT");
	sslcapath=read_env("MYSQL_SSL_CAPATH");
	sslcipher=read_env("MYSQL_SSL_CIPHER");

	if ((sslcert != NULL) && ((sslcacert != NULL) || (sslcapath != NULL)))
	{
		use_ssl=1;
	}
#endif

	server_socket=(char *) read_env("MYSQL_SOCKET");

	if ((p=read_env("MYSQL_PORT")) != 0)
	{
		server_port=(unsigned int) atoi(p);
	}

	if ((p=read_env("MYSQL_OPT")) != 0)
	{
		server_opt=(unsigned int) atol(p);
	}

	if (!server && !server_socket)
	{
		err("authmysql: MYSQL_SERVER nor MYSQL_SOCKET set in"
			AUTHMYSQLRC ".");
		return (-1);
	}

	if (!userid)
	{
		err("authmysql: MYSQL_USERNAME not set in "
			AUTHMYSQLRC ".");
		return (-1);
	}

	if (!database)
	{
		err("authmysql: MYSQL_DATABASE not set in "
			AUTHMYSQLRC ".");
		return (-1);
	}

#if MYSQL_VERSION_ID >= 32200
	mysql_init(&mysql_buf);
	if (use_ssl)
	{
		mysql_ssl_set(&mysql_buf, sslkey, sslcert, sslcacert,
			      sslcapath, sslcipher);
	}
	mysql=mysql_real_connect(&mysql_buf, server, userid, password,
				 NULL,
				 server_port,
				 server_socket,
				 server_opt);
#else
	mysql=mysql_connect(&mysql_buf, server, userid, password);
#endif
	if (!mysql)
	{
		err("failed to connect to mysql server (server=%s, userid=%s): %s",
			server ? server : "<null>",
			userid ? userid : "<null>",
			mysql_error(&mysql_buf));
		return (-1);
	}

	if (mysql_select_db(mysql, database))
	{
		err("authmysql: mysql_select_db(%s) error: %s",
			database, mysql_error(mysql));
		mysql_close(mysql);
		mysql=0;
		return (-1);
	}
	
	DPRINTF("authmysqllib: connected. Versions: "
		"header %lu, "
		"client %lu, "
		"server %lu",
		(long)MYSQL_VERSION_ID,
		mysql_get_client_version(),
		mysql_get_server_version(mysql));

	set_session_options();	
	return (0);
}

void auth_mysql_cleanup()
{
	if (mysql)
	{
		mysql_close(mysql);
		mysql=0;
	}
}

static struct authmysqluserinfo ui={0, 0, 0, 0, 0, 0, 0, 0};

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

struct authmysqluserinfo *auth_mysql_getuserinfo(const char *username,
						 const char *service)
{
const char *defdomain	=NULL;
char	*querybuf;
MYSQL_ROW	row;
MYSQL_RES	*result;
int		num_fields;
char		*endp;

const char	*select_clause; /* siefca@pld.org.pl */

#define DEFAULT_SELECT_QUERY "SELECT %s, %s, %s, %s, %s, %s, %s, %s, %s, %s FROM %s WHERE %s = '%s%s%s' %s%s%s", \
		login_field, crypt_field, clear_field, uid_field,\
		gid_field, home_field, maildir_field, quota_field,\
		name_field, options_field, user_table, login_field,\
		username_escaped,\
		has_domain || !*defdomain ? "":"@", has_domain ? "":defdomain, \
		*where_clause ? " AND (":"", where_clause,\
		*where_clause ? ")":""

	if (do_connect())	return (0);

	initui();

	select_clause=read_env("MYSQL_SELECT_CLAUSE");
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

		user_table=read_env("MYSQL_USER_TABLE");

		if (!user_table)
		{
			err("authmysql: MYSQL_USER_TABLE not set in "
				AUTHMYSQLRC ".");
			return (0);
		}

		crypt_field=read_env("MYSQL_CRYPT_PWFIELD");
		clear_field=read_env("MYSQL_CLEAR_PWFIELD");
		name_field=read_env("MYSQL_NAME_FIELD");

		if (!crypt_field && !clear_field)
		{
			err("authmysql: MYSQL_CRYPT_PWFIELD and "
				"MYSQL_CLEAR_PWFIELD not set in " AUTHMYSQLRC ".");
			return (0);
		}
		if (!crypt_field) crypt_field="\"\"";
		if (!clear_field) clear_field="\"\"";
		if (!name_field) name_field="\"\"";

		uid_field = read_env("MYSQL_UID_FIELD");
		if (!uid_field) uid_field = "uid";

		gid_field = read_env("MYSQL_GID_FIELD");
		if (!gid_field) gid_field = "gid";

		login_field = read_env("MYSQL_LOGIN_FIELD");
		if (!login_field) login_field = "id";

		home_field = read_env("MYSQL_HOME_FIELD");
		if (!home_field) home_field = "home";

		maildir_field=read_env(service && strcmp(service, "courier")==0
				       ? "MYSQL_DEFAULTDELIVERY"
				       : "MYSQL_MAILDIR_FIELD");
		if (!maildir_field) maildir_field="\"\"";

		quota_field=read_env("MYSQL_QUOTA_FIELD");
		if (!quota_field) quota_field="\"\""; 

		options_field=read_env("MYSQL_AUXOPTIONS_FIELD");
		if (!options_field) options_field="\"\"";

		where_clause=read_env("MYSQL_WHERE_CLAUSE");
		if (!where_clause) where_clause = "";

		username_escaped=malloc(strlen(username)*2+1);

		if (!username_escaped)
		{
			perror("malloc");
			return (0);
		}

		mysql_real_escape_string(mysql, username_escaped,
					 username, strlen(username));

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
	if (mysql_query (mysql, querybuf))
	{
		/* <o.blasnik@nextra.de> */

		DPRINTF("mysql_query failed, reconnecting: %s", mysql_error(mysql));
		auth_mysql_cleanup();

		if (do_connect())
		{
			free(querybuf);
			return (0);
		}

		if (mysql_query (mysql, querybuf))
		{
			DPRINTF("mysql_query failed second time, giving up: %s", mysql_error(mysql));
			free(querybuf);
			auth_mysql_cleanup();
			/* Server went down, that's OK,
			** try again next time.
			*/
			return (0);
		}
	}
	free(querybuf);

	result = mysql_store_result (mysql);       
	if (result)
	{
		if (mysql_num_rows(result))
		{
			row = mysql_fetch_row (result);
			num_fields = mysql_num_fields (result);

			if (num_fields < 6)
			{
				DPRINTF("incomplete row, only %d fields returned",
					num_fields);
				mysql_free_result(result);
				return(0);
			}

			if (row[0] && row[0][0])
				ui.username=strdup(row[0]);
			if (row[1] && row[1][0])
				ui.cryptpw=strdup(row[1]);
			if (row[2] && row[2][0])
				ui.clearpw=strdup(row[2]);
			/* perhaps authmysql needs a glob_uid/glob_gid feature
			   like authldap? */
			if (!row[3] || !row[3][0] ||
			   (ui.uid=strtol(row[3], &endp, 10), endp[0] != '\0'))
			{
				DPRINTF("invalid value for uid: '%s'",
					row[3] ? row[3] : "<null>");
				mysql_free_result(result);
				return 0;
			}
			if (!row[4] || !row[4][0] ||
			   (ui.gid=strtol(row[4], &endp, 10), endp[0] != '\0'))
			{
				DPRINTF("invalid value for gid: '%s'",
					row[4] ? row[4] : "<null>");
				mysql_free_result(result);
				return 0;
			}
			if (row[5] && row[5][0])
				ui.home=strdup(row[5]);
			else
			{
				DPRINTF("required value for 'home' (column 6) is missing");
				mysql_free_result(result);
				return(0);
			}
			if (num_fields > 6 && row[6] && row[6][0])
				ui.maildir=strdup(row[6]);
			if (num_fields > 7 && row[7] && row[7][0])
				ui.quota=strdup(row[7]);
			if (num_fields > 8 && row[8] && row[8][0])
				ui.fullname=strdup(row[8]);
			if (num_fields > 9 && row[9] && row[9][0])
				ui.options=strdup(row[9]);
		}
		else
		{
			DPRINTF("zero rows returned");
			mysql_free_result(result);
			return (&ui); /* User not found */
		}
	}
	else
	{
		DPRINTF("mysql_store_result failed");
		return (0);
	}
	mysql_free_result(result);
	return (&ui);
}

int auth_mysql_setpass(const char *user, const char *pass,
		       const char *oldpass)
{
	char *newpass_crypt;
	char *sql_buf;
	int rc=0;

	char *clear_escaped;
	char *crypt_escaped;

	const char  *clear_field	=NULL,
		    *crypt_field	=NULL,
		    *defdomain		=NULL,
		    *where_clause	=NULL,
		    *user_table		=NULL,
		    *login_field	=NULL,
		    *chpass_clause	=NULL; /* siefca@pld.org.pl */

	if (do_connect())	return (-1);

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

	mysql_real_escape_string(mysql, clear_escaped, pass, strlen(pass));
	mysql_real_escape_string(mysql, crypt_escaped,
				 newpass_crypt, strlen(newpass_crypt));

	/* siefca@pld.org.pl */
	chpass_clause=read_env("MYSQL_CHPASS_CLAUSE");
	defdomain=read_env("DEFAULT_DOMAIN");
	user_table=read_env("MYSQL_USER_TABLE");
	if (!chpass_clause)
	{
		int has_domain=strchr(user, '@') != NULL;
		char *username_escaped;
		char dummy_buf[1];
		size_t sql_buf_size;

		username_escaped=malloc(strlen(user)*2+1);

		if (!username_escaped)
		{
			perror("malloc");
			free(clear_escaped);
			free(crypt_escaped);
			free(newpass_crypt);
			return -1;
		}

		mysql_real_escape_string(mysql, username_escaped,
					 user, strlen(user));

		login_field = read_env("MYSQL_LOGIN_FIELD");
		if (!login_field) login_field = "id";
		crypt_field=read_env("MYSQL_CRYPT_PWFIELD");
		clear_field=read_env("MYSQL_CLEAR_PWFIELD");
		where_clause=read_env("MYSQL_WHERE_CLAUSE");

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
		sql_buf=auth_parse_chpass_clause(escape_str,
						 chpass_clause,
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
	if (mysql_query (mysql, sql_buf))
	{
		DPRINTF("setpass SQL failed");
		rc= -1;
		auth_mysql_cleanup();
	}
	free(sql_buf);
	return (rc);
}

void auth_mysql_enumerate( void(*cb_func)(const char *name,
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
	MYSQL_ROW	row;
	MYSQL_RES	*result;

	if (do_connect())
	{
		(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
		return;
	}

	initui();

	select_clause=read_env("MYSQL_ENUMERATE_CLAUSE");
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

		user_table=read_env("MYSQL_USER_TABLE");

		if (!user_table)
		{
			err("authmysql: MYSQL_USER_TABLE not set in "
				AUTHMYSQLRC ".");
			return;
		}

		uid_field = read_env("MYSQL_UID_FIELD");
		if (!uid_field) uid_field = "uid";

		gid_field = read_env("MYSQL_GID_FIELD");
		if (!gid_field) gid_field = "gid";

		login_field = read_env("MYSQL_LOGIN_FIELD");
		if (!login_field) login_field = "id";

		home_field = read_env("MYSQL_HOME_FIELD");
		if (!home_field) home_field = "home";

		maildir_field=read_env("MYSQL_MAILDIR_FIELD");
		if (!maildir_field) maildir_field="\"\"";

		options_field=read_env("MYSQL_AUXOPTIONS_FIELD");
		if (!options_field) options_field="\"\"";

		where_clause=read_env("MYSQL_WHERE_CLAUSE");
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
			DPRINTF("authmysql: parse_select_clause failed");
			return;
		}
	}
	DPRINTF("authmysql: enumerate query: %s", querybuf);

	if (mysql_query (mysql, querybuf))
	{
		DPRINTF("mysql_query failed, reconnecting: %s", mysql_error(mysql));
		/* <o.blasnik@nextra.de> */

		auth_mysql_cleanup();

		if (do_connect())
		{
			free(querybuf);
			return;
		}

		if (mysql_query (mysql, querybuf))
		{
			DPRINTF("mysql_query failed second time, giving up: %s", mysql_error(mysql));
			free(querybuf);
			auth_mysql_cleanup();
			return;
		}
	}
	free(querybuf);

	result = mysql_use_result (mysql);
	if (result)
	{
		const char *username;
		uid_t uid;
		gid_t gid;
		const char *homedir;
		const char *maildir;
		const char *options;

		while ((row = mysql_fetch_row (result)) != NULL)
		{
			if(!row[0] || !row[0][0]
			   || !row[1] || !row[1][0]
			   || !row[2] || !row[2][0]
			   || !row[3] || !row[3][0])
			{
				continue;
			}

			username=row[0];
			uid=atol(row[1]); /* FIXME use strtol to validate */
			gid=atol(row[2]);
			homedir=row[3];
			maildir=row[4];
			options=row[5];

			if (maildir && !*maildir)
				maildir=NULL;

			(*cb_func)(username, uid, gid, homedir,
				   maildir, options, void_arg);
		}
	}
	/* NULL row could indicate end of result or an error */
	if (mysql_errno(mysql))
	{
		DPRINTF("mysql error during enumeration: %s", mysql_error(mysql));
	}
	else
		(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);

	if (result) mysql_free_result(result);
}
