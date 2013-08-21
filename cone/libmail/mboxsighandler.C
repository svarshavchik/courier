/*
** Copyright 2002-2006, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"

#include "mboxsighandler.H"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

mail::mbox::sighandler *mail::mbox::sighandler::curHandler;

void mail::mbox::sighandler::handler(int sig)
{
	if (curHandler->fd >= 0)
		if (ftruncate(curHandler->fd, curHandler->origSize) < 0)
			; // Ignore gcc warning
	_exit(1);
}

mail::mbox::sighandler::sighandler(int fdArg)
	: fd(fdArg)
{
	struct stat stat_buf;

	if (fstat(fd, &stat_buf) < 0)
	{
		fd= -1;
		return;
	}

	origSize=stat_buf.st_size;

	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));

	curHandler=this;
	sa.sa_handler= &handler;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
}

void mail::mbox::sighandler::block()
{
	sigset_t ss;

	sigemptyset(&ss);
	sigaddset(&ss, SIGHUP);
	sigaddset(&ss, SIGTERM);
	sigaddset(&ss, SIGINT);
	sigprocmask(SIG_BLOCK, &ss, NULL);
}

mail::mbox::sighandler::~sighandler()
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));

	sa.sa_handler= SIG_DFL;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	sigset_t ss;

	sigemptyset(&ss);
	sigaddset(&ss, SIGHUP);
	sigaddset(&ss, SIGTERM);
	sigaddset(&ss, SIGINT);
	sigprocmask(SIG_UNBLOCK, &ss, NULL);
}
