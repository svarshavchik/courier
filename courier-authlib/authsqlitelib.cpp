/*
** Copyright 2016 Double Precision, Inc.  See COPYING for
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

extern "C" {
#include	"authsqliterc.h"
#include	"auth.h"
#include	"courierauthdebug.h"
}

#include <string>
#include <sstream>
#include "authsqlite.h"
#include "authconfigfile.h"

#define err courier_auth_err

#define GET(c) ((c) < (n) && columns[(c)] ? columns[(c)]:"")

class authsqlite_connection {

public:
	sqlite3 *dbh;

	class authsqliterc_vars {
	public:
		std::string database;
		std::string select_clause, defdomain;
		std::string user_table, crypt_field, clear_field,
			name_field, uid_field, gid_field, login_field,
			home_field, maildir_field, defaultdelivery_field,
			quota_field, options_field,
			where_clause, chpass_clause, enumerate_clause;

	};

	class authsqliterc_file : public courier::auth::config_file,
				  public authsqliterc_vars {

		authsqlite_connection &conn;

	public:

		authsqliterc_file &operator=(const authsqliterc_file &o);
		authsqliterc_file(authsqlite_connection &connArg);
		~authsqliterc_file();

		bool do_load();
		void do_reload();
	};

	authsqliterc_file config_file;

	authsqlite_connection() : dbh(NULL), config_file(*this)
	{
	}

	~authsqlite_connection()
	{
		disconnect();
	}

	void disconnect()
	{
		if (dbh)
		{
			sqlite3_close(dbh);
			dbh=NULL;
		}
	}

	std::string escape(const std::string &s);

	sqlite3 *do_connect();

	static authsqlite_connection *singleton;

	static authsqlite_connection *connect();

	bool getuserinfo(const char *username,
			 const char *service,
			 authsqliteuserinfo &ui);

	void enumerate( void(*cb_func)(const char *name,
				       uid_t uid,
				       gid_t gid,
				       const char *homedir,
				       const char *maildir,
				       const char *options,
						      void *void_arg),
			void *void_arg);

	bool setpass(const char *user, const char *pass,
		     const char *oldpass);
};


authsqlite_connection::authsqliterc_file
&authsqlite_connection::authsqliterc_file
::operator=(const authsqlite_connection::authsqliterc_file &o)
{
	courier::auth::config_file::operator=(o);
	authsqliterc_vars::operator=(o);
	return *this;
}

authsqlite_connection::authsqliterc_file
::authsqliterc_file(authsqlite_connection &connArg)
	: courier::auth::config_file(AUTHSQLITERC),
	  conn(connArg)
{
}

authsqlite_connection::authsqliterc_file::~authsqliterc_file()
{
}

bool authsqlite_connection::authsqliterc_file::do_load()
{
	if (!config("SQLITE_DATABASE", database, true))
		return false;

	defdomain=config("DEFAULT_DOMAIN");

	select_clause=config("SQLITE_SELECT_CLAUSE");
	chpass_clause=config("SQLITE_CHPASS_CLAUSE");
	enumerate_clause=config("SQLITE_ENUMERATE_CLAUSE");

	if (select_clause.empty() || chpass_clause.empty() ||
	    enumerate_clause.empty())
	{
		if (!config("SQLITE_USER_TABLE", user_table,
			    true))
			return false;

		crypt_field=config("SQLITE_CRYPT_PWFIELD", "''");

		clear_field=config("SQLITE_CLEAR_PWFIELD", "''");

		if (crypt_field + clear_field == "''''")
		{
			err("SQLITE_CLEAR_PWFIELD and SQLITE_CLEAR_FIELD not set in " AUTHSQLITERC);
			return false;
		}

		name_field=config("SQLITE_NAME_FIELD", "''");

		uid_field=config("SQLITE_UID_FIELD", "uid");
		gid_field=config("SQLITE_GID_FIELD", "gid");
		login_field=config("SQLITE_LOGIN_FIELD", "id");
		home_field=config("SQLITE_HOME_FIELD", "home");

		maildir_field=config("SQLITE_MAILDIR_FIELD", "''");
		defaultdelivery_field=config("SQLITE_DEFAULTDELIVERY", "''");

		quota_field=config("SQLITE_QUOTA_FIELD", "''");
		options_field=config("SQLITE_AUXOPTIONS_FIELD", "''");
		where_clause=config("SQLITE_WHERE_CLAUSE", "1=1");
	}

	return true;
}

void authsqlite_connection::authsqliterc_file::do_reload()
{
	authsqliterc_file new_file(conn);

	if (new_file.load(true))
	{
		*this=new_file;
		DPRINTF("authsqlite: reloaded %s", filename);

		// Disconnect from the server, new login
		// parameters, perhaps.

		conn.disconnect();
	}
}

authsqlite_connection *authsqlite_connection::singleton=NULL;

authsqlite_connection *authsqlite_connection::connect()
{
	if (authsqlite_connection::singleton)
	{
		authsqlite_connection::singleton->config_file.load(true);
		return authsqlite_connection::singleton;
	}

	authsqlite_connection *new_conn=new authsqlite_connection;

	if (new_conn->config_file.load())
	{
		authsqlite_connection::singleton=new_conn;
		return new_conn;
	}

	delete new_conn;
	return NULL;
}

sqlite3 *authsqlite_connection::do_connect()
{
	const char *p;

	if (dbh)
		return dbh;

	p=config_file.database.c_str();

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

static void do_auth_sqlite_cleanup()
{
	if (authsqlite_connection::singleton)
	{
		delete authsqlite_connection::singleton;
		authsqlite_connection::singleton=NULL;
	}
}

std::string authsqlite_connection::escape(const std::string &s)
{
	char *q=sqlite3_mprintf("%q", s.c_str());

	std::string r(q);
	sqlite3_free(q);
	return r;
}


class select_callback_info {
public:
	authsqliteuserinfo &ui;
	int ui_cnt;

	select_callback_info(authsqliteuserinfo &uiArg)
		: ui(uiArg), ui_cnt(0)
	{
	}
};

static int select_callback(void *dummy, int n, char **columns, char **names)
{
	select_callback_info *cb=
		reinterpret_cast<select_callback_info *>(dummy);

	if (cb->ui_cnt++)
	{
		err("Multiple rows returned");
		return -1;
	}

	if (n < 6)
	{
		err("Query came back with fewer than 6 columns");
		return -1;
	}

	cb->ui.username=GET(0);
	cb->ui.cryptpw=GET(1);
	cb->ui.clearpw=GET(2);
	cb->ui.home=GET(5);
	cb->ui.maildir=GET(6);
	cb->ui.quota=GET(7);
	cb->ui.fullname=GET(8);
	cb->ui.options=GET(9);

	std::istringstream(GET(3)) >> cb->ui.uid;
	std::istringstream(GET(4)) >> cb->ui.gid;
	return 0;
}

bool authsqlite_connection::getuserinfo(const char *username,
					const char *service,
					authsqliteuserinfo &ui)
{
	char		*errmsg;

	if (!do_connect())
		return false;

      	std::string querybuf;

	if (config_file.select_clause.empty())
	{
		std::ostringstream o;

		o << "SELECT "
		  << config_file.login_field << ", "
		  << config_file.crypt_field << ", "
		  << config_file.clear_field << ", "
		  << config_file.uid_field << ", "
		  << config_file.gid_field << ", "
		  << config_file.home_field << ", "
		  << (strcmp(service, "courier") == 0 ?
		      config_file.defaultdelivery_field:
		      config_file.maildir_field) << ", "
		  << config_file.quota_field << ", "
		  << config_file.name_field << ", "
		  << config_file.options_field
		  << " FROM " << config_file.user_table
		  << " WHERE " << config_file.login_field
		  << " = '" << escape(username);

		if (strchr(username, '@') == NULL &&
		    !config_file.defdomain.empty())
		{
			o << "@" << config_file.defdomain;
		}

		o  << "' AND (" <<  config_file.where_clause << ")";

		querybuf=o.str();
	}
	else
	{
		std::map<std::string, std::string> parameters;

		parameters["service"]=service;

		querybuf=config_file
			.parse_custom_query(config_file.select_clause,
					    escape(username),
					    config_file.defdomain,
					    parameters);
	}

	DPRINTF("SQL query: %s", querybuf.c_str());
	errmsg=0;

	select_callback_info info(ui);
	if (sqlite3_exec(dbh, querybuf.c_str(), select_callback,
		reinterpret_cast<void *>(&info), &errmsg) != SQLITE_OK)
	{
		if (errmsg)
		{
			err(errmsg);
			sqlite3_free(errmsg);
		}
		return false;
	}

	if (errmsg)
	{
		err(errmsg);
		sqlite3_free(errmsg);
	}

	return true;
}

static int dummy_callback(void *dummy, int n, char **columns, char **names)
{
	return 0;
}

bool authsqlite_connection::setpass(const char *user, const char *pass,
				    const char *oldpass)
{
	char *errmsg;
	bool rc=true;

	if (!do_connect())	return false;

	std::string newpass_crypt;

	{
		char *p;

		if (!(p=authcryptpasswd(pass, oldpass)))
			return false;

		newpass_crypt=p;
		free(p);
	}

	std::string clear_escaped=escape(pass);

	std::string crypt_escaped=escape(newpass_crypt);

	std::string sql_buf;

	if (config_file.chpass_clause.size() == 0)
	{
		std::string username_escaped=escape(user);

		bool has_domain=strchr(user, '@') != NULL;

		std::ostringstream o;

		o << "UPDATE " << config_file.user_table << " SET ";

		if (config_file.clear_field != "''")
			o << config_file.clear_field << "='"
			  << clear_escaped << "'";

		if (config_file.crypt_field != "''")
		{
			if (config_file.clear_field != "''") o << ", ";
			o << config_file.crypt_field << "='" << crypt_escaped << "'";
		}

		o << " WHERE " << config_file.login_field << "='"
		  << username_escaped;

		if (!has_domain && config_file.defdomain.size())
			o << "@" << config_file.defdomain;
		o << "'";

		sql_buf=o.str();
	}
	else
	{
		std::map<std::string, std::string> parameters;

		parameters["newpass"]=clear_escaped;
		parameters["newpass_crypt"]=crypt_escaped;

		sql_buf=config_file
			.parse_custom_query(config_file.chpass_clause,
					    user,
					    config_file.defdomain,
					    parameters);
	}

	if (courier_authdebug_login_level >= 2)
	{
		DPRINTF("setpass SQL: %s", sql_buf.c_str());
	}

	errmsg=NULL;

	if (sqlite3_exec(dbh, sql_buf.c_str(), dummy_callback,
			 NULL, &errmsg) != SQLITE_OK)
	{
		rc=false;
	}
	else
	{
		if (sqlite3_changes(dbh) > 0)
		{
			DPRINTF("authsqllite: password updated");
		}
		else
		{
			rc=false;
			DPRINTF("authsqllite: password not updated");
		}
	}

	if (errmsg)
	{
		err(errmsg);
		sqlite3_free(errmsg);
	}

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

void authsqlite_connection::enumerate( void(*cb_func)(const char *name,
						      uid_t uid,
						      gid_t gid,
						      const char *homedir,
						      const char *maildir,
						      const char *options,
						      void *void_arg),
				       void *void_arg)
{
	char *errmsg;
	struct enumerate_user_cb cb;
	std::string querybuf;

	cb.cb_func=cb_func;
	cb.void_arg=void_arg;

	if (!do_connect())      return;

	if (config_file.enumerate_clause.empty())
	{
		std::ostringstream o;

		o << "SELECT "
		  << config_file.login_field << ", "
		  << config_file.uid_field << ", "
		  << config_file.gid_field << ", "
		  << config_file.home_field << ", "
		  << config_file.maildir_field << ", "
		  << config_file.options_field
		  << " FROM " << config_file.user_table
		  << " WHERE " << config_file.where_clause;

		querybuf=o.str();
	}
	else
	{
		std::map<std::string, std::string> parameters;

		parameters["service"]="enumerate";
		querybuf=config_file
			.parse_custom_query(config_file.enumerate_clause, "*",
					    config_file.defdomain, parameters);
	}

	DPRINTF("authsqlite: enumerate query: %s", querybuf.c_str());

	errmsg=NULL;

	sqlite3_exec(dbh, querybuf.c_str(), enumerate_callback,
		     reinterpret_cast<void *>(&cb), &errmsg);

	if (errmsg)
	{
		err(errmsg);
		sqlite3_free(errmsg);
	}

	(*cb.cb_func)(NULL, 0, 0, NULL, NULL, NULL, cb.void_arg);
}

/////////////////////////////////////////////////////////////////////////////

int auth_sqlite_setpass(const char *user, const char *pass,
			const char *oldpass)
{
	authsqlite_connection *conn=authsqlite_connection::connect();

	if (conn && !conn->setpass(user, pass, oldpass))
		return -1;
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
	authsqlite_connection *conn=authsqlite_connection::connect();

	if (conn)
		conn->enumerate(cb_func, void_arg);
}

bool auth_sqlite_getuserinfo(const char *username,
			     const char *service,
			     authsqliteuserinfo &ui)
{
	authsqlite_connection *conn=authsqlite_connection::connect();

	if (conn)
		return conn->getuserinfo(username, service, ui);
	return false;

}

void auth_sqlite_cleanup()
{
	do_auth_sqlite_cleanup();
}
