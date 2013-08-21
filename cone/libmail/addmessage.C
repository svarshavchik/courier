/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "addmessage.H"
#include "attachments.H"
#include <cstring>
#include <errno.h>

// Default method implementation.

mail::addMessage::addMessage(mail::account *ptr) : mail::ptr<mail::account>(ptr),
				       messageDate(0)
{
}

bool mail::addMessage::checkServer()
{
	if (isDestroyed())
	{
		fail("Server connection closed.");
		return false;
	}

	return true;
}

mail::addMessage::~addMessage()
{
}

mail::addMessagePull::addMessagePull() : messageDate(0)
{
}

mail::addMessagePull::~addMessagePull()
{
}

//
// Default MIME assembly implementation
//

void mail::addMessage::assembleContent(size_t &handleRet,
				       const mail::Attachment &a,
				       mail::callback &cb)
{
	att_list.push_back(a);

	std::list<mail::Attachment>::iterator i=att_list.end();
	att_list_vec.push_back(--i);
	handleRet=att_list_vec.size()-1;
	cb.success("Ok.");
}

void mail::addMessage::assembleMessageRfc822(size_t &handleRet,
					     std::string headers,
					     size_t n,
					     mail::callback &cb)
{
	if (n >= att_list_vec.size())
	{
		errno=EINVAL;
		throw strerror(errno);
	}

	std::vector<mail::Attachment *> a;

	a.push_back(&*att_list_vec[n]);

	assembleContent(handleRet, Attachment(headers, a), cb);
}

void mail::addMessage::assembleMultipart(size_t &handleRet,
					 std::string headers,
					 const std::vector<size_t> &atts,
					 std::string type,
					 const mail::mimestruct
					 ::parameterList &typeParams,
					 mail::callback &cb)
{
	std::vector<Attachment *> parts;
	std::vector<size_t>::const_iterator b=atts.begin(), e=atts.end();

	while (b != e)
	{
		if (*b >= att_list_vec.size())
		{
			errno=EINVAL;
			throw strerror(errno);
		}

		parts.push_back(&*att_list_vec[*b]);
		++b;
	}

	assembleContent(handleRet, Attachment(headers, parts, type,
					      typeParams), cb);
}

bool mail::addMessage::assemble()
{
	std::vector<std::list<mail::Attachment>::iterator>::const_iterator
		e=att_list_vec.end();

	if (e == att_list_vec.begin())
		return true;

	--e;

	(*e)->begin();

	bool errflag=false;

	std::string msg;

	while ((msg=(*e)->generate(errflag)).size() > 0)
	{
		saveMessageContents(msg);
	}

	if (errflag)
		return false;
	return true;
}

bool mail::addMessage::chkMsgNum(mail::account *ptr, std::string msgUid,
				 size_t &n)
{
	size_t msgCount=ptr->getFolderIndexSize();
	size_t i;

	if (n < msgCount && ptr->getFolderIndexInfo(n).uid == msgUid)
		return true;
	for (i=1; i<msgCount; i++)
	{
		size_t j= (n+msgCount-i) % msgCount;

		if (ptr->getFolderIndexInfo(j).uid == msgUid)
		{
			n=j;
			return true;
		}
	}
	return false;
}
