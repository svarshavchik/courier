/*
**
** Copyright 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/
#include	"config.h"
#include	"webmlmd.H"
#include	"webmlmdcmlm.H"
#include	<sstream>
#include	<errno.h>
#include	<stdlib.h>
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<signal.h>

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/wait.h>
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	"bindir.h"
#include	"rfc822/rfc822.h"

using namespace webmlmd;

cmlm::filep::filep() : fp(NULL)
{
}

cmlm::filep::~filep()
{
	close();
}

void cmlm::filep::close()
{
	if (fp)
		fclose(fp);
	fp=NULL;
}

cmlm::cmlm() : pid(-1)
{
}

cmlm::~cmlm()
{
}

bool cmlm::start(std::string extension,
		 std::string sender,
		 std::string command,
		 const std::vector<std::string> &args)
{
	int pipefd[2];

	stdout_filep.close();
	if ((stdout_filep.fp=tmpfile()) == NULL)
		return false;

	if (pipe(pipefd) < 0)
	{
		return false;
	}

	pid=fork();

	if (pid < 0)
	{
		close(pipefd[0]);
		close(pipefd[1]);
		return false;
	}

	if (pid == 0)
	{
		if (dup2(pipefd[0], 0) < 0)
		{
			exit(1);
		}

		if (dup2(fileno(stdout_filep.fp), 1) < 0 || dup2(1, 2) < 0)
		{
			exit(1);
		}
		stdout_filep.close();
		fcntl(1, F_SETFD, 0);

		close(pipefd[0]);
		close(pipefd[1]);

		setenv("DEFAULT", extension.c_str(), 1);
		setenv("SENDER", sender.c_str(), 1);

		std::vector< std::vector<char> > argv_buf;

		{
			std::vector<char> v;

			v.insert(v.end(), command.begin(), command.end() );
			v.push_back(0);

			argv_buf.push_back(v);
		}

		// Second arg to couriermlm is list directory, always "."

		{
			std::vector<char> v;

			v.push_back('.');
			v.push_back(0);

			argv_buf.push_back(v);
		}

		std::vector<std::string>::const_iterator ab=args.begin(),
			ae=args.end();

		while (ab != ae)
		{
			std::vector<char> v;

			v.insert(v.end(), ab->begin(), ab->end());
			v.push_back(0);

			argv_buf.push_back(v);
			++ab;
		}

		std::vector<const char *> argv;

		argv.push_back(BINDIR "/couriermlm");

		std::vector< std::vector<char> >::iterator vb=argv_buf.begin(),
			ve=argv_buf.end();

		while (vb != ve)
		{
			argv.push_back( & (*vb)[0] );
			++vb;
		}

		argv.push_back(NULL);

		execv(argv[0], (char **)&argv[0]);
		perror(argv[0]);
		exit(1);
	}

	close(pipefd[0]);

	signal(SIGPIPE, SIG_IGN);
	stdin_filep.close();
	stdin_filep.fp=fdopen(pipefd[1], "w");
	return true;
}

bool cmlm::wait()
{
	int waitstat;
	pid_t p;

	stdin_filep.close();

	while ((p= ::wait(&waitstat)) != pid)
	{
		if (p == -1)
		{
			if (errno == EINTR)
				continue;
			fseek(stdout_filep.fp, 0L, SEEK_END);
			fseek(stdout_filep.fp, 0L, SEEK_SET);
			return false;
		}
	}

	fseek(stdout_filep.fp, 0L, SEEK_END);
	fseek(stdout_filep.fp, 0L, SEEK_SET);

	return waitstat == 0;
}

void cmlm::mk_received_header()
{
	char buf[100];
	const char *ip=getenv("REMOTE_ADDR");

	rfc822_mkdate_buf(time(NULL), buf);

	fprintf(stdin_filep.fp, "Received: from ([%s]) via http; %s\n",
		ip ? ip:"(unknown)", buf);
}

std::string cmlm::format_error_message()
{
	std::string errmsg;

	if (!stdout_filep.fp)
		errmsg=strerror(errno);
	else
	{
		char buf[BUFSIZ];

		std::ostringstream o;

		while (fgets(buf, sizeof(buf), stdout_filep.fp) != NULL)
			o << webmlmd::html_escape(std::string(buf));
		errmsg=o.str();
	}

	if (errmsg == "")
	{
		errmsg="An error has occured while processing this command\n"
			"No further information is available\n";
	}

	return "<blockquote class='error'><pre>" + errmsg + "</pre></blockquote>";
}
