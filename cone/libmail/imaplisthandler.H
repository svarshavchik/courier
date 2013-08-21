/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imaplisthandler_H
#define libmail_imaplisthandler_H

#include "libmail_config.h"
#include "imaphandler.H"
#include "imapfolders.H"
#include "imapparsefmt.H"

#include <vector>

///////////////////////////////////////////////////////////////////////////
//
// A list command.

LIBMAIL_START

class imapListHandler : public imapCommandHandler {

	mail::callback::folderList &callback1;
	mail::callback &callback2;
	std::string hier;

	bool oneFolderOnly;
	bool fallbackOneFolderOnly;

	// True if this LIST command is invoked to determine the folder
	// hierarchy separator.  Otherwise, the LIST command is invoked
	// to obtain a list of subfolders.

	std::vector <imapFolder> folders;

public:
	imapListHandler(mail::callback::folderList &myCallback,
			      mail::callback &myCallback2,
			      std::string myHier, bool oneFolderOnlyArg);
	~imapListHandler();
	const char *getName();
	void timedOut(const char *errmsg);

	void installed(imap &imapAccount);
private:

	bool untaggedMessage(imap &imapAccount, std::string name);
	bool taggedMessage(imap &imapAccount, std::string name, std::string message,
			   bool okfail, std::string errmsg);
};


// Untagged LIST parser

class imapLIST : public imapHandlerStructured {

	std::vector <imapFolder> &folderList;
	size_t pfixLength;
	bool oneNameOnly;

	void (imapLIST::*next_func)(imap &, Token);

	bool hasChildren, hasNoChildren, marked, unmarked, noSelect;
	std::string hiersep;

protected:
	imapparsefmt xattributes;
public:
	imapLIST(std::vector<imapFolder> &mylist, size_t pfixLengthArg,
		       bool oneNameOnlyArg=false);
	~imapLIST();

	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);

	void process(imap &imapAccount, Token t);

	void start_attribute_list(imap &imapAccount, Token t);
	void get_attribute(imap &imapAccount, Token t);
	void get_hiersep(imap &imapAccount, Token t);
	void get_name(imap &imapAccount, Token t);
	void get_xattr_start(imap &imapAccount, Token t);
	void get_xattr_do(imap &imapAccount, Token t);

	virtual void processExtendedAttributes(imapFolder &);
};

LIBMAIL_END

#endif
