/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "nntpcommand.H"
#include "libmail/addmessage.H"
#include "gettext.H"
#include <errno.h>
#include <stdio.h>
#include <cstdlib>
#include <cstring>

#include <sys/types.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

using namespace std;

string nntpCommandFolder::nntpCommand;

nntpCommandFolder::nntpCommandFolder() : folder(NULL)
{
}

nntpCommandFolder::~nntpCommandFolder()
{
}

// Stubs

void nntpCommandFolder::sameServerAsHelperFunc() const
{
}

string nntpCommandFolder::getName() const
{
	return "";
}

string nntpCommandFolder::getPath() const
{
	return "";
}

bool nntpCommandFolder::hasMessages() const
{
	return false;
}

bool nntpCommandFolder::hasSubFolders() const
{
	return false;
}

string nntpCommandFolder::isNewsgroup() const
{
	return "";
}

bool nntpCommandFolder::isParentOf(string path) const
{
	return false;
}

void nntpCommandFolder::hasMessages(bool dummy)
{
}

void nntpCommandFolder::hasSubFolders(bool dummy)
{
}

void nntpCommandFolder::na(mail::callback &cb)
{
	cb.fail("Not implemented");
}

void nntpCommandFolder::readFolderInfo( mail::callback::folderInfo &callback1,
					mail::callback &callback2) const
{
	na(callback2);
}

void nntpCommandFolder::getParentFolder(mail::callback::folderList &callback1,
					mail::callback &callback2) const
{
	na(callback2);
}

void nntpCommandFolder::readSubFolders( mail::callback::folderList &callback1,
					mail::callback &callback2) const
{
	na(callback2);
}

void nntpCommandFolder::createSubFolder(string name, bool isDirectory,
					mail::callback::folderList &callback1,
					mail::callback &callback2) const
{
	na(callback2);
}

void nntpCommandFolder::create(bool isDirectory,
			       mail::callback &callback) const
{
	na(callback);
}

void nntpCommandFolder::destroy(mail::callback &callback,
				bool destroyDir) const
{
	na(callback);
}

void nntpCommandFolder::renameFolder(const mail::folder *newParent,
				     string newName,
				     mail::callback::folderList &callback1,
				     mail::callback &callback2) const
{
	na(callback2);
}

mail::folder *nntpCommandFolder::clone() const
{
	return new nntpCommandFolder();
}

string nntpCommandFolder::toString() const
{
	return "";
}

void nntpCommandFolder::open(mail::callback &openCallback,
			     mail::snapshot *restoreSnapshot,
			     mail::callback::folder &folderCallback) const
{
	na(openCallback);
}

//////////////////////////////////////////////////////////////////////////

class nntpCommandFolder::add : public mail::addMessage {

	mail::callback &cb;
	FILE *t;

	unsigned long bytesDone;

public:
	add(mail::callback &cbArg);
	~add();
	void saveMessageContents(string);
	void go();
	void fail(string errmsg);
};

mail::addMessage *nntpCommandFolder::addMessage(mail::callback &callback) const
{
	nntpCommandFolder::add *n=new add(callback);

	if (!n)
		callback.fail(strerror(errno));

	return n;
}

nntpCommandFolder::add::add(mail::callback &cbArg)
	: mail::addMessage(NULL), cb(cbArg), t(tmpfile()), bytesDone(0)
{
}

nntpCommandFolder::add::~add()
{
	if (t)
		fclose(t);
}

void nntpCommandFolder::add::saveMessageContents(string s)
{
	if (t && !ferror(t))
		if (fwrite(s.c_str(), s.size(), 1, t) != 1)
			; // Ignore gcc warning

	bytesDone += s.size();

	cb.reportProgress(bytesDone, 0, 0, 1);
}

void nntpCommandFolder::add::fail(string errmsg)
{
	cb.fail(errmsg);
	delete this;
}

void nntpCommandFolder::add::go()
{
	if (!t || ferror(t) < 0 || fflush(t) < 0 ||
	    fseek(t, 0L, SEEK_SET) < 0)
	{
		fail(strerror(errno));
		return;
	}

	cb.reportProgress(bytesDone, bytesDone, 0, 1);

	char errbuf[8192];
	int pipefd[2];

	if (pipe(pipefd) < 0)
	{
		fail(strerror(errno));
		return;
	}

	pid_t p=fork();

	if (p < 0)
	{
		close(pipefd[0]);
		close(pipefd[1]);
		fail(strerror(errno));
		return;
	}

	if (p == 0)
	{
		close(2);
		if (dup(pipefd[1]) != 2)
			exit(1);
		close(1);
		if (dup(pipefd[1]) != 1)
		{
			perror("dup");
			exit(1);
		}

		close(pipefd[0]);
		close(pipefd[1]);

		close(0);
		if (dup(fileno(t)) != 0)
		{
			perror("dup");
			exit(1);
		}
		fclose(t);
		t=NULL;

		const char *sh=getenv("SHELL");

		if (!sh || !*sh)
			sh="/bin/sh";

		execl(sh, sh, "-c", nntpCommand.c_str(), NULL);
		perror(sh);
		exit(1);
	}

	fclose(t);
	t=NULL;

	// Capture stderr

	size_t n=0;
	close(pipefd[1]);

	for (;;)
	{
		char buf2[256];

		int i=read(pipefd[0], buf2, sizeof(buf2));

		if (i <= 0)
			break;

		if (i > (int)(sizeof(errbuf)-1-n))
			i=sizeof(errbuf)-1-n;

		if (i)
			memcpy(errbuf+n, buf2, i);
		n += i;
	}
	close(pipefd[0]);

	errbuf[n]=0;

	pid_t p2;
	int waitstat;

	while ((p2=wait(&waitstat)) > 0 && p2 != p)
		;

	if (p2 == p && WIFEXITED(waitstat) && WEXITSTATUS(waitstat) == 0)
	{
		if (errbuf[0] == 0)
			strcpy(errbuf, _("Ok"));

		try {
			cb.success(errbuf);
			delete this;
		} catch (...) {
			delete this;
			throw;
		}
		return;
	}

	if (errbuf[0] == 0)
		strcpy(errbuf, _("Posting command failed"));

	try {
		fail(errbuf);
	} catch (...) {
		delete this;
	}
}

