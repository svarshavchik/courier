#include	"config.h"
#include	"courier.h"
#include	"cdfilters.h"
#include        "filtersocketdir.h"
#include	<fstream>
#include        <sys/socket.h>
#include        <sys/un.h>
#include	<sys/time.h>
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#if HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdlib.h>
#include	<errno.h>
#include	<ctype.h>
#include	<string.h>

#include	<algorithm>
#include	<iostream>
#include	<vector>

#include	"afx/afx.h"

#include	"mydirent.h"
#include	"localstatedir.h"

static int dofilter(std::string,
		    const char *,
		    unsigned,
		    std::string (*)(unsigned, void *),
		    void *);

#define FILTER_LIST_INCREMENT	8
#define MEMORY_ERROR	"432 Out of memory when processing mail filters.\n"
#define FILTER_ERROR	"432 Mail filters temporarily unavailable.\n"

static int run_filter_dir(const char *filterdir,
			  const char *filename,
			  unsigned nmsgids,
			  std::string (*msgidfunc)(unsigned, void *),
			  void *funcarg);

int run_filter(const char *filename,
	       unsigned nmsgids,
	       int iswhitelisted,
	       std::string (*msgidfunc)(unsigned, void *),
	       void *funcarg)
{
	int rc;

	if (nmsgids == 0)	return (0);

	if (!iswhitelisted)
	{
		rc=run_filter_dir(FILTERSOCKETDIR, filename, nmsgids,
				  msgidfunc, funcarg);

		if (rc)
			return (rc < 0 ? 0 : rc);
	}

	rc=run_filter_dir(ALLFILTERSOCKETDIR, filename, nmsgids,
			  msgidfunc, funcarg);
	if (rc)
		return (rc < 0 ? 0 : rc);

	return 0;
}

static int run_filter_dir(const char *filterdir,
			  const char *filename,
			  unsigned nmsgids,
			  std::string (*msgidfunc)(unsigned, void *),
			  void *funcarg)
{
	std::vector<std::string> filterlist;

	DIR	*dirp;
	struct dirent *de;
	std::string	sockname;
	int	rc;

	dirp=opendir(filterdir);

	while (dirp && (de=readdir(dirp)) != 0)
	{
		if (de->d_name[0] == '.')	continue;

		sockname=filterdir;
		sockname += "/";
		sockname += de->d_name;
		filterlist.push_back(sockname);
	}
	if (dirp)	closedir(dirp);

	std::sort(filterlist.begin(), filterlist.end());

	std::vector<std::string>::iterator b, e;

	for (b=filterlist.begin(), e=filterlist.end(); b != e; ++b)
	{
		rc = dofilter( *b, filename, nmsgids,
			       msgidfunc,
			       funcarg);

		if (rc)
			return rc;
	}
	return 0;
}

static void dofilter_err(const char *err,
			std::string sockname)
{
	clog_msg_prerrno();
	clog_msg_start_err();
	clog_msg_str(err);
	clog_msg_str(sockname.c_str());
	clog_msg_send();
	std::cout << FILTER_ERROR << std::flush;
}

static int dofilter(std::string sockname,
	const char *filename,
	unsigned nmsgids,
	std::string (*msgidfunc)(unsigned, void *),
	void *funcarg)
{
	int	s;
	std::vector<char> ssun_buf;

	struct  sockaddr_un *ssun;
	int	triedagain=0;
	int	rc;

	ssun_buf.resize(sockname.size()+sizeof(struct sockaddr_un));

	ssun= (struct sockaddr_un *)&ssun_buf[0];

	if ((s=socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
		clog_msg_errno();

	if (fcntl(s, F_SETFL, O_NDELAY) < 0)
		clog_msg_errno();

	ssun->sun_family=AF_UNIX;
	strcpy(ssun->sun_path, sockname.c_str());

	while ((rc=connect(s, (struct sockaddr *)ssun,
			   ssun->sun_path+strlen(ssun->sun_path)-&ssun_buf[0]))
	       < 0 && errno == EAGAIN)
	{
		if (++triedagain > 5)
			break;
		sleep(1);
		ssun->sun_family=AF_UNIX;
		strcpy(ssun->sun_path, sockname.c_str());
	}

	if (rc < 0)
	{
	struct	timeval	tv;
	fd_set	fds;

		if (errno != EINPROGRESS)
		{
			dofilter_err("Failed to connect to: ", sockname);
			close(s);
			return (1);
		}

		tv.tv_sec=10;
		tv.tv_usec=0;
		FD_ZERO(&fds);
		FD_SET(s, &fds);
		if (select(s+1, &fds, 0, 0, &tv) <= 0)
		{
			dofilter_err("Failed to connect to: ", sockname);
			close(s);
			return (1);
		}
		if (connect(s, (struct sockaddr *)&ssun, sizeof(ssun)) &&
			errno != EISCONN)
		{
			dofilter_err("Failed to connect to: ", sockname);
			close(s);
			return (1);
		}
	}
	if (fcntl(s, F_SETFL, 0) < 0)
		clog_msg_errno();

afxiopipestream	sockstream(s);

	sockstream << TMPDIR "/" << filename << '\n';

unsigned i;

	for (i=0; i<nmsgids; i++)
	{
		sockname=  (*msgidfunc)(i, funcarg);
		if (sockname.size() == 0)
			sockname=" ";
		sockstream << sockname << '\n';
	}
	sockstream << '\n';
	sockstream << std::flush;

	if (sockstream.bad())
	{
		dofilter_err("Connection closed when processing: ", sockname);
		sockstream.close();
		close(s);
		return (1);
	}

	if ( std::getline(sockstream, sockname).fail())
	{
		dofilter_err("Connection closed when processing: ", sockname);
		sockstream.close();
		close(s);
		return (1);
	}

	int	d=sockname.size() ? sockname[0]:0;

	if (isdigit(d))
	{
		if (d != '0' && d != '4' && d != '5')
		{
			while (sockname.size() >= 3 &&
			       isdigit(sockname[0]) &&
				isdigit(sockname[1]) &&
				isdigit(sockname[2]) &&
				sockname[3] == '-')
			{
				if (std::getline(sockstream, sockname).fail())
					break;
			}
			sockstream.close();
			close(s);
			return (0);
		}
	}

	rc = 1;
	if (d == '0')
	{
		sockname[0] = '2';
		rc = -1;
	}
	std::cout << sockname << "\n";

	while (sockname.size() >= 3 &&
	       isdigit(sockname[0]) && isdigit(sockname[1]) &&
	       isdigit(sockname[2]) && sockname[3] == '-')
	{
		if (std::getline(sockstream, sockname).fail())
			break;
		std::cout << sockname << "\n";
	}
	std::cout << std::flush;
	sockstream.close();
	close(s);
	return (rc);
}
