/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"liblock/config.h"
#include	"liblock/liblock.h"
#include	"rfc822/rfc2047.h"
#include	<courier-unicode.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<string.h>
#include	<langinfo.h>
#include	<fstream>

#include	<locale.h>
#include	<langinfo.h>


ExclusiveLock::ExclusiveLock(const char *f) : fd(open(f, O_RDWR|O_CREAT, 0600))
{
	if (fd < 0)	return;

	ll_lock_ex(fd);
}

SharedLock::SharedLock(const char *f) : fd(open(f, O_RDWR|O_CREAT, 0600))
{
	if (fd < 0)	return;

	if (ll_lockfd(fd, ll_readlock|ll_whence_start|ll_wait, 0, 0) < 0)
		ll_lock_ex(fd);
		// Might be because shared locking is not supported

}

SharedLock::~SharedLock()
{
	if (fd >= 0)	close(fd);
}

ExclusiveLock::~ExclusiveLock()
{
	if (fd >= 0)	close(fd);
}

SubSharedLock::SubSharedLock() : SharedLock(SUBLOCKFILE)
{
}

SubSharedLock::~SubSharedLock()
{
}

SubExclusiveLock::SubExclusiveLock() : ExclusiveLock(SUBLOCKFILE)
{
}

SubExclusiveLock::~SubExclusiveLock()
{
}

CommandLock::CommandLock() : ExclusiveLock(CMDLOCKFILE)
{
}

CommandLock::~CommandLock()
{
}

std::string cmdget_s(const char *name)
{
	std::ifstream ifs(OPTIONS);
	std::string buf;

	if (ifs.is_open())
		while (std::getline(ifs, buf).good())
		{
			std::string::iterator b=buf.begin();
			std::string::iterator e=buf.end();

			std::string::iterator p=std::find(b, e, '=');

			if (std::string(b, p) == name)
			{
				if (p != e)
					++p;

				return std::string(p, e);
			}
		}

	return "";
}


std::string get_verp_return(std::string pfix)
{
	std::string n=cmdget_s("ADDRESS");
	std::string::iterator b=n.begin(), e=n.end();

	while (b != e)
	{
		if (*--e == '@')
			return std::string(b, e) + "-" + pfix +
				std::string( e,n.end());
	}

	return "";
}

int updatelistheaders()
{
	std::string addr=cmdget_s("ADDRESS");

	std::string url=cmdget_s("URL");

	if (addr.size() > 0)
	{
		std::ofstream ofs(HEADERADD ".new");

		// Preserve existing non-List headers.

		std::ifstream ifs(HEADERADD);

		if (ifs.is_open())
		{
			std::string line;

			while (std::getline(ifs, line).good())
			{
				std::string::iterator
					b=line.begin(), e=line.end();

				std::string p(b, std::find(b, e, ':'));

				std::transform(p.begin(), p.end(),
					       p.begin(),
					       std::ptr_fun(::tolower));

				if (p.substr(0, 5) == "list-")
					continue;

				ofs << line << std::endl;
			}
		}

		ofs << "List-Subscribe: <mailto:" <<
			get_verp_return("subscribe") << ">";

		if (url.size() > 0)
			ofs << ", <" << url << ">";

		ofs << std::endl;

		ofs << "List-Unsubscribe: <mailto:" <<
			get_verp_return("unsubscribe") << ">";

		if (url.size() > 0)
			ofs << ", <" << url << ">";

		ofs << std::endl;

		ofs << "List-Post: <mailto:" <<
			cmdget_s("ADDRESS") << ">" << std::endl
			<< "List-Owner: <mailto:" <<
			get_verp_return("owner") << ">" << std::endl
			<< "List-Help: <mailto:" <<
			get_verp_return("help") << ">" << std::endl;

		ofs.flush();

		if (ofs.fail())
		{
			perror(HEADERADD);
			return 1;
		}
		ofs.close();
		if (rename(HEADERADD ".new", HEADERADD))
			return 1;
	}
	return 0;
}

// Set mailing list options

int cmdset(const std::vector<std::string> &argv, bool autoencode)
{
	ExclusiveLock set_lock(OPTIONS ".lock");

	std::ofstream ofs(OPTIONS ".new");
	std::ifstream ifs(OPTIONS);

	if (!ofs.is_open() || !ifs.is_open())
	{
		perror(OPTIONS);
		return (1);
	}

	size_t i;

#if HAVE_SETLOCALE
	setlocale(LC_ALL, "");
#endif

	bool updateheaderadd=false;

	for (i=0; i<argv.size(); i++)
	{
		std::string n=argv[i];

		std::replace(n.begin(), n.end(), '\n', ' ');

		std::string::iterator b=n.begin(), e=n.end();
		std::string::iterator p=b;

		while (p != e)
		{
			if (*p & 0x80)
				break;
			++p;
		}

		if (!autoencode)
			p=e;

		std::string::iterator q=std::find(n.begin(), e, '=');

		if (q != e)
			++q;

		std::string w(q, e);

		std::string h_name(b, q);

		if (h_name == "ADDRESS=" ||
		    h_name == "URL=")
			updateheaderadd=true;

		if (p != e)
		{
			if (h_name != "NAME=")
			{
				std::string conv_p=
					unicode::iconvert::convert(w,
								   unicode_default_chset(),
								   "utf-8");

				n=std::string(b,p) + conv_p;
			}
			else
			{
				char *q=rfc2047_encode_str(w.c_str(),
							   unicode_default_chset(),
							   rfc2047_qp_allow_word);

				if (q)
				{
					n=std::string(b,p)+q;
					free(q);
				}
			}
		}

		ofs << n << std::endl;
	}
#if HAVE_SETLOCALE
	setlocale(LC_ALL, "C");
#endif

	std::string buf;

	while (std::getline(ifs, buf).good())
	{
		const char *p=buf.c_str();
		const char *q=strchr(p, '=');

		if (!q)	continue;

		for (i=0; i<argv.size(); i++)
		{
			const char *s=argv[i].c_str();

			if (strncmp(s, p, q-p) == 0 &&
				s[q-p] == '=')
				break;
		}
		if (i < argv.size())	continue;
		ofs << buf << std::endl;
	}

	if (ifs.bad() || ofs.bad())
	{
		perror(OPTIONS);
		return (1);
	}

	ofs.close();
	ifs.close();
	if (rename( OPTIONS ".new", OPTIONS))
	{
		perror(OPTIONS);
		return (1);
	}

	if (updateheaderadd)
		return updatelistheaders();

	return (0);
}

int cmdset(const std::vector<std::string> &argv)
{
	return cmdset(argv, true);
}
