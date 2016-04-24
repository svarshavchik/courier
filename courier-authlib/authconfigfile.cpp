/*
** Copyright 2016 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if HAVE_CONFIG_H
#include "courier_auth_config.h"
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

#include <algorithm>
#include <fstream>
#include <sstream>

#include "authconfigfile.h"

courier::auth::config_file::config_file(const char *filenameArg)
	: filename(filenameArg), loaded(false)
{
}

bool courier::auth::config_file::load(bool reload)
{
	struct stat stat_buf;

	if (stat(filename, &stat_buf) < 0)
	{
		courier_auth_err("stat(%s) failed", filename);
		return false;
	}

	if (loaded)
	{
		if (stat_buf.st_mtime != config_timestamp)
			do_reload();
		return true;
	}

	loaded=open_and_load_file(reload);

	if (loaded)
		config_timestamp=stat_buf.st_mtime;
	return loaded;
}

class courier::auth::config_file::isspace {

public:

	bool operator()(char c)
	{
		return std::isspace(c);
	}
};

class courier::auth::config_file::not_isspace : public isspace {

public:

	bool operator()(char c)
	{
		return !isspace::operator()(c);
	}
};


bool courier::auth::config_file::open_and_load_file(bool reload)
{
	std::ifstream f(filename);

	if (!f.is_open())
	{
		courier_auth_err("Cannot open %s", filename);

		return false;
	}

	std::string s;

	bool seen_marker=false;

	while (s.clear(), !std::getline(f, s).eof() || !s.empty())
	{
		std::string::iterator e=s.end();

		std::string::iterator p=
			std::find_if(s.begin(), e, not_isspace());

		if (p == s.end() || *p == '#')
		{
			static const char marker[]="##NAME: MARKER:";

			if (s.substr(0, sizeof(marker)-1) == marker)
				seen_marker=true;
			continue;
		}

		std::string::iterator q=std::find_if(p, e, isspace());

		std::string name(p, q);
		std::string setting;

		while (1)
		{
			q=std::find_if(q, e, not_isspace());

			while (q != e && isspace()(e[-1]))
				--e;

			if (q == e)
				break;

			bool continuing=false;

			if (e[-1] == '\\')
			{
				continuing=true;
				e[-1]=' ';
			}

			setting.insert(setting.end(), q, e);

			if (!continuing)
				break;

			std::getline(f, s);

			q=s.begin();
			e=s.end();
		}

		parsed_config.insert(std::make_pair(name, setting));
	}

	if (!seen_marker)
	{
		courier_auth_err((reload
				  ? "marker line not found in %s will try again later"
				  : "marker line not found in %s (probably forgot to run sysconftool after an upgrade)"), filename);
		return false;
	}

	return do_load();
}

bool courier::auth::config_file::getconfig(const char *name,
					   std::string &value,
					   bool required,
					   const char *default_value) const
{
	std::map<std::string, std::string>::const_iterator
		iter=parsed_config.find(name);

	if (iter != parsed_config.end())
	{
		value=iter->second;
		return true;
	}

	if (required)
	{
		courier_auth_err("%s not found in %s",
				 name, filename);
		return false;
	}

	value.clear();
	if (default_value)
		value=default_value;
	return true;
}

template<>
bool courier::auth::config_file::config(const char *name,
					std::string &value,
					bool required,
					const char *default_value) const
{
	return getconfig(name, value, required, default_value);
}

std::string courier::auth::config_file::config(const char *name) const
{
	return config(name, 0);
}

std::string courier::auth::config_file::config(const char *name,
					       const char *default_value) const
{
	std::string retval;

	config(name, retval, false, default_value);

	return retval;
}

std::string
courier::auth::config_file::expand_string(const std::string &s,
					  const std::map<std::string,
					  std::string> &parameters)
{
	std::ostringstream o;

	std::string::const_iterator b=s.begin(), e=s.end(), p;
	std::map<std::string, std::string>::const_iterator p_iter;

	while (b != e)
	{
		p=std::find(b, e, '$');

		o << std::string(b, p);

		b=p;

		if (b == e)
			continue;

		if (*++b != '(')
		{
			o << '$';
			continue;
		}

		p=std::find(++b, e, ')');

		p_iter=parameters.find(std::string(b, p));
		b=p;
		if (b != e)
			++b;

		if (p_iter != parameters.end())
			o << p_iter->second;
	}
	return o.str();
}

std::string
courier::auth::config_file::parse_custom_query(const std::string &s,
					       const std::string &login,
					       const std::string &defdomain,
					       std::map<std::string,
					       std::string> &parameters)
{

	std::string::const_iterator b=login.begin(),
		e=login.end(),
		p=std::find(b, e, '@');

	parameters["local_part"]=std::string(b, p);
	parameters["domain"]=p == e ? defdomain:std::string(p+1, e);

	return expand_string(s, parameters);
}
