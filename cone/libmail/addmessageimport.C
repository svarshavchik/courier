/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "addmessage.H"
#include "attachments.H"
#include <errno.h>
#include <cstring>

using namespace std;

class mail::addMessage::assembleImportHelper : public callback::message {

	mail::addMessage *addMessagePtr;
	size_t &handleRet;
	mail::ptr<mail::account> acct;
	string msgUid;
	size_t msgNum;
	const mail::mimestruct &attachment;
	mail::callback &cb;
	FILE *tmpfp;

	string headers;

	// Imported from mail::callback::message:
	void messageTextCallback(size_t n, std::string text);

	// Imported from mail::callback:
	void success(std::string message);
	void fail(std::string message);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,
			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	assembleImportHelper(mail::addMessage *addMessagePtr,
			     size_t &handleRetArg,
			     mail::account *acctArg,
			     std::string msgUidArg,
			     size_t msgNumArg,
			     const mail::mimestruct &attachmentArg,
			     mail::callback &cbArg);
	~assembleImportHelper();
	void go();
};

void mail::addMessage::assembleImportAttachment(size_t &handleRet,
						mail::account *acct,
						std::string msgUid,
						const mail::mimestruct &attachment,
						mail::callback &cb)
{
	size_t n=0;
	mail::addMessage::assembleImportHelper *h;

	if (!mail::addMessage::chkMsgNum(acct, msgUid, n))
	{
		cb.fail("Message not found.");
		return;
	}

	h=new mail::addMessage::assembleImportHelper(this,
						     handleRet, acct,
						     msgUid, n, attachment,
						     cb);

	if (!h)
	{
		cb.fail(strerror(errno));
		return;
	}

	try
	{
		h->go();
	} catch (...) {
		delete h;
		cb.fail(strerror(errno));
	}
}

mail::addMessage::assembleImportHelper
::assembleImportHelper(mail::addMessage *addMessagePtrArg,
		       size_t &handleRetArg,
		       mail::account *acctArg,
		       std::string msgUidArg,
		       size_t msgNumArg,
		       const mail::mimestruct &attachmentArg,
		       mail::callback &cbArg)
	: addMessagePtr(addMessagePtrArg),
	  handleRet(handleRetArg),
	  acct(acctArg),
	  msgUid(msgUidArg),
	  msgNum(msgNumArg),
	  attachment(attachmentArg),
	  cb(cbArg),
	  tmpfp(NULL)
{
}

mail::addMessage::assembleImportHelper::~assembleImportHelper()
{
	if (tmpfp)
		fclose(tmpfp);
}

void mail::addMessage::assembleImportHelper::go()
{
	acct->readMessageContent(msgNum, true, attachment,
				 mail::readHeaders,
				 *this);

}

void mail::addMessage::assembleImportHelper::messageTextCallback(size_t n,
								 string text)
{
	if (tmpfp == NULL)
	{
		// Reading headers.
		headers += text;
	}
	else
	{
		// Reading content
		if (fwrite(text.c_str(), text.size(), 1, tmpfp) != 1)
			; // Supress warning
	}
}

void mail::addMessage::assembleImportHelper::success(string message)
{
	if (tmpfp == NULL) // Just read headers, check for error, read body.
	{
		if (acct.isDestroyed() || !chkMsgNum(acct, msgUid, msgNum))
		{
			fail("Message not found.");
			return;
		}
		if ((tmpfp=tmpfile()) == NULL)
		{
			fail(strerror(errno));
			return;
		}

		acct->readMessageContentDecoded(msgNum, true, attachment,
					       *this);
		return;
	}

	// Just read the content.

	if (fseek(tmpfp, 0L, SEEK_SET) < 0 || ferror(tmpfp))
	{
		fail(strerror(errno));
		return;
	}

	string::iterator hstart, hend;

	string encoding=
		mail::Attachment::find_header("CONTENT-TRANSFER-ENCODING:",
					      headers, hstart, hend);

	if (hstart != hend)
		headers.erase(hstart, hend);

	addMessagePtr->
		assembleContent(handleRet,
				mail::Attachment(headers, fileno(tmpfp), "",
						 encoding), cb);
	delete this;
}

void mail::addMessage::assembleImportHelper::fail(string message)
{
	cb.fail(message);
	delete this;
}

void mail::addMessage::assembleImportHelper::reportProgress(size_t bytesCompleted,
							    size_t bytesEstimatedTotal,
							    size_t messagesCompleted,
							    size_t messagesEstimatedTotal)
{
	cb.reportProgress(bytesCompleted,
			  bytesEstimatedTotal,
			  messagesCompleted,
			  messagesEstimatedTotal);
}
