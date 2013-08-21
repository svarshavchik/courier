/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smapadd.H"
#include <errno.h>
#include <sstream>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

const char* mail::smapAdd::getName()
{
	return "ADD";
}

///////////////////////////////////////////////////////////////////////////
//
// The SMAP ADD command.

mail::smapAdd::smapAdd(std::vector<std::string> cmdsArg,
		       mail::callback &callback)
	: cmds(cmdsArg),
	  okfunc( &mail::smapAdd::sendNextCmd ),
	  failfunc( &mail::smapAdd::failCmd ), ready2send(false), tfile(NULL)
{
	defaultCB= &callback;
}

mail::smapAdd::~smapAdd()
{
	if (tfile)
		fclose(tfile);
}

void mail::smapAdd::installed(imap &imapAccount)
{
	doDestroy=true;
	sendNextCmd("OK");

	if (doDestroy)
		imapAccount.uninstallHandler(this);
}

bool mail::smapAdd::ok(std::string msg)
{
	return (this->*okfunc)(msg);
}

bool mail::smapAdd::fail(std::string msg)
{
	return (this->*failfunc)(msg);
}

bool mail::smapAdd::sendNextCmd(std::string msg)
{
	if (cmds.size() == 0)
	{
		struct stat stat_buf;

		if (fstat(fileno(tfile), &stat_buf) < 0)
		{
			return failCmd(strerror(errno));
		}

		tfilebytecount=stat_buf.st_size;

		ostringstream o;

		o << "ADD {" << tfilebytecount << "/" << tfilebytecount
		  << "}\n";

		okfunc=&mail::smapAdd::doneok;
		doDestroy=false;
		ready2send=true;
		myimap->imapcmd("", o.str());
		return true;
	}

	string s=cmds[0];

	cmds.erase(cmds.begin(), cmds.begin()+1);
	myimap->imapcmd("", s);

	doDestroy=false;
	return true;
}

bool mail::smapAdd::processLine(imap &imapAccount,
				std::vector<const char *> &words)
{
	if (words.size() > 0 && ready2send && strncmp(words[0], ">", 1) == 0)
	{
		ready2send=false;
		WriteTmpFile *w=new WriteTmpFile(this);

		if (!w)
			LIBMAIL_THROW(strerror(errno));

		w->tfile=tfile;
		tfile=NULL;
		w->tfilebytecount=tfilebytecount;
		w->origbytecount=tfilebytecount;

		try {
			imapAccount.socketWrite(w);
		} catch (...) {
			delete w;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		return true;
	}

	if (words.size() >= 2 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "ADD") == 0)
		return true;

	return smapHandler::processLine(imapAccount, words);
}

// When a command fails, must issue a RSET to clean up the ADD

bool mail::smapAdd::failCmd(std::string msg)
{
	okfunc= &mail::smapAdd::donefail;
	failfunc= &mail::smapAdd::donefail;
	doDestroy=false;
	finalmsg=msg;
	myimap->imapcmd("", "RSET\n");
	return true;
}

bool mail::smapAdd::doneok(std::string msg)
{
	return smapHandler::ok(finalmsg);
}

bool mail::smapAdd::donefail(std::string msg)
{
	return smapHandler::fail(finalmsg);
}

/////////////////////////////////////////////

mail::smapAdd::WriteTmpFile::WriteTmpFile(smapAdd *myAddArg)
	: myAdd(myAddArg), tfile(NULL), tfilebytecount(0),
	  origbytecount(0)
{
}

mail::smapAdd::WriteTmpFile::~WriteTmpFile()
{
	if (tfile)
		fclose(tfile);
}

bool mail::smapAdd::WriteTmpFile::fillWriteBuffer()
{
	if (!tfile)
		return false;

	if (tfilebytecount == 0)
	{
		writebuffer += "\n";
		fclose(tfile);
		tfile=NULL;
		myAdd->reportProgress(origbytecount,
				      origbytecount, 1, 1);
		return true;
	}

	char buffer[BUFSIZ];

	size_t n=sizeof(buffer);

	if (n > tfilebytecount)
		n=tfilebytecount;

	int c=fread(buffer, 1, n, tfile);

	if (c <= 0)
	{
		memset(buffer, '\n', n);
		c=(int)n;
	}

	tfilebytecount -= c;

	writebuffer += string(buffer, buffer+c);
	myAdd->reportProgress(origbytecount - tfilebytecount,
			      origbytecount, 0, 1);
	myAdd->setTimeout();
	return true;
}
