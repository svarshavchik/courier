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
#include	<mysql.h>
#include	<time.h>

#include	"authmysql.h"

extern "C" {
#include	"authmysqlrc.h"
#include	"auth.h"
#include	"courierauthdebug.h"
};

#include <map>
#include <sstream>
#include <algorithm>

#define err courier_auth_err

#include "authconfigfile.h"

class authmysql_connection {

public:
	MYSQL *mysql;
	time_t last_time;

	class authmysqlrc_vars {
	public:
		std::string server, server_socket, userid, password, database,
			character_set,
			sslkey, sslcert, sslcacert,
			sslcapath, sslcipher,

			defdomain, user_table,
			uid_field,
			gid_field,
			name_field,
			crypt_field, clear_field,
			login_field,
			home_field,
			maildir_field,
			defaultdelivery_field,
			quota_field,
			options_field,
			where_clause,
			select_clause,
			enumerate_clause,
			chpass_clause;

		unsigned int server_port;
		unsigned int server_opt;

		authmysqlrc_vars()
			: server_port(0), server_opt(0) {}
	};

	class authmysqlrc_file : public courier::auth::config_file,
				 public authmysqlrc_vars {

		authmysql_connection &conn;

	public:

		authmysqlrc_file &operator=(const authmysqlrc_file &o)
		{
			courier::auth::config_file::operator=(o);
			authmysqlrc_vars::operator=(o);
			return *this;
		}

		authmysqlrc_file(authmysql_connection &connArg)
			: courier::auth::config_file(AUTHMYSQLRC),
			  conn(connArg)
		{
		}

		bool do_load()
		{
			server=config("MYSQL_SERVER");
			userid=config("MYSQL_USERNAME");
			password=config("MYSQL_PASSWORD");
			database=config("MYSQL_DATABASE");
			character_set=config("MYSQL_CHARACTER_SET");

			sslkey=config("MYSQL_SSL_KEY");
			sslcert=config("MYSQL_SSL_CERT");
			sslcacert=config("MYSQL_SSL_CACERT");
			sslcapath=config("MYSQL_SSL_CAPATH");
			sslcipher=config("MYSQL_SSL_CIPHER");

			if ((std::istringstream(config("MYSQL_PORT"))
			     >> server_port).fail())
			{
				err("authmysql: cannot parse the MYSQL_PORT "
				    "setting");
				return false;
			}

			if ((std::istringstream(config("MYSQL_OPT"))
			     >> server_opt).fail())
			{
				err("authmysql: cannot parse the MYSQL_OPT "
				    "setting");
				return false;
			}
			server_socket=config("MYSQL_SOCKET");

			if (!server.size() && !server_socket.size())
			{
				err("authmysql: MYSQL_SERVER nor MYSQL_SOCKET set in"
				    AUTHMYSQLRC ".");
				return false;
			}

			if (!userid.size())
			{
				err("authmysql: MYSQL_USERNAME not set in "
				    AUTHMYSQLRC ".");
				return false;
			}

			if (!database.size())
			{
				err("authmysql: MYSQL_DATABASE not set in "
				    AUTHMYSQLRC ".");
				return false;
			}

			defdomain=config("DEFAULT_DOMAIN");
			user_table=config("MYSQL_USER_TABLE");

			if (!user_table.size())
			{
				err("authmysql: MYSQL_USER_TABLE not set in "
				    AUTHMYSQLRC ".");
				return false;
			}

			uid_field=config("MYSQL_UID_FIELD", "uid");
			gid_field=config("MYSQL_GID_FIELD", "gid");
			name_field=config("MYSQL_NAME_FIELD", "''");
			login_field=config("MYSQL_LOGIN_FIELD", "id");
			home_field=config("MYSQL_HOME_FIELD", "home");
			maildir_field=config("MYSQL_MAILDIR_FIELD",
					     "''");
			defaultdelivery_field=
				config("MYSQL_DEFAULTDELIVERY_FIELD",
				       "''");
			quota_field=config("MYSQL_QUOTA_FIELD", "''");

			options_field=config("MYSQL_AUXOPTIONS_FIELD",
					     "''");
			where_clause=config("MYSQL_WHERE_CLAUSE",
					    "1=1");
			select_clause=config("MYSQL_SELECT_CLAUSE");
			enumerate_clause=config("MYSQL_ENUMERATE_CLAUSE");

			chpass_clause=config("MYSQL_CHPASS_CLAUSE");
			crypt_field=config("MYSQL_CRYPT_PWFIELD", "''");
			clear_field=config("MYSQL_CLEAR_PWFIELD", "''");

			if (crypt_field == "''" && clear_field == "''")
			{
				err("authmysql: MYSQL_CRYPT_PWFIELD and "
				    "MYSQL_CLEAR_PWFIELD not set in "
				    AUTHMYSQLRC ".");
				return false;
			}

			return true;
		}

		void do_reload()
		{
			authmysqlrc_file new_file(conn);

			if (new_file.load(true))
			{
				*this=new_file;
				DPRINTF("authmysql: reloaded %s", filename);

				// Disconnect from the server, new login
				// parameters, perhaps.

				conn.cleanup();
			}
		}
	};

	authmysqlrc_file config_file;

	authmysql_connection() : mysql(0), last_time(0), config_file(*this)
	{
	}

	~authmysql_connection()
	{
		cleanup();
	}

	void cleanup()
	{
		if (mysql)
		{
			mysql_close(mysql);
			delete mysql;
			mysql=0;
		}
	}

	static authmysql_connection *singleton;

	bool try_connection()
	{
		bool rc=check_connection();

		if (!rc)
			cleanup();
		return rc;
	}

	bool check_connection();

	class result {

		MYSQL_RES *res;
		MYSQL_ROW row;

		size_t num_fields_n;
		unsigned long *lengths;
	public:
		result(const authmysql_connection &conn, bool use_result=false)
			: res(use_result
			      ? mysql_use_result(conn.mysql)
			      : mysql_store_result(conn.mysql)), row(NULL),
			  num_fields_n(0),
			  lengths(NULL)
		{
		}

		~result()
		{
			if (res)
				mysql_free_result(res);
		}

		operator bool() const
		{
			return res != NULL;
		}

		size_t num_rows() const
		{
			return res ? mysql_num_rows(res):0;
		}

		size_t num_fields() const { return num_fields_n; }

		bool fetch()
		{
			if (res && (row=mysql_fetch_row(res)) != NULL)
			{
				num_fields_n=mysql_num_fields(res);
				lengths=mysql_fetch_lengths(res);
				return true;
			}
			num_fields_n=0;
			lengths=NULL;
			return false;
		}

		std::string operator[](size_t column) const
		{
			if (column < num_fields())
			{
				const char *p=reinterpret_cast<const char *>
					(row[column]);

				return std::string(p, p + lengths[column]);
			}
			return std::string();
		}
	};

	bool query(const std::string &sql)
	{
		if (mysql_query(mysql, sql.c_str()) == 0)
			return true;

		DPRINTF("mysql_query failed: %s", mysql_error(mysql));
		cleanup();

		if (!try_connection())
			return false;

		if (mysql_query (mysql, sql.c_str()))
		{
			DPRINTF("mysql_query failed second time, giving up: %s", mysql_error(mysql));

			cleanup();
			return false;
		}
		return true;
	}

	std::string escape(const std::string &s)
	{
		std::string buffer;
		size_t n=s.size()*2+1;

		buffer.resize(n);

		mysql_real_escape_string(mysql, &buffer[0],
					 s.c_str(), s.size());
		buffer.resize(strlen(&buffer[0]));
		return buffer;
	}

	std::string get_default_select(const char *username,
				       const char *service);

	bool getuserinfo(const char *username,
			 const char *service,
			 authmysqluserinfo &uiret);

	bool setpass(const char *user, const char *pass,
		     const char *oldpass);

	void enumerate( void(*cb_func)(const char *name,
				       uid_t uid,
				       gid_t gid,
				       const char *homedir,
				       const char *maildir,
				       const char *options,
				       void *void_arg),
			void *void_arg);

	static bool connect()
	{
		if (!singleton)
			singleton=new authmysql_connection;

		if (!singleton->config_file.load())
			return false;

		return singleton->try_connection();
	}
};

authmysql_connection *authmysql_connection::singleton=0;

bool authmysql_connection::check_connection()
{
	bool use_ssl=false;

	/*
	** Periodically detect dead connections.
	*/
	if (mysql)
	{
		time_t t_check;

		time(&t_check);

		if (t_check < last_time)
			last_time=t_check;	/* System clock changed */

		if (t_check < last_time + 60)
			return true;

		last_time=t_check;

		if (mysql_ping(mysql) == 0) return true;

		DPRINTF("authmysqllib: mysql_ping failed, connection lost");
		cleanup();
	}

	if (config_file.sslcacert.size() || config_file.sslcapath.size())
	{
		if (config_file.sslcert.size())
			DPRINTF("authmysqllib: certificate file set to %s",
				config_file.sslcert.c_str());

		if (config_file.sslcipher.size())
			DPRINTF("authmysqllib: ciphers set to %s",
				config_file.sslcipher.c_str());

		if (config_file.sslcacert.size())
			DPRINTF("authmysqllib: certificate authority set to %s",
				config_file.sslcacert.c_str());
		if (config_file.sslcapath.size())
			DPRINTF("authmysqllib: certificate authority set to %s",
				config_file.sslcapath.c_str());
		use_ssl=true;
	}

	MYSQL *conn=new MYSQL;

	mysql_init(conn);
	if (use_ssl)
	{
		const char *key=config_file.sslkey.c_str();
		const char *cert=config_file.sslcert.c_str();
		const char *cacert=config_file.sslcacert.c_str();
		const char *capath=config_file.sslcapath.c_str();
		const char *cipher=config_file.sslcipher.c_str();

		if (!*key) key=0;
		if (!*cert) cert=0;
		if (!*cacert) cacert=0;
		if (!*capath) capath=0;
		if (!*cipher) cipher=0;

		mysql_ssl_set(conn, key, cert, cacert,
			      capath, cipher);
	}

	mysql=mysql_real_connect(conn, config_file.server.c_str(),
				 config_file.userid.c_str(),
				 config_file.password.c_str(),
				 NULL,
				 config_file.server_port,
				 (config_file.server_socket.size() ?
				  config_file.server_socket.c_str():0),
				 config_file.server_opt);

	if (!mysql)
	{
		err("failed to connect to mysql server (server=%s, userid=%s): %s",
		    config_file.server.size() ? config_file.server.c_str() : "<null>",
		    config_file.userid.size() ? config_file.userid.c_str() : "<null>",
		    mysql_error(conn));

		delete conn;
		return false;
	}

	if (mysql_select_db(mysql, config_file.database.c_str()))
	{
		err("authmysql: mysql_select_db(%s) error: %s",
		    config_file.database.c_str(), mysql_error(mysql));
		return false;
	}

	DPRINTF("authmysqllib: connected. Versions: "
		"header %lu, "
		"client %lu, "
		"server %lu",
		(long)MYSQL_VERSION_ID,
		mysql_get_client_version(),
		mysql_get_server_version(mysql));

        if (config_file.character_set.size())
	{
		mysql_set_character_set(mysql,
					config_file.character_set.c_str());

		std::string real_character_set=mysql_character_set_name(mysql);

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

std::string authmysql_connection::get_default_select(const char *username,
						     const char *service)
{
	std::string q;


	std::string maildir_field=
		service && strcmp(service, "courier")==0
		? config_file.defaultdelivery_field
		: config_file.maildir_field;

	bool has_domain=strchr(username, '@') != NULL;

	std::ostringstream o;

	o << "SELECT "
	  << config_file.login_field << ", "
	  << config_file.crypt_field << ", "
	  << config_file.clear_field << ", "
	  << config_file.uid_field << ", "
	  << config_file.gid_field << ", "
	  << config_file.home_field << ", "
	  << maildir_field << ", "
	  << config_file.quota_field << ", "
	  << config_file.name_field << ", "
	  << config_file.options_field
	  << " FROM "
	  << config_file.user_table
	  << " WHERE "
	  << config_file.login_field
	  << " = '"
	  << escape(username);

	if (!has_domain && config_file.defdomain.size())
	{
		o << "@" << config_file.defdomain;
	}
	o << "' AND (" << config_file.where_clause << ")";

	q=o.str();
	return q;
}

bool authmysql_connection::getuserinfo(const char *username,
				       const char *service,
				       authmysqluserinfo &uiret)
{
	std::string querybuf;

	if (config_file.select_clause.empty())
	{
		querybuf=get_default_select(username, service);
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

	if (!query(querybuf))
		return false;

	result res(*this);

	if (res.fetch())
	{
		if (res.num_fields() < 6)
		{
			DPRINTF("incomplete row, only %d fields returned",
				res.num_fields());
			return false;
		}

		uiret.username=res[0];
		uiret.cryptpw=res[1];
		uiret.clearpw=res[2];

		std::string uid_s=res[3];
		std::string gid_s=res[4];

		std::istringstream(uid_s) >> uiret.uid;
		std::istringstream(gid_s) >> uiret.gid;

		if (uiret.uid == 0)
		{
			DPRINTF("invalid value for uid: '%s'",
				uid_s.size() ? uid_s.c_str() : "<null>");
			return false;
		}

		if (uiret.gid == 0)
		{
			DPRINTF("invalid value for gid: '%s'",
				gid_s.size() ? gid_s.c_str() : "<null>");
			return false;
		}
		uiret.home=res[5];

		if (uiret.home.size() == 0)
		{
			DPRINTF("required value for 'home' (column 6) is missing");
			return false;
		}

		uiret.maildir=res[6];
		uiret.quota=res[7];
		uiret.fullname=res[8];
		uiret.options=res[9];
	}
	else
	{
		DPRINTF("zero rows returned");
	}
	return true;
}

bool authmysql_connection::setpass(const char *user, const char *pass,
				   const char *oldpass)
{
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

	if (!query(sql_buf))
		return false;
	return true;
}

void authmysql_connection::enumerate(void(*cb_func)(const char *name,
						    uid_t uid,
						    gid_t gid,
						    const char *homedir,
						    const char *maildir,
						    const char *options,
						    void *void_arg),
				     void *void_arg)
{
	std::string querybuf;

	if (config_file.enumerate_clause.empty())
	{
		std::ostringstream o;

		o << "SELECT "
		  << config_file.login_field << ", "
		  << config_file.uid_field << ", "
		  << config_file.gid_field << ", "
		  << config_file.home_field << ", "
		  << config_file.maildir_field << ", "
		  << config_file.options_field << " FROM "
		  << config_file.user_table << " WHERE "
		  << config_file.where_clause;

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
	DPRINTF("authmysql: enumerate query: %s", querybuf.c_str());

	if (!query(querybuf))
		return;

	result row(*this);

	if (row)
	{
		while (row.fetch())
		{
			std::string username=row[0];

			if (username.size() == 0)
				continue;

			uid_t uid=0;
			gid_t gid=0;

			std::istringstream(row[1]) >> uid;
			std::istringstream(row[2]) >> gid;
			std::string homedir=row[3];
			std::string maildir=row[4];
			std::string options=row[5];

			(*cb_func)(username.c_str(), uid, gid,
				   homedir.c_str(),
				   maildir.size() ? maildir.c_str():NULL,
				   options.size() ? options.c_str():NULL,
				   void_arg);
		}
	}
	/* NULL row could indicate end of result or an error */
	if (mysql_errno(mysql))
	{
		DPRINTF("mysql error during enumeration: %s",
			mysql_error(mysql));
	}
	else
		(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
}

void auth_mysql_cleanup()
{
	if (authmysql_connection::singleton)
	{
		delete authmysql_connection::singleton;
		authmysql_connection::singleton=0;
	}
}

bool auth_mysql_setpass(const char *user, const char *pass,
			const char *oldpass)
{
	if (!authmysql_connection::connect())
		return false;
	return authmysql_connection::singleton->setpass(user, pass, oldpass);
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
	if (!authmysql_connection::connect())
	{
		(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
		return;
	}

	authmysql_connection::singleton->enumerate(cb_func, void_arg);
}

bool auth_mysql_getuserinfo(const char *username,
			    const char *service,
			    authmysqluserinfo &uiret)
{
	if (!authmysql_connection::connect())
		return false;

	return authmysql_connection::singleton->getuserinfo(username, service,
							    uiret);
}
