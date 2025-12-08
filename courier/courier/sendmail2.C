/*
** Copyright 2025 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	"rfc822/rfc2047.h"
#include	<sys/types.h>
#include	<pwd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<string>
#include	<idn2.h>
#include	<algorithm>

#ifndef ENVIRON_SOURCE
#define ENVIRON_SOURCE getenv
#endif

#ifndef MYPWD
struct passwd *mypwd()
{
static struct passwd *pwd=0;
int	first_pwd=1;

	if (first_pwd)
	{
		first_pwd=0;
		pwd=getpwuid(getuid());
	}

	return (pwd);
}
#endif

static const char *env(const char *envname)
{
	const char *p=ENVIRON_SOURCE(envname);

	if (p && !*p)	p=0;
	return (p);
}

std::string rewrite_env_sender(const char *from)
{
const char *host;

	if (!from)
	{
		if ((from=env("MAILSUSER")) == 0 &&
			(from=env("MAILUSER")) == 0 &&
			(from=env("LOGNAME")) == 0 &&
			(from=env("USER")) == 0)
		{
		struct passwd *pw=mypwd();

			from=pw ? pw->pw_name:"nobody";
		}

		if ((host=env("MAILSHOST")) != 0 ||
			(host=env("MAILHOST")) != 0)
		{
			std::string ret;

			ret.reserve(strlen(from)+strlen(host)+1);

			ret = from;
			ret += '@';
			ret += host;

			auto d=udomainutf8(ret.c_str());
			ret=d;
			free(d);
			return ret;
		}
	}

	return from;
}

static std::string rewrite_from(const char *, const char *, const char *,
				const std::string &);

static rfc822::tokens tokenize_name(std::string_view name,
				    std::string &encoded_namebuf)
{
	if (std::find_if(name.begin(), name.end(),
			 []
			 (auto &c)
			 {
				 return (c & 0x80) || c == '"' || c == '\\';
			 }) != name.end())
	{
		auto [s, error] = rfc2047::encode(
			name.begin(),
			name.end(),
			unicode_default_chset(),
			rfc2047_qp_allow_word
		);

		encoded_namebuf=std::move(s);

		return rfc822::tokens{encoded_namebuf};
	}

	rfc822::tokens tokens{name};

	for (auto &t:tokens)
	{
		if (t.type)
		{
			tokens.resize(1);
			tokens[0].type='"';
			tokens[0].str=name;
			break;
		}
	}

	return tokens;
}

static std::string get_gecos()
{
	struct passwd *pw=mypwd();

	if (!pw || !pw->pw_gecos)	return "";

	std::string p=pw->pw_gecos;

	auto i=p.find(',');

	if (i < p.size())
		p=p.substr(0, i);

	return (p);
}

void rewrite_headers(const char *From,
		     FILE *read_from,
		     FILE *submit_to)
{
	int	seen_from=0;
	char	headerbuf[5000];
	int	c;
	size_t  i;
	const char *mailuser, *mailuser2, *mailhost;

	std::string pfrom;

	if (From)
		pfrom=From;

	if ((mailuser=env("MAILUSER")) == 0 &&
		(mailuser=env("LOGNAME")) == 0)
		mailuser=env("USER");
	mailuser2=env("MAILUSER");
	mailhost=env("MAILHOST");

	while (fgets(headerbuf, sizeof(headerbuf), read_from))
	{
	char	*p=strchr(headerbuf, '\n');

		if (p)
		{
			*p=0;
			if (p == headerbuf || strcmp(headerbuf, "\r") == 0)
				break;
		}

		if (strncasecmp(headerbuf, "from:", 5))
		{
			fprintf(submit_to, "%s", headerbuf);
			if (!p)
				while ((c=getc(read_from)) != EOF && c != '\n')
					putc(c, submit_to);
			putc('\n', submit_to);
			continue;
		}
		if (!p)
			while ((c=getc(read_from)) != EOF && c != '\n')
				;	/* I don't care */
		if (seen_from)	continue;	/* Screwit */
		seen_from=1;

		i=strlen(headerbuf);
		for (;;)
		{
			c=getc(read_from);
			if (c != EOF)	ungetc(c, read_from);
			if (c == EOF || c == '\r' || c == '\n')	break;
			if (!isspace((int)(unsigned char)c))	break;
			while ((c=getc(read_from)) != EOF && c != '\n')
			{
				if (i < sizeof(headerbuf)-1)
					headerbuf[i++]=c;
			}
			headerbuf[i]=0;
		}

		auto s=rewrite_from(headerbuf+5, mailuser2, mailhost, pfrom);
		fprintf(submit_to, "From: %s\n", s.c_str());
	}
	if (!seen_from)
	{
		if (!mailuser)
		{
		struct passwd *pw=mypwd();

			mailuser=pw ? pw->pw_name:"nobody";
		}

		if (pfrom.empty())
		{
			if ( !(From=env("MAILNAME")) && !(From=env("NAME")))
			{
				pfrom=get_gecos();
			}
			else	pfrom=From;
		}

		auto s=rewrite_from(NULL, mailuser, mailhost, pfrom);
		fprintf(submit_to, "From: %s\n", s.c_str());
	}
	putc('\n', submit_to);
}

static std::string rewrite_from_idna(const char *oldfrom, const char *newuser,
				     const char *newhost, std::string newname);

static std::string rewrite_from(const char *oldfrom, const char *newuser,
				const char *newhost,
				const std::string &newname)
{
	char *newhost_idna=0;

	if (newhost && idna_to_ascii_8z(newhost, &newhost_idna, 0)
	    != IDNA_SUCCESS)
	{
		if (newhost_idna)
			free(newhost_idna);
		newhost_idna=0;
	}

	auto p=rewrite_from_idna(oldfrom, newuser,
				 (newhost_idna ? newhost_idna:newhost),
				 newname);

	if (newhost_idna)
		free(newhost_idna);

	return p;
}

static std::string rewrite_from_idna(const char *oldfrom, const char *newuser,
				     const char *newhost,
				     std::string newname)
{
	if (!oldfrom)
	{
		std::string p= "<";

		if (newuser)
			p += newuser;
		if (newuser && newhost)
		{
			p += "@";
			p += newhost;
		}
		p += ">";

		if (!newname.empty())
		{
			std::string encoded_namebuf;
			auto namet=tokenize_name(newname, encoded_namebuf);

			std::string s;

			s.reserve(rfc822::tokens::print(namet.begin(),
							namet.end(),
							rfc822::length_counter{}
				  ) + p.size() + 1);
			rfc822::tokens::print(namet.begin(),
					      namet.end(),
					      std::back_inserter(s));
			s += " ";
			s += p;

			p=s;
		}
		return (p);
	}

	rfc822::tokens rfct{oldfrom};
	rfc822::addresses rfca{rfct};

	const char *q;

	if ((q=env("MAILNAME")) || (q=env("NAME")))
		newname=q;

	if (newname.empty() && rfca.empty())
		newname=get_gecos();

	if ((rfca.empty() || rfca[0].address.empty()) && newuser == 0)
	{
		struct	passwd *pw=mypwd();

		if (pw)	newuser=pw->pw_name;
	}

	std::string encoded_namebuf;

	auto namet=tokenize_name(newname, encoded_namebuf);
	auto usert=rfc822::tokens{newuser ? newuser:""};
	auto hostt=rfc822::tokens{newhost ? newhost:""};

	if (rfca.empty() || rfca[0].address.empty())
	{
		if (!hostt.empty())
		{
			usert.push_back({'@',"@"});
			usert.insert(usert.end(),
				     hostt.begin(), hostt.end());
		}

		rfca.clear();
		rfca.push_back({std::move(namet), std::move(usert)});
	}
	else
	{
		rfca.resize(1);

		if (!usert.empty())
		{
			rfca[0].address.erase(
				rfca[0].address.begin(),
				std::find_if(
					rfca[0].address.begin(),
					rfca[0].address.end(),
					[]
					(auto &t)
					{
						return t.type == '@';
					}));

			rfca[0].address.insert(
				rfca[0].address.begin(),
				usert.begin(), usert.end());
		}

		if (!hostt.empty() && !rfca[0].address.empty())
		{
			rfca[0].address.erase(
				std::find_if(
					rfca[0].address.begin(),
					rfca[0].address.end(),
					[]
					(auto &t)
					{
						return t.type == '@';
					}),
				rfca[0].address.end());

			rfca[0].address.push_back({'@', "@"});
			rfca[0].address.insert(
				rfca[0].address.end(),
				hostt.begin(),
				hostt.end());
		}
		if (!namet.empty())
			rfca[0].name=namet;
	}

	std::string s;

	s.reserve(rfca.print( rfc822::length_counter{}));
	rfca.print(std::back_inserter(s));

	return s;
}
