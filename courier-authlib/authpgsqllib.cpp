/*
** Copyright 2000-2016 Double Precision, Inc.  See COPYING for
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
#include	<errno.h>

extern "C" {
#include	"authpgsql.h"
#include	"authpgsqlrc.h"
#include	"auth.h"
#include	"courierauthdebug.h"
};

#include "authconfigfile.h"
#include <string>

#define err courier_auth_err


class authpgsql_userinfo {
public:
	std::string username;
	std::string fullname;
	std::string cryptpw;
	std::string clearpw;
	std::string home;
	std::string maildir;
	std::string quota;
	std::string options;
	uid_t uid;
	gid_t gid;
	} ;

class authpgsql_connection {

	time_t last_time;
	PGconn *pgconn;
public:

	class sentquery {

		int status;

	public:
		sentquery(const authpgsql_connection &conn,
			  const std::string &query)
			: status(PQsendQuery(conn.pgconn, query.c_str()))
		{
			if (status == 0)
				DPRINTF("PQsendQuery failed: %s",
					PQerrorMessage(conn.pgconn));
		}

		operator bool() const
		{
			return status != 0;
		}
	};

	class result {
		PGresult *res;

	public:
		result(const authpgsql_connection &conn,
		       const std::string &query)
			: res(PQexec(conn.pgconn, query.c_str()))
		{
		}

		result(const authpgsql_connection &conn,
		       const sentquery &query)
			: res(PQgetResult(conn.pgconn))
		{
		}

		~result()
		{
			if (res)
				PQclear(res);
		}

		result(const result &res);
		result &operator=(const result &res);

		operator bool() const { return res != 0; }

		bool query_failed() const
		{
			return res == 0
				|| PQresultStatus(res) != PGRES_TUPLES_OK;
		}

		bool command_failed() const
		{
			return res == 0
				|| PQresultStatus(res) != PGRES_COMMAND_OK;
		}

		size_t ntuples() const
		{
			return res == 0 ? 0: PQntuples(res);
		}

		size_t nfields() const
		{
			return res == 0 ? 0: PQnfields(res);
		}

		std::string value(size_t tuple, size_t field) const
		{
			std::string v;

			if (tuple < ntuples() && field < nfields())
			{
				const char *p=PQgetvalue(res, tuple, field);

				if (p)
					v=p;
			}
			return v;
		}
	};

	class authpgsqlrc_vars {

	public:

		std::string character_set;
		std::string connection;
		std::string select_clause;
		std::string chpass_clause;
		std::string enumerate_clause;
		std::string defdomain;
		std::string user_table;
		std::string clear_field;
		std::string crypt_field;
		std::string name_field;
		std::string uid_field;
		std::string gid_field;
		std::string login_field;
		std::string home_field;
		std::string maildir_field;
		std::string defaultdelivery_field;
		std::string quota_field;
		std::string options_field;
		std::string where_clause;
	};

	class authpgsqlrc_file : public courier::auth::config_file,
				 public authpgsqlrc_vars {

		authpgsql_connection &conn;

	public:

		authpgsqlrc_file &operator=(const authpgsqlrc_file &o)
		{
			courier::auth::config_file::operator=(o);
			authpgsqlrc_vars::operator=(o);
			return *this;
		}

		authpgsqlrc_file(authpgsql_connection &connArg)
			: courier::auth::config_file(AUTHPGSQLRC),
			  conn(connArg)
		{
		}

		bool do_load()
		{
			character_set=config("PGSQL_CHARACTER_SET");

			if (!config("PGSQL_CONNECTION", connection, true))
				return false;

			select_clause=config("PGSQL_SELECT_CLAUSE");
			chpass_clause=config("PGSQL_CHPASS_CLAUSE");
			enumerate_clause=config("PGSQL_ENUMERATE_CLAUSE");

			defdomain=config("DEFAULT_DOMAIN");

			if (select_clause.empty() || chpass_clause.empty() ||
			    enumerate_clause.empty())
			{
				if (!config("PGSQL_USER_TABLE", user_table, true))
					return false;

				clear_field=config("PGSQL_CLEAR_PWFIELD");
				if (clear_field.empty())
				{
					if (!config("PGSQL_CRYPT_PWFIELD",
						    crypt_field,
						    true))
						return false;
				}
				else
				{
					crypt_field=config("PGSQL_CRYPT_PWFIELD");
				}

				config("PGSQL_NAME_FIELD", name_field, false,
				       "''");

				if (crypt_field.empty()) crypt_field="''";
				if (clear_field.empty()) clear_field="''";

				config("PGSQL_UID_FIELD", uid_field, false, "uid");
				config("PGSQL_GID_FIELD", gid_field, false, "gid");
				config("PGSQL_LOGIN_FIELD", login_field, false, "id");
				config("PGSQL_HOME_FIELD", home_field, false, "home");
				config("PGSQL_MAILDIR_FIELD", maildir_field, false, "''");
				config("PGSQL_DEFAULTDELIVERY", defaultdelivery_field, false, "''");
				config("PGSQL_QUOTA_FIELD", quota_field, false, "''");
				config("PGSQL_AUXOPTIONS_FIELD", options_field, false, "''");

				config("PGSQL_WHERE_CLAUSE", where_clause, false, "1=1");
			}

			return true;
		}

		void do_reload()
		{
			authpgsqlrc_file new_file(conn);

			if (new_file.load(true))
			{
				*this=new_file;
				DPRINTF("authpgsql: reloaded %s", filename);

				// Disconnect from the server, new login
				// parameters, perhaps.

				conn.disconnect();
			}
		}
	};

	authpgsqlrc_file config_file;

	authpgsql_connection()
		: last_time(0), pgconn(0), config_file(*this)
	{
	}

	~authpgsql_connection()
	{
		disconnect();
	}

	void disconnect()
	{
		if (pgconn)
		{
			PQfinish(pgconn);
			pgconn=0;
		}
	}

	bool do_connect();

	bool getuserinfo(authpgsql_userinfo &uiret,
			 const char *username,
			 const char *service);

private:
	bool getuserinfo(authpgsql_userinfo &uiret,
			 const result &res);
public:
	bool setpass(const char *user, const char *pass, const char *oldpass);

	void enumerate( void(*cb_func)(const char *name,
				       uid_t uid,
				       gid_t gid,
				       const char *homedir,
				       const char *maildir,
				       const char *options,
				       void *void_arg),
			void *void_arg);
	void enumerate( const sentquery &sent,
			void(*cb_func)(const char *name,
				       uid_t uid,
				       gid_t gid,
				       const char *homedir,
				       const char *maildir,
				       const char *options,
				       void *void_arg),
			void *void_arg);

	std::string escape(const std::string &s)
	{
		std::string buffer;
		size_t n=s.size()*2+1;

		buffer.resize(n);

		n=PQescapeStringConn(pgconn, &buffer[0],
				     s.c_str(), s.size(), 0);

		buffer.resize(n);

		return buffer;
	}

	std::string escape_username(std::string username)
	{
		if (username.find('@') == username.npos &&
		    !config_file.defdomain.empty())
		{
			username.push_back('@');
			username += config_file.defdomain;
		}

		return escape(username);
	}

	static authpgsql_connection *singleton;
};

bool authpgsql_connection::do_connect()
{
	if (pgconn)
	{
		time_t t_check;

		time(&t_check);

		if (t_check < last_time)
			last_time=t_check;	/* System clock changed */

		if (t_check < last_time + 60)
			return true;

		last_time=t_check;

		if (PQstatus(pgconn) == CONNECTION_OK) return true;

		DPRINTF("authpgsql: PQstatus failed, connection lost");
		PQfinish(pgconn);
		pgconn=0;
	}

	pgconn = PQconnectdb(config_file.connection.c_str());

	if (PQstatus(pgconn) == CONNECTION_BAD)
    	{
       		err("PGSQL_CONNECTION could not be established");
        	err("%s", PQerrorMessage(pgconn));
		PQfinish(pgconn);
		pgconn=0;
		return false;
    	}

	if (!config_file.character_set.empty())
	{
		PQsetClientEncoding(pgconn,
				    config_file.character_set.c_str());
		std::string real_character_set=
			pg_encoding_to_char(PQclientEncoding(pgconn));

		if (config_file.character_set != real_character_set)
		{
			err("Cannot set character set to \"%s\","
			    " using \"%s\"\n",
			    config_file.character_set.c_str(),
			    real_character_set.c_str());
		}
		else
		{
			DPRINTF("Using character set: %s",
				config_file.character_set.c_str());
		}
        }

	return true;
}

bool authpgsql_connection::getuserinfo(authpgsql_userinfo &uiret,
				       const char *username,
				       const char *service)
{
	std::string querybuf;

	if (!do_connect())
		return false;

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
		      config_file.defaultdelivery_field
		      :config_file.maildir_field) << ", "
		  << config_file.quota_field << ", "
		  << config_file.name_field << ", "
		  << config_file.options_field
		  << " FROM " << config_file.user_table
		  << " WHERE " << config_file.login_field
		  << " = '"
		  << escape_username(username)
		  << "' AND (" << config_file.where_clause << ")";

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

	result res1(*this, querybuf);

	if (res1)
		return getuserinfo(uiret, res1);

	disconnect();
	if (do_connect())
	{
		result res2(*this, querybuf);

		if (res2)
			return getuserinfo(uiret, res2);
	}
	return false;
}

bool authpgsql_connection::getuserinfo(authpgsql_userinfo &uiret,
				       const result &res)
{
	if (res.query_failed())
		return false;

	if (res.ntuples() > 0)
	{
		if (res.nfields() < 6)
		{
			DPRINTF("incomplete row, only %d fields returned",
				res.nfields());
			return false;
		}

		uiret.username=res.value(0, 0);
		uiret.cryptpw=res.value(0, 1);
		uiret.clearpw=res.value(0, 2);

		{
			std::string v=res.value(0, 3);

			std::istringstream i(v);

			i >> uiret.uid;

			if (i.fail() || !i.eof())
			{
				DPRINTF("invalid value for uid: '%s'",
					v.c_str());
				return false;
			}
		}

		{
			std::string v=res.value(0, 4);

			std::istringstream i(v);

			i >> uiret.gid;

			if (i.fail() || !i.eof())
			{
				DPRINTF("invalid value for gid: '%s'",
					v.c_str());
				return false;
			}
		}


		uiret.home=res.value(0, 5);
		uiret.maildir=res.value(0, 6);
		uiret.quota=res.value(0, 7);
		uiret.fullname=res.value(0, 8);
		uiret.options=res.value(0, 9);
	}
	else
	{
		DPRINTF("zero rows returned");
		return true;
	}

	return true;
}

bool authpgsql_connection::setpass(const char *user, const char *pass,
				   const char *oldpass)
{
	if (!do_connect())
		return false;

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

	if (config_file.chpass_clause.empty())
	{
		std::ostringstream o;

		o << "UPDATE " << config_file.user_table
		  << " SET ";
		if (config_file.clear_field != "''")
		{
			o << config_file.clear_field << "='"
			  << clear_escaped
			  << "'";

			if (config_file.crypt_field != "''")
				o << ", ";
		}

		if (config_file.crypt_field != "''")
		{
			o << config_file.crypt_field << "='"
			  << crypt_escaped
			  << "'";
		}

		o << " WHERE "
		  << config_file.login_field << "='"
		  << escape_username(user)
		  << "' AND (" << config_file.where_clause << ")";
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

	result res(*this, sql_buf);

	if (res.command_failed())
	{
		DPRINTF("setpass SQL failed");
		disconnect();
		return false;
	}
	return (true);
}

void authpgsql_connection::enumerate( void(*cb_func)(const char *name,
						       uid_t uid,
						       gid_t gid,
						       const char *homedir,
						       const char *maildir,
						       const char *options,
						       void *void_arg),
					void *void_arg)
{
	if (!do_connect())
	{
		(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
		return;
	}

	std::string sql_buf;

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
		  << " FROM "
		  << config_file.user_table << " WHERE "
		  << config_file.where_clause;

		sql_buf=o.str();
	}
	else
	{
		std::map<std::string, std::string> parameters;

		parameters["service"]="enumerate";
		sql_buf=config_file
			.parse_custom_query(config_file.enumerate_clause, "*",
					    config_file.defdomain, parameters);
	}

	DPRINTF("authpgsql: enumerate query: %s", sql_buf.c_str());

	sentquery query1(*this, sql_buf);

	if (query1)
	{
		enumerate(query1, cb_func, void_arg);
		return;
	}

	disconnect();

	if (!do_connect())
		return;

	sentquery query2(*this, sql_buf);
	if (query2)
	{
		enumerate(query2, cb_func, void_arg);
		return;
	}
}

void authpgsql_connection::enumerate( const sentquery &sent,
				      void(*cb_func)(const char *name,
						     uid_t uid,
						     gid_t gid,
						     const char *homedir,
						     const char *maildir,
						     const char *options,
						     void *void_arg),
				      void *void_arg)
{
	while (1)
	{
		result res(*this, sent);

		if (!res)
			break;

		if (res.query_failed())
			continue;

		size_t n=res.ntuples();

		for (size_t i=0; i<n; ++i)
		{
			std::string username=res.value(i, 0);
			std::string uid_s=res.value(i, 1);
			std::string gid_s=res.value(i, 2);
			std::string homedir=res.value(i, 3);
			std::string maildir=res.value(i, 4);
			std::string options=res.value(i, 5);

			uid_t uid=0;
			gid_t gid=0;

			std::istringstream(uid_s) >> uid;
			std::istringstream(gid_s) >> gid;

			if (username.empty() || homedir.empty())
				continue;

			(*cb_func)(username.c_str(), uid, gid,
				   homedir.c_str(),
				   (maildir.empty() ? 0:maildir.c_str()),
				   (options.empty() ? 0:options.c_str()),
				   void_arg);
		}
	}
	(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
}


static authpgsql_connection *get_conn()
{
	if (authpgsql_connection::singleton)
	{
		authpgsql_connection::singleton->config_file.load(true);
		return authpgsql_connection::singleton;
	}

	authpgsql_connection *new_conn=new authpgsql_connection;

	if (new_conn->config_file.load())
	{
		authpgsql_connection::singleton=new_conn;
		return new_conn;
	}

	delete new_conn;
	return NULL;
}

authpgsql_connection *authpgsql_connection::singleton=0;


static bool auth_pgsql_getuserinfo(authpgsql_userinfo &uiret,
				   const char *username,
				   const char *service)
{
	authpgsql_connection *conn=get_conn();

	if (!conn)
		return false;

	return conn->getuserinfo(uiret, username, service);
}

static bool docheckpw(authpgsql_userinfo &authinfo,
			   const char *pass)
{
	if (!authinfo.cryptpw.empty())
	{
		if (authcheckpassword(pass,authinfo.cryptpw.c_str()))
		{
			errno=EPERM;
			return false;
		}
	}
	else if (!authinfo.clearpw.empty())
	{
		if (authinfo.clearpw != pass)
		{
			if (courier_authdebug_login_level >= 2)
			{
				DPRINTF("supplied password '%s' does not match clearpasswd '%s'",
					pass, authinfo.clearpw.empty());
			}
			else
			{
				DPRINTF("supplied password does not match clearpasswd");
			}
			errno=EPERM;
			return false;
		}
	}
	else
	{
		DPRINTF("no password available to compare");
		errno=EPERM;
		return false;
	}
	return true;
}

static int do_auth_pgsql_login(const char *service, char *authdata,
			       int (*callback_func)(struct authinfo *, void *),
			       void *callback_arg)
{
	char *user, *pass;
	authpgsql_userinfo uiret;
	struct	authinfo	aa;

	if ((user=strtok(authdata, "\n")) == 0 ||
		(pass=strtok(0, "\n")) == 0)
	{
		errno=EPERM;
		return (-1);
	}

	if (!auth_pgsql_getuserinfo(uiret, user, service))
	{
		errno=EACCES;	/* Fatal error - such as PgSQL being down */
		return (-1);
	}

	if (!docheckpw(uiret, pass))
		return -1;

	memset(&aa, 0, sizeof(aa));

	aa.sysuserid= &uiret.uid;
	aa.sysgroupid= uiret.gid;
	aa.homedir=uiret.home.c_str();
	aa.maildir=uiret.maildir.empty() ? 0:uiret.maildir.c_str();
	aa.address=uiret.username.c_str();
	aa.quota=uiret.quota.empty() ? 0:uiret.quota.c_str();
	aa.fullname=uiret.fullname.c_str();
	aa.options=uiret.options.c_str();
	aa.passwd=uiret.cryptpw.empty() ? 0:uiret.cryptpw.c_str();
	aa.clearpasswd=pass;
	courier_authdebug_authinfo("DEBUG: authpgsql: ", &aa,
			    aa.clearpasswd, aa.passwd);
	return (*callback_func)(&aa, callback_arg);
}


static int do_auth_pgsql_changepw(const char *service, const char *user,
				  const char *pass,
				  const char *newpass)
{
	authpgsql_connection *conn=get_conn();

	if (!conn)
		return false;

	authpgsql_userinfo uiret;

	if (conn->getuserinfo(uiret, user, service))
	{
		if (!docheckpw(uiret, pass))
		{
			errno=EPERM;
			return -1;
		}

		if (!conn->setpass(user, newpass, uiret.cryptpw.c_str()))
		{
			errno=EPERM;
			return (-1);
		}
		return 0;
	}

	errno=EPERM;
	return (-1);
}

extern "C" {
#if 0
};
#endif

void auth_pgsql_cleanup()
{
	if (authpgsql_connection::singleton)
		delete authpgsql_connection::singleton;
	authpgsql_connection::singleton=0;
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
	authpgsql_connection *conn=get_conn();

	if (conn)
		conn->enumerate(cb_func, void_arg);
}

int auth_pgsql_login(const char *service, char *authdata,
		     int (*callback_func)(struct authinfo *, void *),
		     void *callback_arg)
{
	return do_auth_pgsql_login(service, authdata,
				   callback_func,
				   callback_arg);
}

int auth_pgsql_changepw(const char *service, const char *user,
			const char *pass,
			const char *newpass)
{
	return do_auth_pgsql_changepw(service, user, pass, newpass);
}

int	auth_pgsql_pre(const char *user, const char *service,
		       int (*callback)(struct authinfo *, void *), void *arg)
{
	struct	authinfo	aa;
	authpgsql_userinfo	uiret;

	if (!auth_pgsql_getuserinfo(uiret, user, service))
		return 1;

	if (uiret.home.empty())	/* User not found */
		return (-1);

	memset(&aa, 0, sizeof(aa));

	aa.sysuserid= &uiret.uid;
	aa.sysgroupid= uiret.gid;
	aa.homedir=uiret.home.c_str();
	aa.maildir=uiret.maildir.empty() ? 0:uiret.maildir.c_str();
	aa.address=uiret.username.c_str();
	aa.quota=uiret.quota.empty() ? 0:uiret.quota.c_str();
	aa.fullname=uiret.fullname.c_str();
	aa.options=uiret.options.c_str();
	aa.passwd=uiret.cryptpw.empty() ? 0:uiret.cryptpw.c_str();
	aa.clearpasswd=uiret.clearpw.empty() ? 0:uiret.clearpw.c_str();

	return ((*callback)(&aa, arg));
}

#if 0
{
#endif
};
