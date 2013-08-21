/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imapfolder_H
#define libmail_imapfolder_H

#include "libmail_config.h"
#include "maildir/maildirkeywords.h"
#include "mail.H"
#include "imap.H"
#include <vector>

LIBMAIL_START

class imap;

class imapFOLDERinfo {

public:
	mail::callback::folder &folderCallback;

	imapFOLDERinfo(std::string pathArg,
		       mail::callback::folder &folderCallbackArg);
	virtual ~imapFOLDERinfo();

	class indexInfo : public mail::messageInfo {
	public:
		mail::keywords::Message keywords;

		indexInfo();
		~indexInfo();
	};

	std::vector<indexInfo> index;
	size_t exists;
	// May be less than index.length(), when
	// synchronization is in progress

	bool closeInProgress;

	std::string path;

	virtual void opened(); // Used by SMAP
	virtual void existsMore(mail::imap &, size_t)=0;
	virtual void resetMailCheckTimer()=0;
	virtual void setUid(size_t, std::string);
};


/////////////////////////////////////////////////////////////////////////
//
// A currently-SELECTED folder.
//
// This object handles all kinds of messages from the IMAP server when a
// folder is open.
//

class imapFOLDER : public imapCommandHandler, public imapFOLDERinfo {

	int mailCheckInterval;

public:

	class existsCallback : public mail::callback {

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal); // TODO

	public:
		existsCallback();
		~existsCallback();
		class imapFOLDER *me;

	private:
		void success(std::string message);
		void fail(std::string message);
	} *existsNotify;

	std::string uidv;

	imapFOLDER(std::string pathArg, std::string uidvArg, size_t existsArg,
		   mail::callback::folder &folderCallback,
		   mail::imap &myserver);

	~imapFOLDER();

	static const char name[];

	void installed(imap &imapAccount);
private:
	const char *getName();
	void timedOut(const char *);
	virtual int getTimeout(imap &);

	bool untaggedMessage(imap &imapAccount, std::string name);
	bool taggedMessage(imap &imapAccount, std::string name,
			   std::string message,
			   bool okfail, std::string errmsg);

	void existsMore(mail::imap &, size_t);
	void resetMailCheckTimer();
	void setUid(size_t, std::string);
};

// The SMAP version

class smapFOLDER : public imapFOLDERinfo, public imapHandler {

	int mailCheckInterval;

	void installed(imap &);
	int process(imap &imapAccount, std::string &buffer);
	const char *getName();
	void timedOut(const char *errmsg);

	int getTimeout(mail::imap &imapAccount);

	bool openedFlag;

public:
	static const char name[];

	smapFOLDER(std::string pathArg,
		   mail::callback::folder &folderCallbackArg,
		   mail::imap &myserver);
	~smapFOLDER();

	void existsMore(mail::imap &imapAccount, size_t n);
	void resetMailCheckTimer();
	void opened();
};

LIBMAIL_END

#endif
