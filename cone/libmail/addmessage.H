/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_addmessage_H
#define libmail_addmessage_H

#include "mail.H"

#include <string>
#include <list>
#include <vector>
#include <time.h>

#include "objectmonitor.H"
#include "namespace.H"
#include "structure.H"

LIBMAIL_START

class Attachment;

////////////////////////////////////////////////////////////////////////////
//
// Add message to a folder.  Each mail account is expected to create a
// subclass of mail::addMessage.  A mail::addMessage object gets created by
// mail::folder::addMessage().  This superclass provides fields, initialized
// with defaults.  The fields may be changed at any time before invoking the
// go() method.  The message text is provided by calling saveMessageContents().
// saveMessageContents() may be called repeatedly to provide the contents of
// a large message in pieces.  The message is saved by the go() method.
// The process may be aborted at any time, prior to go(), by invoking fail(),
// which automatically destroys the mail::addMessage object.
//
// A failure occuring while trying to add the message (including invoking
// fail()) is reported by the callback's fail method (the subclass receives
// the callback object and is responsible for dispatching the proper
// notification).


class addMessage : private ptr<mail::account> {

protected:
	bool checkServer();

public:
	addMessage(mail::account *);
	virtual ~addMessage();

	mail::messageInfo messageInfo; // Message flags

	time_t messageDate;	// Message add date.

	virtual void saveMessageContents(std::string)=0;
	virtual void go()=0;
	virtual void fail(std::string errmsg)=0;

	// Default MIME composition implementation

protected:
	std::list<mail::Attachment> att_list;
	std::vector< std::list<mail::Attachment>::iterator > att_list_vec;

public:
	virtual void assembleContent(size_t &,
				     const mail::Attachment &,
				     mail::callback &);
	virtual void assembleMessageRfc822(size_t &, std::string, size_t,
					     mail::callback &);
	virtual void assembleMultipart(size_t &,
				       std::string,
				       const std::vector<size_t> &,
				       std::string,
				       const mail::mimestruct::parameterList &,
				       mail::callback &);
	void assembleMultipart(size_t &handleRet,
			       std::string headers,
			       const std::vector<size_t> &parts,
			       std::string multipart_type,
			       mail::callback &cb)
	{
		mail::mimestruct::parameterList dummy;

		return assembleMultipart(handleRet,
					 headers, parts, multipart_type, dummy,
					 cb);
	}

	virtual void assembleImportAttachment(size_t &handleRet,
					      mail::account *acct,
					      std::string msgUid,
					      const mail::mimestruct &attachment,
					      mail::callback &cb);
	class assembleImportHelper;

	virtual void assembleRemoveAttachmentsFrom(size_t &handleRet,
						   mail::account *acct,
						   std::string msgUid,
						   const mail::mimestruct
						   &msgStruct,
						   const std::set<std::string>
						   &removeUidList,
						   mail::callback &cb);
	class assembleRemoveAttachmentsHelper;

	virtual bool assemble();

private:
	static bool chkMsgNum(mail::account *ptr, std::string msgUid,
			      size_t &n);


};

// addMessage is a "push" interface - the application "pushes"
// the new message's contents via saveMessageContents().  The mail::ACCOUNT
// API provides an alternative "pull" implementation, where a
// addMessagePull object is passed instead, whose getMessageContents()
// method is repeatedly invoked.  getMessageContents() should return the
// next part of the message's contents.  getMessageContents() is called
// repeatedly, until it returns an empty string which signifies end of
// message contents.

class addMessagePull {
public:
	addMessagePull();
	virtual ~addMessagePull();

	mail::messageInfo messageInfo; // Message flags
	time_t messageDate;	// Message add date.

	virtual std::string getMessageContents()=0;
};

LIBMAIL_END

#endif

