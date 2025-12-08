/*
** Copyright 2002-2004, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"

#include "mail.H"
#include "imap.H"
#include "misc.H"
#include "generic.H"
#include "genericdecode.H"
#include "runlater.H"
#include "envelope.H"
#include "structure.H"
#include "rfcaddr.H"
#include "runlater.H"
#include "objectmonitor.H"

#include "rfc822/rfc822.h"
#include "rfc2045/rfc2045.h"
#include "maildir/maildirkeywords.h"
#include <errno.h>
#include <ctype.h>
#include <sstream>
#include <queue>
#include <set>
#include <string>

using namespace std;

mail::generic::generic()
{
}

mail::generic::~generic()
{
}

//////////////////////////////////////////////////////////////////////////////
//
// mail::generic::genericAttributes() creates an Attributes object.
//
// The Attributes objects takes the original list of messages.  For each
// message, genericGetMessage functions are invoked to grab the message,
// then synthesize the attributes.

class mail::generic::Attributes : public mail::callback::message,
		     public mail::runLater {

	std::shared_ptr<rfc822::fdstreambuf> fd;
	std::shared_ptr<rfc2045::entity> rfc2045p;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal) override;

public:

	mail::ptr<mail::account> account;		// My account
	mail::generic *genericInterface;	// Ditto

	mail::envelope envelope;
	mail::mimestruct structure;

	time_t arrivalDate;

	vector< pair<string, size_t> > messages;

	vector< pair<string, size_t> >::iterator nextMsg;

	mail::account::MessageAttributes attributes, // Attributes requested

		attributesToDo; // Attributes left to synthesize for cur msg

	mail::callback::message *callback;	// Original app callback

	Attributes(mail::account *accountArg,
		   mail::generic *genericInterfaceArg,
		   const vector<size_t> &msgsArg,
		   mail::account::MessageAttributes attributesArg,
		   mail::callback::message *callbackArg);
	~Attributes();

	void success(string msg) override;
	void fail(string msg) override;
	void RunningLater() override;
	void go(string);

	string headerBuffer;
	void messageTextCallback(size_t n, string text) override;

	void messageSizeCallback(size_t messageNumber,
				 unsigned long size) override;
};

mail::generic::Attributes::Attributes(mail::account *accountArg,
				      mail::generic *genericInterfaceArg,
				      const vector<size_t> &msgsArg,
				      mail::account::MessageAttributes attributesArg,
				      mail::callback::message *callbackArg)
	: account(accountArg),
	  genericInterface(genericInterfaceArg),
	  attributes(attributesArg),
	  callback(callbackArg)
{
	// Memorize all message numbers and uids

	vector<size_t>::const_iterator b=msgsArg.begin(), e=msgsArg.end();

	while (b != e)
	{
		size_t n= *b++;

		messages.push_back( make_pair(accountArg->getFolderIndexInfo(n)
					      .uid, n));
	}
	nextMsg=messages.begin();
	attributesToDo=attributes;
}

mail::generic::Attributes::~Attributes()
{
	mail::callback::message *c=callback;

	callback=NULL;

	if (c)
		c->fail("Server connection unexpectedly shut down.");
}


// See mail::Runlater

void mail::generic::Attributes::RunningLater()
{
	go("OK");
}

void mail::generic::Attributes::go(string msg)
{
	while (nextMsg != messages.end() && !account.isDestroyed())
	{
		if (!fixMessageNumber(account, nextMsg->first,
				      nextMsg->second)) // Msg disappeared?
		{
			attributesToDo=attributes;
			nextMsg++;
			continue;
		}

		// Pick next attributes to synthesize

		if (attributesToDo & mail::account::MESSAGESIZE)
		{
			genericInterface->genericMessageSize(nextMsg->first,
							     nextMsg->second,
							     *this);
			return;
		}


		if (attributesToDo & mail::account::MIMESTRUCTURE)
		{
			genericInterface->
				genericGetMessageFdStruct(nextMsg->first,
							  nextMsg->second,
							  true,
							  fd,
							  rfc2045p,
							  *this);
			return;
		}

		// ARRIVALDATE is synthesized from header information
		// So is the envelope

		if (attributesToDo & (mail::account::ENVELOPE | mail::account::ARRIVALDATE))
		{
			envelope=mail::envelope();
			arrivalDate=0;

			headerBuffer="";
			genericInterface->genericMessageRead(nextMsg->first,
							     nextMsg->second,
							     true,
							     mail::readHeadersFolded,
							     *this);
			return;
		}

		attributesToDo=attributes;
		nextMsg++;
		RunLater();
		return;
	}

	// All done.

	mail::callback::message *c=callback;

	callback=NULL;

	if (c)
		c->success(msg);

	delete this;
	return;
}

void mail::generic::Attributes::success(string msg)
{
	if (account.isDestroyed())
	{
		delete this;
		return;
	}

	// Must use the same order as go(), in order to figure out which
	// attribute to synthesize.  The synthesized attribute is turned off.
	// Lather, rinse, repeat.

	if (attributesToDo & mail::account::MESSAGESIZE)
	{
		attributesToDo &= ~mail::account::MESSAGESIZE;
		go(msg);
		return;
	}

	bool goodMsg=fixMessageNumber(account, nextMsg->first,
				      nextMsg->second);

	if (attributesToDo & mail::account::MIMESTRUCTURE)
	{
		attributesToDo &= ~mail::account::MIMESTRUCTURE;

		mail::mimestruct s;

		genericMakeMimeStructure(s, fd, *rfc2045p, "", NULL);

		if (callback && goodMsg)
			callback->messageStructureCallback(nextMsg->second,
							   s);
		go(msg);
		return;
	}

	if (attributesToDo & mail::account::ENVELOPE)
		if (callback && goodMsg)
			callback->messageEnvelopeCallback(nextMsg->second,
							  envelope);

	if ((attributesToDo & mail::account::ARRIVALDATE) && callback && goodMsg)
	{
		if (arrivalDate != 0) // First date in Received: hdr seen.
			callback->messageArrivalDateCallback(nextMsg->second,
							     arrivalDate);
		else if (envelope.date != 0)
			callback->messageArrivalDateCallback(nextMsg->second,
							     envelope.date);
	}

	attributesToDo &= ~(mail::account::ENVELOPE | mail::account::ARRIVALDATE);
	go(msg);
}

void mail::generic::Attributes::fail(string msg)
{
	mail::callback::message *c=callback;

	callback=NULL;

	delete this;

	if (c)
		c->fail(msg);
}

void mail::generic::Attributes::reportProgress(size_t bytesCompleted,
					       size_t bytesEstimatedTotal,

					       size_t messagesCompleted,
					       size_t messagesEstimatedTotal)
{
	callback->reportProgress(bytesCompleted, bytesEstimatedTotal,
				 nextMsg - messages.begin(),
				 messages.size());
}

void mail::generic::Attributes::messageSizeCallback(size_t messageNumber,
						    unsigned long size)
{
	if (fixMessageNumber(account, nextMsg->first,
			     nextMsg->second))
		callback->messageSizeCallback(nextMsg->second, size);
}

// This messageTextCallback is used for reading headers.

void mail::generic::Attributes::messageTextCallback(size_t dummy,
						    string text)
{
	headerBuffer += text;

	size_t n;

	while ((n=headerBuffer.find('\n')) != std::string::npos)
	{
		string header=headerBuffer.substr(0, n);

		headerBuffer=headerBuffer.substr(n+1);

		n=header.find(':');

		string value="";

		if (n != std::string::npos)
		{
			size_t nsave=n++;

			while (n < header.size() &&
			       unicode_isspace((unsigned char)header[n]))
				n++;

			value=header.substr(n);
			header=header.substr(0, nsave);
		}

		rfc2045::entity::tolowercase(header);
		genericBuildEnvelope(header, value, envelope);
		// Accumulate envelope information

		if (strcasecmp(header.c_str(), "Received:") == 0 &&
		    arrivalDate == 0)
		{
			// Attempt to synthesize arrival date, based on the
			// first parsed Received: header.

			size_t n=value.size();

			while (n > 0)
				if (value[--n] == ';')
				{
					rfc822_parsedate_chk(value.c_str()
							     + n + 1,
							     &arrivalDate);
					break;
				}
		}

	}
}

//
// Accumulate header lines into the envelope structure
//

void mail::generic::genericBuildEnvelope(string_view header, string_view value,
					 mail::envelope &envelope)
{
	if (header == "from")
	{
		size_t dummy;

		mail::address::fromString({value.begin(), value.end()},
					  envelope.from, dummy);
	}
	else if (header == "sender")
	{
		size_t dummy;

		mail::address::fromString({value.begin(), value.end()},
					  envelope.sender,
					  dummy);
	}
	else if (header == "reply-to")
	{
		size_t dummy;

		mail::address::fromString({value.begin(), value.end()},
					  envelope.replyto,
					  dummy);
	}
	else if (header == "to")
	{
		size_t dummy;

		mail::address::fromString({value.begin(), value.end()},
					  envelope.to,
					  dummy);
	}
	else if (header == "cc")
	{
		size_t dummy;

		mail::address::fromString({value.begin(), value.end()},
					  envelope.cc,
					  dummy);
	}
	else if (header == "bcc")
	{
		size_t dummy;

		mail::address::fromString({value.begin(), value.end()},
					  envelope.bcc,
					  dummy);
	}
	else if (header == "in-reply-to")
	{
		envelope.inreplyto={value.begin(), value.end()};
	}
	else if (header == "message-id")
	{
		envelope.messageid={value.begin(), value.end()};
	}
	else if (header == "subject")
	{
		envelope.subject={value.begin(), value.end()};
	}
	else if (header == "date")
	{
		rfc822_parsedate_chk(
			std::string{value.begin(), value.end()}.c_str(),
			&envelope.date);
	}
	else if (header == "references")
	{
		vector<address> address_list;
		size_t err_index;

		address::fromString({value.begin(), value.end()},
				    address_list, err_index);

		envelope.references.clear();

		vector<address>::iterator
			b=address_list.begin(),
			e=address_list.end();

		while (b != e)
		{
			string s=b->getAddr();

			if (s.size() > 0)
				envelope.references
					.push_back("<" + s + ">");
			++b;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// A reusable class that reads a raw message from a file descriptor, and:
//
// A) gets rids of the carriage returns.
//
// B) If only the header portion is requested, stop at the first blank line.
//

class mail::generic::CopyMimePart {

	char input_buffer[BUFSIZ];
	char output_buffer[BUFSIZ];

public:
	CopyMimePart();
	virtual ~CopyMimePart();

	bool copy(const std::shared_ptr<rfc822::fdstreambuf> &fd, off_t *cnt,
		  mail::ptr<mail::account> &account,
		  mail::readMode readType,
		  mail::callback::message *callback);

	virtual void copyto(string)=0;	// Cooked text goes here
};

mail::generic::CopyMimePart::CopyMimePart()
{
}

mail::generic::CopyMimePart::~CopyMimePart()
{
}

bool mail::generic::CopyMimePart::copy(
	// File descriptor to copy from
	const std::shared_ptr<rfc822::fdstreambuf> &fd,
	off_t *cnt, // Not null - max byte count
	mail::ptr<mail::account> &account,
	// The canary (drops dead, we're done)

	mail::readMode readType,
	mail::callback::message *callback
	// Original callback
)
{
	int n=0;
	size_t output_ptr=0;
	char lastCh='\n';
	bool inHeaders=true;
	bool foldingHeader=false;

	while (cnt == NULL || *cnt > 0)
	{
		if (cnt == NULL || (off_t)sizeof(input_buffer) < *cnt)
			n=sizeof(input_buffer);
		else
			n= (int) *cnt;

		n=fd->sgetn(input_buffer, n);

		if (n <= 0)
			break;

		if (cnt)
			*cnt -= n;

		int i;

		for (i=0; i<n; i++)
		{
			if (input_buffer[i] == '\r')
				continue;

			bool endingHeaders=
				inHeaders &&
				input_buffer[i] == '\n' && lastCh == '\n';

			if (inHeaders ?
			    readType != mail::readContents:
			    readType == mail::readContents ||
			    readType == mail::readBoth)
			{
				if (inHeaders && readType ==
				    mail::readHeadersFolded)
					// Fold headers
				{
					if (output_ptr > 0 &&
					    output_buffer[output_ptr-1]
					    == '\n' && input_buffer[i] != '\n'
					    && unicode_isspace((unsigned char)
						       input_buffer[i]))
					{
						output_buffer[output_ptr-1]
							=' ';
						foldingHeader=true;
					}
				}

				if (foldingHeader &&
				    (input_buffer[i] == '\n' ||
				     !unicode_isspace((unsigned char)
					     input_buffer[i])))
				{
					foldingHeader=false;
				}

				if (!foldingHeader)
				{
					if (output_ptr >= sizeof(output_buffer)
					    || endingHeaders)
					{
						if (output_ptr > 0)
							copyto(string(output_buffer,
								      output_buffer +
								      output_ptr));

						if (account.isDestroyed())
							return false;

						output_ptr=0;
					}
					output_buffer[output_ptr++]=input_buffer[i];
				}
			}

			if (endingHeaders)
				inHeaders=false;

			lastCh=input_buffer[i];
		}
	}

	if (n < 0)
		return false;

	if (output_ptr > 0)
	{
		copyto(string(output_buffer, output_buffer + output_ptr));

		if (account.isDestroyed())
			return false;

		if (foldingHeader)
			copyto(string("\n"));
		if (account.isDestroyed())
			return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
// The ReadMultiple object implements the multiple message version of
// genericReadMessageContent().


class mail::generic::ReadMultiple : public mail::callback::message,
		     public mail::generic::CopyMimePart,
		     public mail::runLater {

	mail::ptr<mail::account> account;
	mail::generic *generic;

	bool peek;
	enum mail::readMode readType;

	size_t completedCnt;

	mail::callback::message *callback;

	std::shared_ptr<rfc822::fdstreambuf> temp_fd;

	// Callback for when genericGetMessageFd() is used

	class TempFileCallback : public mail::callback {
		ReadMultiple *me;

	public:
		TempFileCallback(ReadMultiple *meArg);
		~TempFileCallback();

		void success(string) override;
		void fail(string) override;

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal) override;

	};

	TempFileCallback temp_callback;

	void copyto(string) override;

	string uid;
	size_t messageNumber;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal) override;

public:
	size_t totalCnt;
	bool useTempFile;
	// True if the contents of exactly one message were requested.
	// Use genericGetMessageFd() instead of genericMessageRead(), perhaps
	// the dumb driver will be able to take advantage of this.

	queue< pair<size_t, string> > messageq; // msg num/msg uid

	ReadMultiple(mail::account *accountArg, mail::generic *genericArg,
		     bool peekArg,
		     enum mail::readMode readTypeArg,
		     mail::callback::message &callbackArg,
		     size_t totalCntArg);

	~ReadMultiple();

	void success(string) override;
	void fail(string) override;
	void messageTextCallback(size_t n, string text) override;
	void RunningLater() override;

	void processTempFile(string);
};

mail::generic::ReadMultiple::TempFileCallback::TempFileCallback(ReadMultiple
								*meArg)
	: me(meArg)
{
}

mail::generic::ReadMultiple::TempFileCallback::~TempFileCallback()
{
}

void mail::generic::ReadMultiple::TempFileCallback::success(string msg)
{
	me->processTempFile(msg);
}

void mail::generic::ReadMultiple::TempFileCallback::fail(string msg)
{
	me->fail(msg);
}

void mail::generic::ReadMultiple
::TempFileCallback::reportProgress(size_t bytesCompleted,
				   size_t bytesEstimatedTotal,

				   size_t messagesCompleted,
				   size_t messagesEstimatedTotal)
{
	me->reportProgress(bytesCompleted, bytesEstimatedTotal,
			   messagesCompleted,
			   messagesEstimatedTotal);
}

mail::generic::ReadMultiple::ReadMultiple(mail::account *accountArg,
					  mail::generic *genericArg,
					  bool peekArg,
					  enum mail::readMode readTypeArg,
					  mail::callback::message
					  &callbackArg,
					  size_t totalCntArg)
	: account(accountArg),
	  generic(genericArg),
	  peek(peekArg),
	  readType(readTypeArg),
	  completedCnt(0),
	  callback(&callbackArg),
	  temp_callback(this),
	  totalCnt(totalCntArg),
	  useTempFile(false)
{
}

mail::generic::ReadMultiple::~ReadMultiple()
{
}

void mail::generic::ReadMultiple::success(string s)
{
	completedCnt++;

	try {
		if (!messageq.empty())
		{
			RunLater();
			return;
		}
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	mail::callback *c=callback;
	callback=NULL;

	delete this;
	c->success(s);
}

void mail::generic::ReadMultiple::reportProgress(size_t bytesCompleted,
						 size_t bytesEstimatedTotal,

						 size_t messagesCompleted,
						 size_t messagesEstimatedTotal)
{
	callback->reportProgress(bytesCompleted, bytesEstimatedTotal,
				 completedCnt, totalCnt);
}

void mail::generic::ReadMultiple::RunningLater()
{
	try {
		if (account.isDestroyed())
		{
			fail("Server connection unexpectedly shut down.");
			return;
		}

		if (!fixMessageNumber(account, messageq.front().second,
				      messageq.front().first))
		{
			messageq.pop();
			success("OK");
			return;
		}

		uid=messageq.front().second;
		messageNumber=messageq.front().first;

		messageq.pop();

		if (!peek)
		{
			ptr<mail::account> a(account);

			generic->genericMarkRead(messageNumber);

			if (a.isDestroyed())
			{
				fail("Aborted.");
				return;
			}
		}

		if (!useTempFile)
			generic->genericMessageRead(uid,
						    messageNumber,
						    peek,
						    readType,
						    *this);
		else
			generic->genericGetMessageFd(uid,
						     messageNumber,
						     peek,
						     temp_fd,
						     temp_callback);
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::generic::ReadMultiple::messageTextCallback(size_t n,
						      string text)
{
	callback->messageTextCallback(messageNumber, text);
}

// Extract the read data.

void mail::generic::ReadMultiple::processTempFile(string s)
{
	try {
		if (temp_fd->pubseekpos(0) != 0)
		{
			fail(strerror(errno));
			return;
		}

		if (!copy(temp_fd, NULL, account, readType, callback))
		{
			fail("Server connection unexpectedly shut down.");
			return;
		}
	} catch (...) {
		delete this;

		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	success(s);
}

void mail::generic::ReadMultiple::copyto(string s)
{
	callback->messageTextCallback(messageNumber, s);
}

void mail::generic::ReadMultiple::fail(string s)
{
	mail::callback *c=callback;
	callback=NULL;

	delete this;
	c->fail(s);
}

//////////////////////////////////////////////////////////////////////////////
//
// This object implements the MIME section version of
//  genericReadMessageContent.

class mail::generic::ReadMimePart : public mail::callback,
		     public mail::generic::CopyMimePart {

	mail::ptr<mail::account> account;
	mail::generic *generic;

	size_t messageNum;

	string mime_id;

	enum mail::readMode readType;

	mail::callback::message *callback;

	void copyto(string) override;

	bool copyHeaders(rfc2045::entity *);
	bool copyContents(rfc2045::entity *);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal) override;

public:
	std::shared_ptr<rfc822::fdstreambuf> fd;
	std::shared_ptr<rfc2045::entity> rfcp;

	ReadMimePart(mail::account *account, mail::generic *generic,
		     size_t messageNum,
		     string mime_id,
		     enum mail::readMode readTypeArg,
		     mail::callback::message *callback);

	~ReadMimePart();

	void success(string message) override;
	void fail(string message) override;
};

mail::generic::ReadMimePart::ReadMimePart(mail::account *accountArg,
					  mail::generic *genericArg,
					  size_t messageNumArg,
					  string mime_idArg,
					  // from mail::mimestruct

					  enum mail::readMode readTypeArg,
					  mail::callback::message
					  *callbackArg)
	: account(accountArg),
	  generic(genericArg),
	  messageNum(messageNumArg),
	  mime_id(mime_idArg),
	  readType(readTypeArg),
	  callback(callbackArg)
{
}

mail::generic::ReadMimePart::~ReadMimePart()
{
}

void mail::generic::ReadMimePart::fail(string message)
{
	mail::callback *c=callback;

	callback=NULL;

	delete this;
	c->fail(message);
}

void mail::generic::ReadMimePart::success(string message)
{
	const char *p=mime_id.c_str();

	// Parse the synthesized MIME id, and locate the relevant rfc2045
	// tructure.

	rfc2045::entity *ptr=&*rfcp;

	while (ptr && *p)
	{
		unsigned partNumber=0;

		while (*p && isdigit((int)(unsigned char)*p))
			partNumber=partNumber * 10 + (*p++ - '0');

		if (*p)
			p++;

		if (partNumber < 0 || partNumber > ptr->subentities.size())
		{
			ptr=nullptr;
			break;
		}

		ptr= &ptr->subentities[partNumber-1];
	}

	bool rc=true;

	if (ptr)
		try {
			rc=readType == mail::readBoth ||
				readType == mail::readContents
				? copyContents(ptr):copyHeaders(ptr);
		} catch (...) {
			delete this;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

	mail::callback *c=callback;

	callback=NULL;

	delete this;
	if (rc)
		c->success(message);
	else
		c->fail(message);
}

// Return just the header portion.

bool mail::generic::ReadMimePart::copyHeaders(rfc2045::entity *ptr)
{
	rfc2045::entity::line_iter<false>::headers headers{*ptr, *fd};

	headers.name_lc=false;

	if (readType == mail::readHeaders)
		headers.keep_eol=true;

	do {
		auto h=headers.current_header();

		if (h.size() == 0 || h == "\n")
			break;

		std::string recovered_header{h.begin(), h.end()};

		if (!headers.keep_eol)
			// Otherwise the trailing one is preserved
			recovered_header += "\n";
		copyto(std::move(recovered_header));
	} while (headers.next());

	return true;
}

// Return body end/or header.

bool mail::generic::ReadMimePart::copyContents(rfc2045::entity *ptr)
{
	if (static_cast<size_t>(fd->pubseekpos(ptr->startpos)) != ptr->startpos)
		return false;

	off_t end_pos=ptr->endbody-ptr->startpos;

	return copy(fd, &end_pos, account, readType, callback);
}

void mail::generic::ReadMimePart::reportProgress(size_t bytesCompleted,
						 size_t bytesEstimatedTotal,

						 size_t messagesCompleted,
						 size_t messagesEstimatedTotal)
{
	callback->reportProgress(bytesCompleted, bytesEstimatedTotal,
				 messagesCompleted,
				 messagesEstimatedTotal);
}

void mail::generic::ReadMimePart::copyto(string s)
{
	callback->messageTextCallback(messageNum, s);
}

//////////////////////////////////////////////////////////////////////////////
//
// Default genericGetMessageFdStruct() implementation.  First, invoke
// genericGetMessageFd(), with the following object used as the callback.
// The GetMessageFdStruct then immediately invoked genericGetMessageStruct.
//

class mail::generic::GetMessageFdStruct : public mail::callback {

	string uid;
	size_t messageNumber;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal) override;

public:
	mail::generic *generic;
	mail::callback *callback;

	std::shared_ptr<rfc822::fdstreambuf> &fd;
	std::shared_ptr<rfc2045::entity> &rfc2045p;

	GetMessageFdStruct(string uidArg,
			   size_t messageNumberArg,
			   mail::generic *genericArg,
			   mail::callback *callbackArg,
			   std::shared_ptr<rfc822::fdstreambuf> &fdArg,
			   std::shared_ptr<rfc2045::entity> &rfc2045pArg);
	~GetMessageFdStruct();

	void success(string) override;
	void fail(string) override;
};

mail::generic::GetMessageFdStruct::GetMessageFdStruct(
	string uidArg,
	size_t messageNumberArg,
	mail::generic *genericArg,
	mail::callback *callbackArg,
	std::shared_ptr<rfc822::fdstreambuf> &fdArg,
	std::shared_ptr<rfc2045::entity> &rfc2045pArg
) : uid(uidArg),
    messageNumber(messageNumberArg),
    generic(genericArg),
    callback(callbackArg),
    fd(fdArg),
    rfc2045p(rfc2045pArg)
{
	fd={};
	rfc2045p={};
}

mail::generic::GetMessageFdStruct::~GetMessageFdStruct()
{
}

void mail::generic::GetMessageFdStruct::success(string message)
{
	if (!rfc2045p)
	{
		generic->genericGetMessageStruct(uid, messageNumber,
					  rfc2045p, *this);
		return;
	}

	mail::callback *c=callback;

	callback=NULL;
	delete this;
	c->success(message);
}

void mail::generic::GetMessageFdStruct::fail(string message)
{
	mail::callback *c=callback;

	callback=NULL;
	delete this;
	c->fail(message);
}

void mail::generic
::GetMessageFdStruct::reportProgress(size_t bytesCompleted,
				     size_t bytesEstimatedTotal,

				     size_t messagesCompleted,
				     size_t messagesEstimatedTotal)
{
	callback->reportProgress(bytesCompleted, bytesEstimatedTotal,
				 messagesCompleted,
				 messagesEstimatedTotal);
}


//////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATIONS

void mail::generic::genericGetMessageFdStruct(
	string uid,
	size_t messageNumber,
	bool peek,
	std::shared_ptr<rfc822::fdstreambuf> &fdRet,
	std::shared_ptr<rfc2045::entity> &structRet,
	mail::callback &callback)
{
	GetMessageFdStruct *s=new GetMessageFdStruct(uid, messageNumber,
						     this,
						     &callback,
						     fdRet,
						     structRet);

	if (!s)
	{
		callback.fail("Out of memory.");
		return;
	}

	try {
		genericGetMessageFd(uid, messageNumber, peek, fdRet, *s);
	} catch (...) {
		delete s;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::generic::genericAttributes(mail::account *account,
				      mail::generic *genericInterface,
				      const vector<size_t> &msgs,
				      mail::account::MessageAttributes attributes,
				      mail::callback::message &callback)
{
	Attributes *a=new Attributes(account, genericInterface, msgs,
				     attributes, &callback);

	if (!a)
		LIBMAIL_THROW("Out of memory.");

	a->go("OK");
}


bool mail::generic::fixMessageNumber(mail::account *account,
				     string uid,
				     size_t &msgNum)
{
	if (!account)
		return false;

	size_t n=account->getFolderIndexSize();

	if (n > msgNum &&
	    account->getFolderIndexInfo(msgNum).uid == uid)
		return true;

	while (n)
	{
		--n;
		if (account->getFolderIndexInfo(n).uid == uid)
		{
			msgNum=n;
			return true;
		}
	}

	return false;
}

void mail::generic::genericReadMessageContent(mail::account *account,
					      mail::generic *generic,
					      const vector<size_t> &messages,
					      bool peek,
					      enum mail::readMode readType,
					      mail::callback::message
					      &callback)
{
	ReadMultiple *r=new ReadMultiple(account, generic,
					 peek, readType,
					 callback, messages.size());

	if (!r)
	{
		callback.fail(strerror(errno));
		return;
	}

	try {
		vector<size_t>::const_iterator b=messages.begin(), e=messages.end();
		while (b != e)
		{
			size_t n= *b++;

			r->messageq.push( make_pair(n, account->
						    getFolderIndexInfo(n).uid)
					  );
		}

		if (messages.size() == 1 &&
		    (readType == mail::readBoth ||
		     readType == mail::readContents ||
		     generic->genericCachedUid(r->messageq.front().second)
		     ))
			r->useTempFile=true; // Maybe we can cache this

		r->success("OK");
	} catch (...) {
		delete r;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

// Use mail::generic::Decode to synthesize decoded content from
// genericReadMessageContent

void mail::generic::genericReadMessageContentDecoded(mail::account *account,
						     mail::generic *generic,
						     size_t messageNum,
						     bool peek,
						     const mail::mimestruct
						     &info,
						     mail::callback::message
						     &callback)
{
	mail::generic::Decode *d=
		new mail::generic::Decode(callback,
					  info.content_transfer_encoding);

	if (!d)
		LIBMAIL_THROW("Out of memory.");

	try {
		genericReadMessageContent(account, generic,
					  messageNum, peek, info,
					  mail::readContents, *d);
	} catch (...)
	{
		delete d;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::generic::genericReadMessageContent(mail::account *account,
					      mail::generic *generic,
					      size_t messageNum,
					      bool peek,
					      const mail::mimestruct &msginfo,
					      enum mail::readMode readType,
					      mail::callback::message
					      &callback)
{
	ptr<mail::account> a(account);

	if (!peek)
		generic->genericMarkRead(messageNum);

	if (a.isDestroyed())
	{
		callback.fail("Aborted.");
		return;
	}

	ReadMimePart *m=new ReadMimePart(account, generic, messageNum,
					 msginfo.mime_id,
					 readType,
					 &callback);
	if (!m)
	{
		callback.fail(strerror(errno));
		return;
	}

	try {
		string uid=account->getFolderIndexInfo(messageNum).uid;

		generic->genericGetMessageFdStruct(uid, messageNum, peek,
						   m->fd,
						   m->rfcp,
						   *m);

	} catch (...) {
		delete m;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}


///////////////////////////////////////////////////////////////////////////////
//
// Convert rfc2045 parse structure to a mail::mimestruct.  A simple
// mechanism is used to synthesize mime IDs.

void mail::generic::genericMakeMimeStructure(
	mail::mimestruct &s,
	std::shared_ptr<rfc822::fdstreambuf> fd,
	rfc2045::entity &rfc2045p,
	string mime_id,
	mail::envelope *envelopePtr)
{
	s=mail::mimestruct();

	s.mime_id=mime_id;

	s.type="text/plain"; // Fixed below

	s.content_transfer_encoding="8BIT";

	// Now read the headers, and figure out the rest

	rfc2045::entity::line_iter<false>::headers headers{rfc2045p, *fd};

	s.content_size=rfc2045p.endbody - rfc2045p.startbody;
	s.content_lines=rfc2045p.nbodylines;
	s.smtputf8=rfc2045p.errors.code & RFC2045_ERR8BITHEADER ? true:false;

	do {
		const auto &[header, value] = headers.name_content();

		if (header == "content-id")
		{
			s.content_id=value;
			continue;
		}

		if (header == "content-description")
		{
			s.content_description={value.begin(), value.end()};
			continue;
		}

		if (header == "content-transfer-encoding")
		{
			s.content_transfer_encoding={value.begin(),
						     value.end()};
			continue;
		}

		if (header == "content-md5")
		{
			s.content_md5={value.begin(), value.end()};
			continue;
		}

		if (header == "content-language")
		{
			s.content_id={value.begin(), value.end()};
			continue;
		}

		string *name;

		mail::mimestruct::parameterList *attributes;

		if (header == "content-type")
		{
			name= &s.type;
			attributes= &s.type_parameters;
		}
		else if (header == "content-disposition")
		{
			name= &s.content_disposition;
			attributes= &s.content_disposition_parameters;
		}
		else
		{
			if (envelopePtr)
				genericBuildEnvelope(header, value,
						     *envelopePtr);
			continue;
		}

		rfc2045::entity::rfc2231_header parsed_header{value};

		*name=parsed_header.value;
		mail::upper(*name);

		for (const auto &[key, value] : parsed_header.parameters)
		{
			attributes->set_simple(key, value.value);
		}
	} while (headers.next());

	// Fix content type/subtype

	size_t n=s.type.find('/');

	if (n != std::string::npos)
	{
		s.subtype=s.type.substr(n+1);
		s.type=s.type.substr(0, n);
	}

	mail::upper(s.type);
	mail::upper(s.subtype);
	mail::upper(s.content_transfer_encoding);

	// Now, parse the subsections
	//

	mail::envelope *env=NULL;

	if (s.messagerfc822())
		env= &s.getEnvelope(); // Subsection needs an envelope

	if (mime_id.size() > 0)
		mime_id += ".";

	size_t cnt=1;

	for (auto subentity:rfc2045p.subentities)
	{
		string buffer;

		{
			ostringstream o;

			o << cnt++;
			buffer=o.str();
		}

		genericMakeMimeStructure( *s.addChild(),
					  fd,
					  subentity,
					  mime_id + buffer,
					  env );
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// Generic remove messages for accounts that use mark deleted/expunge
// paradigm.

class mail::generic::Remove : public mail::callback {

	mail::ptr<mail::account> acct;

	set<string> msgs;

	mail::callback *callback;

	void expunge(string);
	void restore(string);
	void done(string);

	void (mail::generic::Remove::*success_func)(string);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,
			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal) override;

public:
	void success(string message) override;
	void fail(string message) override;

	Remove(mail::account *acctArg,
	       const vector<size_t> &messages,
	       mail::callback *callbackArg);
	~Remove();

	void mkMessageList(vector<size_t> &msgList);
};


mail::generic::Remove::Remove(mail::account *acctArg,
			      const std::vector<size_t> &messages,
			      mail::callback *callbackArg)
	: acct(acctArg), callback(callbackArg),
	  success_func(&mail::generic::Remove::expunge)
{
	vector<size_t>::const_iterator b=messages.begin(), e=messages.end();

	size_t n= acct->getFolderIndexSize();

	while (b != e)
	{
		if (*b < n)
			msgs.insert(acct->getFolderIndexInfo(*b).uid);
		b++;
	}
}

mail::generic::Remove::~Remove()
{
	if (callback)
	{
		mail::callback * volatile p=callback;

		callback=NULL;
		p->success("OK");
	}
}

void mail::generic::Remove::expunge(std::string message)
{
	success_func= &mail::generic::Remove::restore;
	acct->updateFolderIndexInfo( *this );
}

void mail::generic::Remove::restore(std::string message)
{
	success_func= &mail::generic::Remove::done;

	vector<size_t> flipDeleted;

	mkMessageList(flipDeleted);

	if (flipDeleted.size() > 0)
	{
		messageInfo flags;

		flags.deleted=true;

		acct->updateFolderIndexFlags(flipDeleted, false,
					     true, flags, *this);
		return;
	}
	done(message);
}

void mail::generic::Remove::success(std::string message)
{
	if (acct.isDestroyed())
	{
		delete this;
		return;
	}
	(this->*success_func)(message);
}

void mail::generic::Remove::fail(std::string message)
{
	if (callback)
	{
		mail::callback * volatile p=callback;

		callback=NULL;
		p->fail(message);
	}
}

void mail::generic::Remove::done(std::string message)
{
	if (callback)
	{
		mail::callback * volatile p=callback;

		callback=NULL;
		p->success(message);
	}
	delete this;
}

void mail::generic::Remove::reportProgress(size_t bytesCompleted,
					   size_t bytesEstimatedTotal,
					   size_t messagesCompleted,
					   size_t messagesEstimatedTotal)
{
	if (callback)
		callback->reportProgress(bytesCompleted,
					 bytesEstimatedTotal,
					 messagesCompleted,
					 messagesEstimatedTotal);
}



void mail::generic::Remove::mkMessageList(std::vector<size_t> &msgList)
{
	mail::account *p=acct;

	if (!p)
		return;

	size_t n=p->getFolderIndexSize();
	size_t i;

	for (i=0; i<n; i++)
		if (msgs.count(p->getFolderIndexInfo(i).uid) > 0)
			msgList.push_back(i);
}


void mail::generic::genericRemoveMessages(mail::account *account,
					  const std::vector<size_t> &messages,
					  callback &cb)
{
	vector<size_t> msgToDo;

	{
		set<size_t> msgSet;

		msgSet.insert(messages.begin(), messages.end());

		size_t i, n=account->getFolderIndexSize();

		for (i=0; i<n; i++)
		{
			bool toDelete=msgSet.count(i) > 0;

			if (toDelete != account->getFolderIndexInfo(i).deleted)
				msgToDo.push_back(i);
		}
	}

	Remove *r=new Remove(account, msgToDo, &cb);

	if (!r)
	{
		cb.fail(strerror(errno));
		return;
	}

	try
	{
		vector<size_t> flipDeleted;

		r->mkMessageList(flipDeleted);

		if (flipDeleted.size() > 0)
		{
			messageInfo flags;

			flags.deleted=true;

			account->updateFolderIndexFlags(flipDeleted, true,
							false, flags,
							*r);
			return;
		}

		r->success("OK");
	} catch (...) {
		delete r;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// Generic 'move messages' for accounts that do not support a message 'move',
// or for moves between accounts.  This is implemented as a COPY, followed by
// a Remove.

class mail::generic::Move : public mail::callback {

	mail::ptr<mail::account> acct;

	set<string> msgs;

	mail::callback *callback;

public:
	Move(mail::account *acctArg,
	     const vector<size_t> &messages,
	     mail::callback *callbackArg);
	~Move();

	void success(string message) override;
	void fail(string message) override;
	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,
			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal) override;
};

mail::generic::Move::Move(mail::account *acctArg,
			  const vector<size_t> &messages,
			  mail::callback *callbackArg)
	: acct(acctArg), callback(callbackArg)
{
	vector<size_t>::const_iterator b=messages.begin(), e=messages.end();
	size_t n= acct->getFolderIndexSize();

	while (b != e)
	{
		if (*b < n)
			msgs.insert(acct->getFolderIndexInfo(*b).uid);
		b++;
	}
}

mail::generic::Move::~Move()
{
	if (callback)
	{
		mail::callback * volatile p=callback;

		callback=NULL;
		p->fail("Exception caught in generic::Move::success");
	}
}

void mail::generic::Move::reportProgress(size_t bytesCompleted,
					 size_t bytesEstimatedTotal,
					 size_t messagesCompleted,
					 size_t messagesEstimatedTotal)
{
	if (callback)
		callback->reportProgress(bytesCompleted,
					 bytesEstimatedTotal,
					 messagesCompleted,
					 messagesEstimatedTotal);
}

void mail::generic::Move::success(string message)
{
	if (!acct.isDestroyed() && callback)
	{
		vector<size_t> msgList;

		size_t n=acct->getFolderIndexSize();
		size_t i;

		for (i=0; i<n; i++)
			if (msgs.count(acct->getFolderIndexInfo(i).uid) > 0)
				msgList.push_back(i);

		if (msgs.size() > 0)
		{
			mail::callback * volatile p=callback;
			mail::account * volatile a=acct;

			callback=NULL;
			delete this;

			try {
				a->removeMessages(msgList, *p);
			} catch (...) {
				p->fail("Exception caught in generic::Move::success");
			}
			return;
		}
	}

	if (callback)
	{
		mail::callback * volatile p=callback;

		callback=NULL;
		delete this;
		p->success(message);
	}
}

void mail::generic::Move::fail(string message)
{
	if (callback)
	{
		mail::callback * volatile p=callback;

		callback=NULL;
		delete this;
		p->fail(message);
	}
}

void mail::generic::genericMoveMessages(mail::account *account,
					const vector<size_t> &messages,
					mail::folder *copyTo,
					mail::callback &callback)
{
	Move *m=new Move(account, messages, &callback);

	if (!m)
	{
		callback.fail(strerror(errno));
		return;
	}

	try {
		account->copyMessagesTo(messages, copyTo, *m);
	} catch (...) {
		delete m;
	}
}

//////////////////////////////////////////////////////////////////////////

mail::generic::updateKeywordHelper::updateKeywordHelper(const set<string> &keywordsArg,
							bool setOrChangeArg,
							bool changeToArg)
	: keywords(keywordsArg),
	  setOrChange(setOrChangeArg),
	  changeTo(changeToArg)
{
}

mail::generic::updateKeywordHelper::~updateKeywordHelper()
{
}

bool mail::generic::updateKeywordHelper
::doUpdateKeyword(mail::keywords::Message &keyWords,
		  mail::keywords::Hashtable &h)
{
	if (!setOrChange)
		return keyWords.setFlags(h, keywords);

	set<string>::iterator b=keywords.begin(), e=keywords.end();

	while (b != e)
	{
		if (!(changeTo ? keyWords.addFlag(h, *b):
		      keyWords.remFlag(*b)))
			return false;
		++b;
	}

	return true;
}

bool mail::generic::genericProcessKeyword(size_t messageNumber,
					  updateKeywordHelper &helper)
{
	return true;
}

void mail::generic::genericUpdateKeywords(const vector<size_t> &messages,
					  const set<string> &keywords,
					  bool setOrChange,
					  // false: set, true: see changeTo
					  bool changeTo,
					  mail::callback::folder
					  *folderCallback,
					  mail::generic *generic,
					  mail::callback &cb)
{
	vector<size_t>::const_iterator b=messages.begin(),
		e=messages.end();

	updateKeywordHelper helper(keywords, setOrChange, changeTo);

	while (b != e)
	{
		--e;
		if (!generic->genericProcessKeyword(*e, helper))
		{
			cb.fail(strerror(errno));
			return;
		}
	}

	e=messages.end();

	while (b != e)
		folderCallback->messageChanged(*--e);

	return cb.success("Ok.");
}
