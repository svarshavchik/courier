/*
** Copyright 2002-2006, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "mboxadd.H"
#include "mboxsighandler.H"
#include "file.H"

#include <errno.h>
#include <pwd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

mail::mbox::folder::add::add(mail::mbox &mboxArg, string pathArg,
					   mail::callback &callbackArg)
	: mail::addMessage(&mboxArg), path(pathArg),
	  callback(callbackArg),
	  fp(tmpfile()),
	  mboxAccount(&mboxArg)
{
}

mail::mbox::folder::add::~add()
{
	if (fp)
		fclose(fp);
}

void mail::mbox::folder::add::saveMessageContents(string msg)
{
	if (*(msg.c_str()) == 0)
		return;

	if (fp)
		if (fwrite(&msg[0], msg.size(), 1, fp) != 1)
			; // Ignore gcc warning
}

void mail::mbox::folder::add::fail(string msg)
{
	callback.fail(msg);
	delete this;
}

void mail::mbox::folder::add::success(string msg)
{
	callback.success(msg);
	delete this;
}

void mail::mbox::folder::add::reportProgress(size_t bytesCompleted,
					     size_t bytesEstimatedTotal,
			    
					     size_t messagesCompleted,
					     size_t messagesEstimatedTotal)
{
	callback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				messagesCompleted, messagesEstimatedTotal);
}

void mail::mbox::folder::add::go()
{
	if (!fp)
		fail(strerror(errno));

	if (mboxAccount.isDestroyed())
	{
		fail("Server connection aborted");
		return;
	}

	try {
		if (path == mboxAccount->currentFolder)
		{
			mboxAccount->installTask(new LockCurrentFolder( *this ));
		}
		else
		{
			mboxAccount->installTask(new LockCurrentFolder( *this,
								 path));
		}
	} catch (...) {
		fail("An exception occured while adding message.");
		return;
	}
}

//
// Folder's now locked.
//

void mail::mbox::folder::add::copyTo(mail::file &file)
{
	struct stat st;

	mail::mbox::sighandler updating(fileno(static_cast<FILE *>(file)));

	try {

		// Make sure the mboxAccount file ends with a trailing newline

		if (fstat(fileno(static_cast<FILE *>(file)), &st) < 0)
		{
			fail(strerror(errno));
			return;
		}

		int ch='\n';

		if (st.st_size > 0)
		{
			if (fseek(file, -1, SEEK_END) < 0 ||
			    (ch=getc(file)) == EOF)
			{
				fail(strerror(errno));
				return;
			}
		}

		if (fseek(file, 0L, SEEK_END) < 0 ||
		    fseek(fp, 0L, SEEK_SET) < 0)
		{
			fail(strerror(errno));
			return;
		}

		if (ch != '\n')
			putc('\n', file);

		mail::file f(fileno(fp), "w+");

		messageInfo.uid= mail::mboxMagicTag()
			.getMessageInfo().uid;

		string hdr=mail::mboxMagicTag(messageInfo.uid,
					      messageInfo,

					      mail::keywords::Message()
					      // KEYWORDS

					      ).toString();

		struct passwd *pw;
		pw=getpwuid(getuid());

		fprintf(file, "From %s %s%s\n",
			pw ? pw->pw_name:"nobody",
			ctime(&messageDate),
			hdr.c_str());

		size_t bytesDone=0;
		size_t bytesNextReport=0;

		while (!feof(f))
		{
			string l=f.getline();

			if (l.size() == 0 && feof(f))
				break;

			const char *p=l.c_str();

			while (*p == '>')
				p++;

			if (strncmp(p, "From ", 5) == 0)
				l=">" + l;

			l += "\n";

			if (fwrite(&l[0], l.size(), 1, file) != 1)
				; // Ignore gcc warning

			bytesDone += l.size();

			if (bytesDone >= bytesNextReport)
			{
				bytesNextReport=bytesDone + BUFSIZ;
				callback.reportProgress(bytesDone, 0, 0, 1);
			}
		}

		if (fflush(file) < 0 || ferror(file))
		{
			updating.rollback();
			fail(strerror(errno));
			return;
		}

		callback.reportProgress(bytesDone, bytesDone, 1, 1);
		updating.block();
	} catch (...) {
		updating.rollback();
		fail("An exception occured while adding message.");
		return;
	}

	try {
		fclose(fp);
		fp=NULL;
		success("OK");
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

///////////////////////////////////////////////////////////////////////////

mail::mbox::folder::add::LockCurrentFolder
::LockCurrentFolder(add &meArg, string pathArg)
	: LockTask(*meArg.mboxAccount, meArg, pathArg), me(meArg)
{
}

mail::mbox::folder::add::LockCurrentFolder::~LockCurrentFolder()
{
}


bool mail::mbox::folder::add::LockCurrentFolder::locked(mail::file
							&lockedFile)
{
	// If the current folder is opened in read-only mode, this is a no-go.

	if (me.path == me.mboxAccount->currentFolder && mboxAccount.currentFolderReadOnly)
	{
		me.fail("Folder opened in read-only mode.");
		done();
		return true;
	}

	me.copyTo(lockedFile);
	done();
	return true;
}

