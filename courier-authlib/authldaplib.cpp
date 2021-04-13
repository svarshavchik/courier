/*
** Copyright 2016 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if HAVE_CONFIG_H
#include "courier_auth_config.h"
#endif
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cctype>

#if HAVE_LBER_H
#include <lber.h>
#endif
#if HAVE_LDAP_H
#include <ldap.h>
#endif
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <time.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

extern "C" {
#include "authldap.h"
#include "auth.h"
#include "authldaprc.h"
#include "courierauthdebug.h"
};

#include "authconfigfile.h"
#include <stdio.h>

#ifndef	LDAP_OPT_SUCCESS
#define LDAP_OPT_SUCCESS LDAP_SUCCESS
#endif

/*
** Main connection to the LDAP server, and a separate connection for the
** LDAP_AUTHBIND option.
*/

class ldap_connection {

public:
	LDAP *connection;
	bool bound;

	ldap_connection() : connection(0), bound(false) {}
	~ldap_connection() { disconnect(); }

	bool connected() const { return connection != 0; }

	bool connect();
	void disconnect();
	void close();

private:
	bool enable_tls();

public:

	static bool ok(const char *method, int rc)
	{
		if (rc == 0 || LDAP_NAME_ERROR(rc))
			return true;

		courier_auth_err("%s failed: %s", method,
				 ldap_err2string(rc));
		return false;
	}

	bool bind(const std::string &userid,
		  const std::string &password)
	{
		if (do_bind(userid, password))
		{
			bound=true;
			return true;
		}

		return false;
	}

private:
	bool do_bind(const std::string &userid,
		  const std::string &password)
	{
		std::vector<char> buffer(password.begin(), password.end());
		struct berval cred;

		cred.bv_len=buffer.size();
		cred.bv_val=&buffer[0];

		if (connect() &&
		    ok("ldap_sasl_bind_s",
		       ldap_sasl_bind_s(connection, userid.c_str(),
					NULL, &cred,
					NULL, NULL, NULL)))
			return true;

		disconnect();
		return connect() &&
			ok("ldap_sasl_bind_s",
			   ldap_sasl_bind_s(connection, userid.c_str(),
					    NULL, &cred,
					    NULL, NULL, NULL));
	}
};

ldap_connection main_connection, bind_connection;


// Loaded and parsed authldaprc configuration file.

class authldaprc_file : public courier::auth::config_file {

public:

	int protocol_version;
	int timeout;
	int authbind;
	int initbind;
	int tls;
	uid_t uid;
	gid_t gid;

	std::string ldap_uri, ldap_binddn, ldap_bindpw, ldap_basedn;
	int ldap_deref;

	std::vector<std::string> auxoptions, auxnames;

	authldaprc_file();

private:
	bool do_load();
	void do_reload();
public:
};

/*
** There's a memory leak in OpenLDAP 1.2.11, presumably in earlier versions
** too.  See http://www.OpenLDAP.org/its/index.cgi?findid=864 for more
** information.  To work around the bug, the first time a connection fails
** we stop trying for 60 seconds.  After 60 seconds we kill the process,
** and let the parent process restart it.
**
** We'll control this behavior via LDAP_MEMORY_LEAK.  Set it to ZERO to turn
** off this behavior (whenever OpenLDAP gets fixed).
*/

static time_t ldapfailflag=0;

static void ldapconnfailure()
{
	const char *p=getenv("LDAP_MEMORY_LEAK");

	if (!p)
	{
#ifdef LDAP_VENDOR_NAME
#ifdef LDAP_VENDOR_VERSION
#define DO_OPENLDAP_CHECK
#endif
#endif

#ifdef DO_OPENLDAP_CHECK

		/* It's supposed to be fixed in 20019 */

		if (strcmp(LDAP_VENDOR_NAME, "OpenLDAP") == 0 &&
		    LDAP_VENDOR_VERSION < 20019)
			p="1";
		else
			p="0";
#else
		p="0";
#endif
	}

	if (atoi(p) && !ldapfailflag)
	{
		time(&ldapfailflag);
		ldapfailflag += 60;
	}
}

static int ldapconncheck()
{
	time_t t;

	if (!ldapfailflag)
		return (0);

	time(&t);

	if (t >= ldapfailflag)
		exit(0);
	return (1);
}

authldaprc_file::authldaprc_file()
	: config_file(AUTHLDAPRC)
{
}

bool authldaprc_file::do_load()
{
	bool loaded=true;

	// Frequently-accessed variables.

	if (!config("LDAP_TIMEOUT", timeout, false, "5") ||
	    !config("LDAP_TLS", tls, false, "0"))
	{
		loaded=false;
	}

	if (!config("LDAP_URI", ldap_uri, true))
	{
		loaded=false;
	}

	ldap_deref=0;

	std::string deref_setting;

	config("LDAP_DEREF", deref_setting, false, "");

	for (std::string::iterator p=deref_setting.begin();
	     p != deref_setting.end(); ++p)
		*p=std::tolower(*p);

#ifdef LDAP_OPT_DEREF

	ldap_deref=LDAP_DEREF_NEVER;

	if (deref_setting == "never")
		ldap_deref = LDAP_DEREF_NEVER;
	else if (deref_setting == "searching")
		ldap_deref = LDAP_DEREF_SEARCHING;
	else if (deref_setting == "finding")
		ldap_deref = LDAP_DEREF_FINDING;
	else if (deref_setting == "always")
		ldap_deref = LDAP_DEREF_ALWAYS;
	else if (deref_setting != "")
	{
		loaded=false;
		courier_auth_err("authldap: INVALID LDAP_OPT_DEREF");
	}
#endif
	uid=0;
	gid=0;

	std::string uid_str, gid_str;

	config("LDAP_GLOB_UID", uid_str, false);
	config("LDAP_GLOB_GID", gid_str, false);

	if (!uid_str.empty())
	{
		std::istringstream i(uid_str);

		i >> uid;

		if (i.fail())
		{
			struct passwd *pwent=getpwnam(uid_str.c_str());

			if (!pwent)
			{
				courier_auth_err("authldap: INVALID LDAP_GLOB_UID");
				loaded=false;
			}
			else
			{
				uid=pwent->pw_uid;
			}
		}
	}

	if (!gid_str.empty())
	{
		std::istringstream i(uid_str);

		i >> gid;

		if (i.fail())
		{
			struct group  *grent=getgrnam(gid_str.c_str());

			if (!grent)
			{
				courier_auth_err("authldap: INVALID LDAP_GLOB_GID");
				loaded=false;
			}
			else
			{
				gid=grent->gr_gid;
			}
		}
	}

	if (!config("LDAP_AUTHBIND", authbind, false, "0")
	    || !config("LDAP_INITBIND", initbind, false, "0")
	    || !config("LDAP_BASEDN", ldap_basedn, true))
		loaded=false;

	if (initbind)
	{
		if (!config("LDAP_BINDDN", ldap_binddn, true) ||
		    !config("LDAP_BINDPW", ldap_bindpw, true))
			loaded=false;
	}

	if (!config("LDAP_PROTOCOL_VERSION", protocol_version, false, "0"))
		loaded=false;

	if (protocol_version)
	{
#ifndef LDAP_OPT_PROTOCOL_VERSION
		courier_auth_err("authldaplib: LDAP_OPT_PROTOCOL_VERSION ignored");
		loaded=false;
#endif
		if (0
#ifdef LDAP_VERSION_MIN
			|| protocol_version < LDAP_VERSION_MIN
#endif
#ifdef LDAP_VERSION_MAX
			|| protocol_version > LDAP_VERSION_MAX
#endif
		)
		{
			protocol_version=0;
			courier_auth_err("authldaplib: LDAP_PROTOCOL_VERSION not supported");
			loaded=false;
		}
	}

	std::string auxoptions_str;

	config("LDAP_AUXOPTIONS", auxoptions_str, "");

	std::string::iterator p=auxoptions_str.begin();

	while (p != auxoptions_str.end())
	{
		if (*p == ',')
		{
			++p;
			continue;
		}

		std::string::iterator q=p;

		p=std::find(p, auxoptions_str.end(), ',');

		std::string auxoption(q, p);
		std::string auxname;

		q=std::find(auxoption.begin(), auxoption.end(), '=');

		if (q != auxoption.end())
		{
			auxname=std::string(q+1, auxoption.end());
			auxoption=std::string(auxoption.begin(), q);
		}

		auxoptions.push_back(auxoption);
		auxnames.push_back(auxname);
	}

	return loaded;
}

void authldaprc_file::do_reload()
{
	// File changed, try to load it again.

	authldaprc_file new_file;

	if (new_file.load(true))
	{
		*this=new_file;
		DPRINTF("authldap: reloaded %s", filename);

		// If we reloaded the file, close the
		// connections, so they can be reopened using
		// the new config.

		main_connection.close();
		bind_connection.close();
	}
}

static authldaprc_file authldaprc;

#if HAVE_LDAP_TLS
bool ldap_connection::enable_tls()
{
	int version;

	if (!ok("ldap_get_option",
		ldap_get_option(connection, LDAP_OPT_PROTOCOL_VERSION,
				&version)))
		return false;

	if (version < LDAP_VERSION3)
	{
		version = LDAP_VERSION3;
		(void)ldap_set_option (connection,
				       LDAP_OPT_PROTOCOL_VERSION,
				       &version);
	}

	if (!ok("ldap_start_tls_s",
		ldap_start_tls_s(connection, NULL, NULL)))
		return false;

	return true;
}

#else

bool ldap_connection::enable_tls()
{
	courier_auth_err("authldaplib: TLS not available");
	return (-1);
}
#endif

bool ldap_connection::connect()
{
	if (connected()) return true;

	bound=false;

	DPRINTF("authldaplib: connecting to %s", authldaprc.ldap_uri.c_str());

	if (ldapconncheck())
	{
		DPRINTF("authldaplib: timing out after failed connection");
		return (false);
	}

	ldap_initialize(&connection, authldaprc.ldap_uri.c_str());

	if (connection==NULL)
	{
		courier_auth_err("cannot connect to LDAP server (%s): %s",
				 authldaprc.ldap_uri.c_str(), strerror(errno));
		ldapconnfailure();
	}
#ifdef LDAP_OPT_NETWORK_TIMEOUT
	else if (authldaprc.timeout > 0)
	{
		DPRINTF("timeout set to %d", authldaprc.timeout);
		ldap_set_option (connection, LDAP_OPT_NETWORK_TIMEOUT, &authldaprc.timeout);
#endif
	}

#ifdef LDAP_OPT_PROTOCOL_VERSION

	if (authldaprc.protocol_version &&
	    !ok("ldap_set_option",
		ldap_set_option(connection,
				LDAP_OPT_PROTOCOL_VERSION,
				(void *) &authldaprc.protocol_version)))
	{
		disconnect();
		return false;
	}

	if (authldaprc.protocol_version)
	{
		DPRINTF("selected ldap protocol version %d", authldaprc.protocol_version);
	}
#endif

	if (authldaprc.tls && !enable_tls())
	{
		disconnect();
		return false;
	}

#ifdef LDAP_OPT_DEREF
	if (!ok("ldap_set_option",
		ldap_set_option(connection, LDAP_OPT_DEREF,
				(void *)&authldaprc.ldap_deref)))
	{
		disconnect();
		return (false);
	}
#else
	if (!deref_setting.empty())
	{
		courier_auth_err("authldaplib: LDAP_OPT_DEREF not available, ignored");
	}
#endif

	return true;
}

void ldap_connection::disconnect()
{
	close();
	ldapconnfailure();
}

void ldap_connection::close()
{
	if (connection == NULL)
		return;

	ldap_unbind_ext(connection, 0, 0);
	connection=NULL;
}

static int ldapopen()
{
	if (!main_connection.connected())
	{
		if (!main_connection.connect())
			return 1;
	}

	if (authldaprc.initbind && !main_connection.bound)
	{
		/* Bind to server */
		if (courier_authdebug_login_level >= 2)
		{
			DPRINTF("binding to LDAP server as DN '%s', password '%s'",
				authldaprc.ldap_binddn.empty() ? "<null>"
				: authldaprc.ldap_binddn.c_str(),
				authldaprc.ldap_bindpw.empty() ? "<null>"
				: authldaprc.ldap_bindpw.c_str());
		}
		else
		{
			DPRINTF("binding to LDAP server as DN '%s'",
				authldaprc.ldap_binddn.empty() ? "<null>"
				: authldaprc.ldap_binddn.c_str());
		}

		if (!main_connection.bind(authldaprc.ldap_binddn,
					  authldaprc.ldap_bindpw))
		{
			authldapclose();
			ldapconnfailure();
			return (-1);
		}
	}
	return (0);
}

class authldaprc_attributes {

public:

	std::map<std::string, std::vector<std::string *> > attributes;

	std::string attribute(const char *name,
			      const char *default_value,
			      std::string &return_value)
	{
		std::string value;

		authldaprc.config(name, value, false, default_value);

		if (!value.empty())
			attributes[value].push_back(&return_value);
		return value;
	}
};

class authldaprc_attribute_vector : public std::vector<std::string> {

public:

	authldaprc_attribute_vector(const std::map<std::string,
				    std::vector<std::string *> > &attributes)
	{
		for (std::map<std::string, std::vector<std::string *> >
			     ::const_iterator
			     p=attributes.begin(); p != attributes.end(); ++p)
		{
			push_back(p->first);
		}
	}
};

class authldaprc_search_attributes {

	std::vector<std::string> copy_buffer;

public:

	std::vector<char *> all_attributes_ptr;

	authldaprc_search_attributes(const std::vector<std::string> &attributes)
		: copy_buffer(attributes)
	{
		std::set<std::string> dupes;

		for (std::vector<std::string>::iterator
			     p=copy_buffer.begin();
		     p != copy_buffer.end(); ++p)
		{
			if (p->empty())
				continue;

			if (dupes.find(*p) != dupes.end())
				continue;
			dupes.insert(*p);
			p->push_back(0);
			all_attributes_ptr.push_back(& (*p)[0]);
		}

		all_attributes_ptr.push_back(0);
	}

	char **search_attributes()
	{
		return &all_attributes_ptr[0];
	}
};

class authldaprc_search_result : authldaprc_search_attributes {

public:

	LDAPMessage *ptr;
	bool finished;

	authldaprc_search_result(ldap_connection &conn,
				 const std::string &basedn,
				 const std::string &query,
				 const std::vector<std::string> &attributes,
				 const struct timeval &timeout)
		: authldaprc_search_attributes(attributes),
		  ptr(NULL), finished(false)
	{
		struct timeval timeout_copy=timeout;

		if (!conn.connect() ||
		    !conn.ok("ldap_search_ext_s",
			     ldap_search_ext_s(conn.connection,
					       basedn.c_str(),
					       LDAP_SCOPE_SUBTREE,
					       query.c_str(),
					       search_attributes(),
					       0,
					       NULL, NULL,
					       &timeout_copy,
					       100, &ptr)))
		{
			ptr=NULL;
			conn.disconnect();
			if (!conn.connect()
			    || !conn.ok("ldap_search_ext_s",
				     ldap_search_ext_s(conn.connection,
						       basedn.c_str(),
						       LDAP_SCOPE_SUBTREE,
						       query.c_str(),
						       search_attributes(),
						       0,
						       NULL, NULL,
						       &timeout_copy,
						       100, &ptr)))
			{
				ptr=NULL;
			}
		}
	}

	authldaprc_search_result(ldap_connection &conn,
				 int msgid,
				 bool all,
				 const struct timeval &timeout)
		:authldaprc_search_attributes(std::vector<std::string>()),
		 ptr(NULL), finished(false)
	{
		while (1)
		{
			struct timeval timeout_copy=timeout;

			int rc=ldap_result(conn.connection, msgid, all ? 1:0,
					   &timeout_copy,
					   &ptr);

			switch (rc)
			{
			case -1:
				DPRINTF("ldap_result() failed");
				ldap_msgfree(ptr);
				ptr=NULL;
				return;
			case 0:
				DPRINTF("ldap_result() timed out");
				ldap_msgfree(ptr);
				ptr=NULL;
				return;
			case LDAP_RES_SEARCH_ENTRY:
				break; /* deal with below */
			case LDAP_RES_SEARCH_RESULT:
				if (ldap_parse_result(conn.connection, ptr,
						      &rc,
						      NULL, NULL, NULL, NULL,
						      0) != LDAP_SUCCESS)
				{
					DPRINTF("ldap_parse_result failed");
					ldap_msgfree(ptr);
					ptr=NULL;
					return;
				}
				ldap_msgfree(ptr);
				ptr=NULL;
				if (rc != LDAP_SUCCESS)
				{
					DPRINTF("search failed: %s",
						ldap_err2string(rc));
					return;
				}
				finished=true;
				return;
			default:
				DPRINTF("ldap_result(): ignored 0x%02X status",
					rc);
				ldap_msgfree(ptr);
				ptr=NULL;
				continue;
			}
			break;
		}
	}

	bool operator!() const
	{
		return ptr == NULL;
	}

	operator bool() const
	{
		return ptr != NULL;
	}

	~authldaprc_search_result()
	{
		if (ptr)
			ldap_msgfree(ptr);
	}
};

static std::vector<std::string>
authldap_entry_values(LDAP *connection, LDAPMessage *msg,
		      const std::string &attrname)
{
	std::vector<std::string> values;

	struct berval **p=ldap_get_values_len(connection, msg,
					      attrname.c_str());

	if (p)
	{

		size_t n=ldap_count_values_len(p);

		values.reserve(n);

		for (size_t i=0; i<n; i++)
		{
			const char *ptr=
				reinterpret_cast<const char *>(p[i]->bv_val);

			values.push_back(std::string(ptr, ptr+p[i]->bv_len));
		}

		ldap_value_free_len(p);
	}
	return values;
}

class authldap_get_values {

	LDAP *connection;
	LDAPMessage *entry;
	std::string context;

public:

	authldap_get_values(LDAP *connectionArg,
			    LDAPMessage *entryArg,
			    const std::string &contextArg)
		: connection(connectionArg),
		  entry(entryArg),
		  context(contextArg)
	{
	}

	bool operator()(const std::string &attrname,
			std::string &value)
	{
		std::vector<std::string> values=
			authldap_entry_values(connection, entry, attrname);

		if (values.empty())
			return false;

		if (values.size() > 1)
		{
			fprintf(stderr,
				"WARN: authldaplib: duplicate attribute %s for %s\n",
				attrname.c_str(),
				context.c_str());
		}

		value=values[0];
		return true;
	}

	bool operator()(const std::string &attrname,
			const std::vector<std::string *> &values)
	{
		bool found=true;

		for (std::vector<std::string *>::const_iterator
			     b=values.begin();
		     b != values.end(); ++b)
		{
			found=operator()(attrname, **b);
		}

		return found;
	}

	std::string options()
	{
		size_t i;

		std::ostringstream options;
		const char *options_sep="";

		for (i=0; i<authldaprc.auxoptions.size(); ++i)
		{
			std::string value;

			if (operator()(authldaprc.auxoptions[i], value)
			    && !value.empty())
			{
				options << options_sep
					<< authldaprc.auxnames[i]
					<< "="
					<< value;
				options_sep=",";
			}
		}
		return options.str();
	}
};

static void cpp_auth_ldap_enumerate( void(*cb_func)(const char *name,
						    uid_t uid,
						    gid_t gid,
						    const char *homedir,
						    const char *maildir,
						    const char *options,
						    void *void_arg),
				     void *void_arg)
{
	int msgid;

	if (ldapopen())
	{
		(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
		return;
	}

	std::string mail_field, uid_field, gid_field,
		homedir_field, maildir_field;

	authldaprc.config("LDAP_MAIL", mail_field, false, "mail");
	authldaprc.config("LDAP_UID", uid_field, false);
	authldaprc.config("LDAP_GID", gid_field, false);
	authldaprc.config("LDAP_HOMEDIR", homedir_field, false, "homeDir");
	authldaprc.config("LDAP_MAILDIR", maildir_field, false);

	std::vector<std::string> attribute_vector;

	attribute_vector.push_back(mail_field);
	attribute_vector.push_back(uid_field);
	attribute_vector.push_back(gid_field);
	attribute_vector.push_back(homedir_field);
	attribute_vector.push_back(maildir_field);

	for (size_t i=0; i<authldaprc.auxoptions.size(); i++)
	{
		attribute_vector.push_back(authldaprc.auxoptions[i]);
	}

	std::string enumerate_filter;

	authldaprc.config("LDAP_ENUMERATE_FILTER", enumerate_filter, false);

	if (enumerate_filter.empty())
	{
		std::string filter;

		authldaprc.config("LDAP_FILTER", filter, false);

		if (!filter.empty())
		{
			enumerate_filter=filter;
		}
		else
		{
			std::string s;

			authldaprc.config("LDAP_MAIL", s, false);

			enumerate_filter = s + "=*";
		}
	}

	DPRINTF("ldap_search: basedn='%s', filter='%s'",
		authldaprc.ldap_basedn.c_str(), enumerate_filter.c_str());

	authldaprc_search_attributes search_attributes(attribute_vector);

	struct timeval tv;
	tv.tv_sec=60*60;
	tv.tv_usec=0;

	if (ldap_search_ext(main_connection.connection,
			    authldaprc.ldap_basedn.c_str(),
			    LDAP_SCOPE_SUBTREE,
			    enumerate_filter.c_str(),
			    search_attributes.search_attributes(),
			    0,
			    NULL, NULL, &tv, 1000000, &msgid) < 0)
	{
		DPRINTF("ldap_search_ext failed");
		return;
	}

	/* Process results */

	while (1)
	{
		struct timeval timeout;
		LDAPMessage *entry;

		timeout.tv_sec=authldaprc.timeout;
		timeout.tv_usec=0;

		authldaprc_search_result search_result(main_connection,
						       msgid,
						       false,
						       timeout);

		if (search_result.finished)
			break;

		if (!search_result)
			return;

		entry = ldap_first_entry(main_connection.connection, search_result.ptr);

		while (entry)
		{
			std::vector<std::string>
				names=authldap_entry_values(main_connection.connection,
							    entry,
							    mail_field);

			if (names.empty())
			{
				entry = ldap_next_entry(main_connection.connection, entry);
				continue;
			}

			size_t n=names.size();

			if (n > 0)
			{
				std::string uid_s, gid_s, homedir, maildir;
				uid_t uid=authldaprc.uid;
				gid_t gid=authldaprc.gid;

				authldap_get_values
					get_value(main_connection.connection, entry,
						  names[0]);

				if (!uid_field.empty())
					get_value(uid_field, uid_s);

				if (!gid_field.empty())
				{
					get_value(gid_field, gid_s);
				}

				get_value(homedir_field, homedir);
				get_value(maildir_field, maildir);

				if (!uid_s.empty())
					std::istringstream(uid_s)
						>> uid;

				if (!gid_s.empty())
					std::istringstream(gid_s)
						>> gid;

				std::string options=get_value.options();

				for (size_t j=0; j < n; j++)
				{
					if (!homedir.empty())
						(*cb_func)(names[j].c_str(),
							   uid, gid,
							   homedir.c_str(),
							   maildir.empty()
							   ? 0:maildir.c_str(),
							   options.empty()
							   ? 0:options.c_str(),
							   void_arg);
				}
			}

			entry = ldap_next_entry(main_connection.connection, entry);
		}
	}

	/* Success */
	(*cb_func)(NULL, 0, 0, NULL, NULL, NULL, void_arg);
}

static void cpp_authldapclose()
{
	main_connection.disconnect();
	bind_connection.disconnect();
}

class authldap_lookup : private authldaprc_attributes {

	struct authinfo auth;
	const char *service;
	std::string attrname;
	std::string user;
	const char *pass;
	const char *newpass;
	const char *authaddr;

public:
	authldap_lookup(const char *serviceArg,
			const std::string &attrnameArg,
			const std::string &userArg,
			const char *passArg,
			const char *newpassArg,
			const char *authaddrArg);

	int operator()(int (*callback)(struct authinfo *, void *), void *arg);

private:
	int verify_password(const std::string &dn);
	int verify_password_myself(const std::string &dn);
	int verify_password_authbind(const std::string &dn);
};

authldap_lookup::authldap_lookup(const char *serviceArg,
				 const std::string &attrnameArg,
				 const std::string &userArg,
				 const char *passArg,
				 const char *newpassArg,
				 const char *authaddrArg)
	: service(serviceArg),
	  attrname(attrnameArg),
	  user(userArg),
	  pass(passArg),
	  newpass(newpassArg),
	  authaddr(authaddrArg)
{
}

int authldap_lookup::operator()(int (*callback)(struct authinfo *, void *),
				void *arg)
{
	struct timeval timeout;

	LDAPMessage *entry;

	std::string dn;

	std::string homeDir, mailDir, userPassword,
		cryptPassword,
		cn,
		uidNumber,
		gidNumber, quota;

	uid_t au;
	gid_t ag;
	int rc;

	std::ostringstream query;

	std::string filter;

	authldaprc.config("LDAP_FILTER", filter, false);

	if (!filter.empty())
	{
		query << "(&" << filter;
	}

	query << "(" << attrname << "=" << user;

	std::string domain;

	authldaprc.config("LDAP_DOMAIN", domain, false);

	if (!domain.empty() &&
	     std::find(user.begin(), user.end(), '@') == user.end())
		query << "@" << domain;
	query << ")";

	if (!filter.empty())
		query << ")";

	std::string query_str=query.str();

	DPRINTF("Query: %s", query_str.c_str());

	timeout.tv_sec=authldaprc.timeout;
	timeout.tv_usec=0;

	attribute("LDAP_HOMEDIR", "homeDir", homeDir);
	attribute(service && strcmp(service, "courier") == 0
		  ? "LDAP_DEFAULTDELIVERY":"LDAP_MAILDIR", 0, mailDir);
	attribute("LDAP_FULLNAME", 0, cn);
	std::string clearpw_value=attribute("LDAP_CLEARPW", 0, userPassword);
	std::string cryptpw_value=attribute("LDAP_CRYPTPW", 0, cryptPassword);
	attribute("LDAP_UID", 0, uidNumber);
	attribute("LDAP_GID", 0, gidNumber);
	attribute("LDAP_MAILDIRQUOTA", 0, quota);

	authldaprc_attribute_vector all_attributes(attributes);

	for (size_t i=0; i<authldaprc.auxoptions.size(); i++)
	{
		all_attributes.push_back(authldaprc.auxoptions[i]);
	}

	authldaprc_search_result result(main_connection,
					authldaprc.ldap_basedn,
					query_str,
					all_attributes,
					timeout);

	int n_entries=ldap_count_entries(main_connection.connection, result.ptr);

	if (n_entries != 1)
	{
		if (n_entries == 0)
		{
			DPRINTF("Not found");
		}
		else
		{
			DPRINTF("Returned multiple entries (%d)", n_entries);
		}
		return -1;
	}

	char *dn_p = ldap_get_dn(main_connection.connection, result.ptr);

	DPRINTF("Returned DN: %s", dn_p ? dn_p : "<null>");

	if (dn_p == NULL)
	{
		DPRINTF("ldap_get_dn failed");
		return -1;
	}

	dn=dn_p;
	free(dn_p);

	/* Get the pointer on this result */
	entry=ldap_first_entry(main_connection.connection, result.ptr);
	if (entry==NULL)
	{
		DPRINTF("ldap_first_entry failed");
		return -1;
	}

	/* print all the raw attributes */
	if (courier_authdebug_login_level >= 2)
	{
		BerElement *berptr = 0;
		char *attr=
			ldap_first_attribute(main_connection.connection, entry, &berptr);

		while (attr)
		{
			std::vector<std::string>
				values=authldap_entry_values(main_connection.connection,
							     entry,
							     attr);
			for (size_t i=0; i<values.size(); ++i)
				DPRINTF("    %s: %s", attr, values[i].c_str());

			ldap_memfree(attr);
			attr = ldap_next_attribute(main_connection.connection, entry,
						   berptr);
		}

		ber_free(berptr, 0);
	}

	authldap_get_values get_value(main_connection.connection, entry, dn.c_str());

	for (std::map<std::string, std::vector<std::string *> >::iterator
		     p=attributes.begin(); p != attributes.end(); ++p)
	{
		get_value(p->first, p->second);
	}

	au=authldaprc.uid;
	ag=authldaprc.gid;
	if (!uidNumber.empty())
	{
		std::istringstream(uidNumber) >> au;
	}

	if (!gidNumber.empty())
	{
		std::istringstream(gidNumber) >> ag;
	}

	std::string mailroot;

	authldaprc.config("LDAP_MAILROOT", mailroot, false);

	if (!homeDir.empty() && !mailroot.empty())
	{
		homeDir = mailroot + "/" + homeDir;
	}

	std::string options=get_value.options();

	memset(&auth, 0, sizeof(auth));

	auth.sysuserid= &au;
	auth.sysgroupid= ag;
	auth.homedir=homeDir.c_str();
	auth.address=authaddr;
	auth.fullname=cn.c_str();

	if (!mailDir.empty())
		auth.maildir=mailDir.c_str();

	if (!userPassword.empty())
		auth.clearpasswd=userPassword.c_str();

	if (!cryptPassword.empty())
		auth.passwd=cryptPassword.c_str();

	if (!quota.empty())
		auth.quota=quota.c_str();

	if (!options.empty())
		auth.options=options.c_str();

	rc=0;

	if (au == 0 || ag == 0)
	{
		courier_auth_err("authldaplib: refuse to authenticate %s: uid=%d, gid=%d (zero uid or gid not permitted)",
				 user.c_str(), au, ag);
		rc= 1;
	}

	courier_authdebug_authinfo("DEBUG: authldaplib: ", &auth,
				   userPassword.c_str(),
				   cryptPassword.c_str());

	if (rc == 0)
		rc=verify_password(dn);

	std::string newpass_crypt;

	if (rc == 0 && newpass)
	{
		char *p=authcryptpasswd(newpass, auth.passwd);

		if (!p)
			rc=-1;
		else
		{
			newpass_crypt=p;
			free(p);
		}

		LDAPMod *mods[3];
		int mod_index=0;

		LDAPMod mod_clear, mod_crypt;
		char *mod_clear_vals[2], *mod_crypt_vals[2];

		std::vector<char> clearpw_buffer, cryptpw_buffer;

		if (!clearpw_value.empty() && auth.clearpasswd)
		{
			clearpw_buffer.insert(clearpw_buffer.end(),
					      clearpw_value.begin(),
					      clearpw_value.end());

			clearpw_buffer.push_back(0);

			mods[mod_index]= &mod_clear;
			mod_clear.mod_op=LDAP_MOD_REPLACE;
			mod_clear.mod_type=&clearpw_buffer[0];
			mod_clear.mod_values=mod_clear_vals;

			DPRINTF("Modify: %s", mod_clear.mod_type);

			mod_clear_vals[0]=(char *)newpass;
			mod_clear_vals[1]=NULL;
			++mod_index;
		}

		if (!cryptpw_value.empty() &&
		    !newpass_crypt.empty() && auth.passwd)
		{
			cryptpw_buffer.insert(cryptpw_buffer.end(),
					      cryptpw_value.begin(),
					      cryptpw_value.end());

			cryptpw_buffer.push_back(0);

			mods[mod_index]= &mod_crypt;
			mod_crypt.mod_op=LDAP_MOD_REPLACE;
			mod_crypt.mod_type=&cryptpw_buffer[0];
			mod_crypt.mod_values=mod_crypt_vals;

			DPRINTF("Modify: %s", mod_crypt.mod_type);

			newpass_crypt.push_back(0);
			mod_crypt_vals[0]=&newpass_crypt[0];
			mod_crypt_vals[1]=NULL;
			++mod_index;
		}
		if (mod_index == 0)
			rc= -1;
		else
		{
			mods[mod_index]=0;

			/*
			** Use the user connection for updating the password.
			*/

			int ld_errno=
				ldap_modify_ext_s(bind_connection.connected()
						  ? bind_connection.connection
						  : main_connection.connection,
						  dn.c_str(), mods,
						  NULL, NULL);

			if (ld_errno != LDAP_SUCCESS)
			{
				rc= -1;
				DPRINTF("Password update failed: %s",
					ldap_err2string(ld_errno));
			}
		}
	}

	if (rc == 0 && callback)
	{
		if (!auth.clearpasswd)
			auth.clearpasswd=pass;
		rc= (*callback)(&auth, arg);
	}

	return (rc);
}

int authldap_lookup::verify_password(const std::string &dn)
{
	if (!pass)
		return 0;

	if (authldaprc.authbind)
		return verify_password_authbind(dn);

	return verify_password_myself(dn);
}

int authldap_lookup::verify_password_authbind(const std::string &dn)
{
	if (!bind_connection.connect())
		return 1;

	if (!bind_connection.bind(dn, pass))
	{
		bind_connection.close();
		return -1;
	}

	if (authldaprc.protocol_version == 2)
	{
		// protocol version 2 does not allow rebinds.

		bind_connection.close();
	}

	return 0;
}

int authldap_lookup::verify_password_myself(const std::string &dn)
{
	if (auth.clearpasswd)
	{
		if (strcmp(pass, auth.clearpasswd))
		{
			if (courier_authdebug_login_level >= 2)
			{
				DPRINTF("Password for %s: '%s' does not match clearpasswd '%s'",
					dn.c_str(), pass, auth.clearpasswd);
			}
			else
			{
				DPRINTF("Password for %s does not match",
					dn.c_str());
			}
			return -1;
		}
	}
	else
	{
		const char *p=auth.passwd;

		if (!p)
		{
			DPRINTF("Missing password in LDAP!");
			return -1;
		}

		if (authcheckpassword(pass, p))
		{
			DPRINTF("Password for %s does not match",
				dn.c_str());
			return -1;
		}
        }

	return 0;
}

/*
** Replace keywords in the emailmap string.
*/

static std::string emailmap_replace(const char *str,
				    const std::string &user_value,
				    const std::string &realm_value)
{
	std::ostringstream o;

	while (*str)
	{
		const char *p=str;

		while (*str && *str != '@')
			++str;

		o << std::string(p, str);

		if (*str != '@')
			continue;

		++str;

		p=str;
		while (*str && *str != '@')
			++str;

		std::string key(p, str);

		if (*str)
			++str;

		if (key == "user")
			o << user_value;
		else if (key == "realm")
			o << realm_value;
	}

	return o.str();
}

static int auth_ldap_try(const char *service,
			 const char *unquoted_user, const char *pass,
			 int (*callback)(struct authinfo *, void *),
			 void *arg, const char *newpass)
{
	std::string user;

	{
		char *q=courier_auth_ldap_escape(unquoted_user);

		user=q;
		free(q);
	}

	if (ldapopen())	return (-1);

	std::string::iterator at=std::find(user.begin(), user.end(), '@');

	std::string emailmap;

	authldaprc.config("LDAP_EMAILMAP", emailmap, false);

	std::string mail;

	if (!authldaprc.config("LDAP_MAIL", mail, false, "mail"))
		return -1;

	if (emailmap.empty() || at == user.end())
	{
		authldap_lookup real_lookup(service, mail, user, pass,
					    newpass, user.c_str());

		return real_lookup(callback, arg);
	}

	std::string user_value(user.begin(), at);
	std::string realm_value(++at, user.end());

	std::string query=
		emailmap_replace(emailmap.c_str(), user_value, realm_value);

	DPRINTF("using emailmap search: %s", query.c_str());

	struct timeval tv;

	tv.tv_sec=authldaprc.timeout;
	tv.tv_usec=0;

	std::vector<std::string> attributes;

	attributes.push_back("");

	authldaprc.config("LDAP_EMAILMAP_ATTRIBUTE", attributes[0], false);

	if (attributes[0].empty())
		attributes[0]="handle";

	std::string basedn;

	if (!authldaprc.config("LDAP_EMAILMAP_BASEDN", basedn, true))
		return -1;

	authldaprc_search_result
		lookup(main_connection,
		       basedn,
		       query,
		       attributes,
		       tv);

	if (!lookup)
	{
		if (main_connection.connection)	return (-1);
		return (1);
	}

	int cnt=ldap_count_entries(main_connection.connection, lookup.ptr);

	if (cnt != 1)
	{
		courier_auth_err("emailmap: %d entries returned from search %s (but we need exactly 1)",
		    cnt, query.c_str());
		return -1;
	}

	LDAPMessage *entry=ldap_first_entry(main_connection.connection, lookup.ptr);

	if (!entry)
	{
		courier_auth_err("authldap: unexpected NULL from ldap_first_entry");
		return -1;
	}

	authldap_get_values get_value(main_connection.connection, entry, query);

	std::string v;

	get_value(attributes[0], v);

	if (v.empty())
	{
		DPRINTF("emailmap: empty attribute");
		return (-1);
	}


	std::string attrname;

	authldaprc.config("LDAP_EMAILMAP_MAIL", attrname, false);

	if (attrname.empty())
	{
		attrname=mail;
	}

	DPRINTF("emailmap: attribute=%s, value=%s", attrname.c_str(),
		v.c_str());

	authldap_lookup real_lookup(service, attrname, v.c_str(), pass,
				    newpass, user.c_str());

	return real_lookup(callback, arg);
}

// Try the query once. If there's a connection-level error, try again.

static int auth_ldap_retry(const char *service,
			   const char *unquoted_user,
			   const char *pass,
			   int (*callback)(struct authinfo *, void *),
			   void *arg, const char *newpass)
{
	int rc=auth_ldap_try(service, unquoted_user, pass, callback, arg,
			     newpass);

	if (rc > 0)
		rc=auth_ldap_try(service, unquoted_user, pass, callback, arg,
				   newpass);

	return rc;
}

extern "C" {

#if 0
};
#endif

int auth_ldap_changepw(const char *dummy, const char *user,
		       const char *pass,
		       const char *newpass)
{
	if (!authldaprc.load())
		return 1;

	return auth_ldap_retry("authlib", user, pass, NULL, NULL, newpass);
}

int authldapcommon(const char *service,
		   const char *user, const char *pass,
		   int (*callback)(struct authinfo *, void *),
		   void *arg)
{
	if (!authldaprc.load())
		return 1;
	return (auth_ldap_retry(service, user, pass, callback, arg, NULL));
}

void authldapclose()
{
	cpp_authldapclose();
}

void auth_ldap_enumerate( void(*cb_func)(const char *name,
					 uid_t uid,
					 gid_t gid,
					 const char *homedir,
					 const char *maildir,
					 const char *options,
					 void *void_arg),
			   void *void_arg)
{
	if (!authldaprc.load())
		return;

	cpp_auth_ldap_enumerate(cb_func, void_arg);
}

#if 0
{
#endif
}
