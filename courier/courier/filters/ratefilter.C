/*
** Copyright 2013 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	<cstdlib>
#include	<cstring>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<time.h>
#include	<iostream>
#include	<iomanip>
#include	<sstream>
#include	<fstream>
#include	<map>
#include	"comctlfile.h"
#include	"filtersocketdir.h"
#include	"afx/afx.h"
#include	<sys/types.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#include	"libfilter/libfilter.h"

static int ratefilter(size_t interval, size_t maxrcpts, size_t minuid)
{
	int	listensock;
	std::string line;
	std::string sender;

	typedef std::map<std::string, size_t> countermap_t;

	if (interval < 60)
		interval=60;

	// Keep two maps, each map covers one half of the rate-limiting
	// intervals. Each time we cross the half-interval boundary, we
	// swap the two pointers to the maps.

	countermap_t map0, map1;
	countermap_t *mapptr0=&map0, *mapptr1=&map1;
	time_t lasttimestamp=0;
	interval /= 2;

	listensock=lf_init("filters/ratefilter-mode",
		ALLFILTERSOCKETDIR "/ratefilter",
		ALLFILTERSOCKETDIR "/.ratefilter",
		FILTERSOCKETDIR "/ratefilter",
		FILTERSOCKETDIR "/.ratefilter");

	if (listensock < 0)
		return (1);

	lf_init_completed(listensock);

	for (;;)
	{
		int fd;

		if ((fd=lf_accept(listensock)) < 0)	break;

		afxiopipestream io(fd);

		std::getline(io, line); // Data file
		sender.clear();

		{
			std::ifstream i(line.c_str());

			if (i.is_open())
			{
				while (!std::getline(i, line).eof())
				{
					// Look for the first Received: header
					if (strncmp(line.c_str(), "Received:",
						    9))
						continue;

					// The next line should be (uid nnnn).
					if (!std::getline(i, line).eof())
					{
						const char *p=line.c_str();

						while (*p == ' ')
							++p;

						if (strncmp(p, "(uid", 4) == 0)
						{
							uid_t uid;

							p += 4;

							while (*p == ' ')
								++p;

							uid=atoi(p);

							// Don't rate-limit
							// daemon mail.
							if (uid >= minuid)
							{
								sender=line;
							}
						}
					}
					break;
				}
			}
		}

		bool toomuch=false;

		size_t receipients=0;

		while (!std::getline(io, line).eof() && line.size() > 0)
		{
			std::ifstream i(line.c_str());

			if (!i.is_open())
				continue;

			while (!std::getline(i, line).eof())
			{
				const char *p=line.c_str();
				if (*p == COMCTLFILE_AUTHNAME)
					sender=line.substr(1);
				if (*p == COMCTLFILE_RECEIPIENT)
					++receipients;
			}
		}

		if (sender.size())
		{
			time_t t=time(NULL);

			if (t != (time_t)-1)
			{
				t /= interval;

				if (t != lasttimestamp)
				{
					countermap_t *swap=mapptr0;

					mapptr0=mapptr1;
					mapptr1=swap;

					mapptr0->clear(); // It's a new day

					if (lasttimestamp != t+1)
						// More than on day, actually...
						mapptr1->clear();

					lasttimestamp=t;
				}

				countermap_t::iterator p=
					mapptr0->insert(std::make_pair(sender, 0))
					.first;

				countermap_t::const_iterator last=
					mapptr1->find(sender);

				p->second += receipients;
				if (p->second + (last == mapptr1->end() ? 0:
						 last->second) > maxrcpts)
					toomuch=true;
			}
		}
		io << (toomuch ? "500 Number of messages exceeded administrative limit.\n":"200 Ok\n") << std::flush;
	}
	return (0);
}

static size_t getconfig(const char *configname, size_t defaultvalue)
{
 	char	*fn, *f;
	fn=config_localfilename(configname);

	if ( (f=config_read1l(fn)) != 0)
	{
		std::istringstream(f) >> defaultvalue;
		free(f);
	}
	free(fn);
	return defaultvalue;
}

int main(int argc, char **argv)
{
	clog_open_stderr(0);
	ratefilter(getconfig("filters/ratefilter-interval", 60),
		   getconfig("filters/ratefilter-maxrcpts", 100),
		   getconfig("filters/ratefilter-minuid", 100));
	exit(0);
	return 0;
}
