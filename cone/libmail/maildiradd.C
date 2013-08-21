/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "maildiradd.H"
#include <cstring>

#include "maildir/config.h"
#include "maildir/maildircreate.h"
#include "maildir/maildirrequota.h"
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <errno.h>

#include <sstream>

#if HAVE_UTIME_H
#include <utime.h>
#endif
#if TIME_WITH_SYS_TIME
#include        <sys/time.h>
#include        <time.h>
#else
#if HAVE_SYS_TIME_H
#include        <sys/time.h>
#else
#include        <time.h>
#endif
#endif

using namespace std;

mail::maildir::addmessage::addmessage(mail::maildir *maildirArg,
				      string folderPathArg,
				      mail::callback &callbackArg)
	: mail::addMessage(maildirArg),
	  tmpname(""),
	  newname(""),
	  folderPath(folderPathArg),
	  initialized(0),
	  errmsg(""),
	  tmpfile(NULL),
	  callback(&callbackArg)
{
}

void mail::maildir::addmessage::initialize()
{
	static unsigned cnt=0;

	initialized=true;

	ostringstream o;

	o << ++cnt;

	string ocnt=o.str();

	struct maildir_tmpcreate_info createInfo;

	maildir_tmpcreate_init(&createInfo);

	createInfo.maildir=folderPath.c_str();
	createInfo.uniq=ocnt.c_str();

	while ((tmpfile=maildir_tmpcreate_fp(&createInfo)) == NULL)
	{
		if (errno != EAGAIN)
		{
			errmsg=strerror(errno);
			return;
		}
		sleep(3);
	}

	try {
		tmpname=createInfo.tmpname;
		newname=createInfo.newname;
		maildir_tmpcreate_free(&createInfo);
	} catch (...) {
		fclose(tmpfile);
		tmpfile=NULL;
		unlink(createInfo.tmpname);
		maildir_tmpcreate_free(&createInfo);
		errmsg=strerror(errno);
	}
}

mail::maildir::addmessage::~addmessage()
{
	if (tmpfile)
	{
		fclose(tmpfile);
		unlink(tmpname.c_str());
	}

	if (callback)
		callback->fail("Operation cancelled.");
}

void mail::maildir::addmessage::saveMessageContents(string s)
{
	if (!initialized)
		initialize();

	if (tmpfile != NULL)
		if (fwrite(&s[0], s.size(), 1, tmpfile) != 1)
			; // Ignore gcc warning
}


// NOTE: pop3maildrop depends on go() completing immediately.

void mail::maildir::addmessage::go()
{
	if (!initialized)
		initialize();

	if (!tmpfile)
	{
		fail(errmsg);
		return;
	}

	struct stat stat_buf;

	if (ferror(tmpfile) || fflush(tmpfile) ||
	    fstat(fileno(tmpfile), &stat_buf) < 0)
	{
		fail(strerror(errno));
		return;
	}

	// For Courier compatibility, modify the filename to include the
	// file size.

	char *p=maildir_requota(newname.c_str(), stat_buf.st_size);

	if (!p)
	{
		fail(strerror(errno));
		return;
	}

	try {
		newname=p;
		free(p);
	} catch (...) {
		free(p);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	fclose(tmpfile);
	tmpfile=NULL;

	// Now, stick on the flags we want.

	string flags=getMaildirFlags(messageInfo);

	if (flags.size() > 0)
	{
		newname = newname + MDIRSEP "2," + flags;
		memcpy(strrchr(const_cast<char *>(newname.c_str()), '/')-3,
		       "cur", 3);
		// We go into the cur directory, now
	}

#if     HAVE_UTIME
	if (messageDate)
        {
		struct  utimbuf ub;

                ub.actime=ub.modtime=messageDate;
                utime(tmpname.c_str(), &ub);
        }
#else
#if     HAVE_UTIMES
        if (messageDate)
        {
		struct  timeval tv;

                tv.tv_sec=messageDate;
                tv.tv_usec=0;
                utimes(tmpname.c_str(), &tv);
        }
#endif
#endif

	if (maildir_movetmpnew(tmpname.c_str(), newname.c_str()) < 0)
	{
		fail(strerror(errno));
		return;
	}

	try {
		if (callback)
		{
			mail::callback *p=callback;
			callback=NULL;

			p->success("OK");
		}

		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::maildir::addmessage::fail(string errmsg)
{
	try {
		if (callback)
		{
			mail::callback *p=callback;
			callback=NULL;

			p->fail(errmsg);
		}
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}
