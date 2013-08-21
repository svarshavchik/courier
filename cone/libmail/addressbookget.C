/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "misc.H"
#include "addressbookget.H"
#include "unicode/unicode.h"
#include <ctype.h>
#include <sstream>

using namespace std;

template<class T>
mail::addressbook::GetAddressList<T>::GetAddressList(mail::addressbook
						  *addressBookArg,
						  size_t msgNumArg,
						  std::vector<T>
						  &addrListRetArg,
						  mail::callback &callbackArg)
	: addressBook(addressBookArg),
	  msgNum(msgNumArg),
	  addrListRet(addrListRetArg),
	  callback(callbackArg)
{
}

template<class T>
mail::addressbook::GetAddressList<T>::~GetAddressList()
{
}

// 1. Read the message's MIME structure.

template<class T>
void mail::addressbook::GetAddressList<T>::go()
{
	successFunc= &mail::addressbook::GetAddressList<T>::readstructure;

	vector<size_t> msgNumVec;

	msgNumVec.push_back(msgNum);

	addressBook->server->readMessageAttributes(msgNumVec,
						   addressBook->server->
						   MIMESTRUCTURE,
						   *this);
}

template<class T>
void mail::addressbook::GetAddressList<T>::success(std::string successMsg)
{
	(this->*successFunc)(successMsg);
}

// 2. Verify the presence of text/x-libmail-addressbook content.

template<class T>
void mail::addressbook::GetAddressList<T>::readstructure(string successMsg)
{
	size_t n=mimeStructure.getNumChildren();
	mail::mimestruct *lastChild;

	if (n > 0 && (lastChild=mimeStructure.getChild(n-1))
	    -> type == "TEXT" &&
	    lastChild->subtype == "X-LIBMAIL-ADDRESSBOOK")
	{
		successFunc=&mail::addressbook::GetAddressList<T>::readContents;
		addressBook->server->readMessageContentDecoded(msgNum, false,
							       *lastChild,
							       *this);
		return;
	}

	readContents(successMsg);
}

template<class T>
void mail::addressbook::GetAddressList<T>::readContents(string successMsg)
{
	messageTextCallback(msgNum, "\n");

	size_t errIndex;

	map<string, string>::iterator p;

	p=addressBookLineMap.find("VERSION");

	int version=1;

	if (p != addressBookLineMap.end())
	{
		istringstream i(p->second);

		i >> version;
	}

	p=addressBookLineMap.find("ADDRESS");

	if (p != addressBookLineMap.end())
	{
		size_t n=addrListRet.size();

		if (!mail::address::fromString(p->second, addrListRet,
					       errIndex))
		{
			fail("Syntax error in address book entry");
			return;
		}

		if (version < 2) // Raw UTF-8
		{
			typename std::vector<T>::iterator
				b=addrListRet.begin() + n,
				e=addrListRet.end();

			while (b != e)
			{
				mail::emailAddress
					convAddress(mail::address("",
								  b->getAddr()
								  ));

				convAddress
					.setDisplayName(b->getName(), "utf-8");
				*b=convAddress;
				++b;
			}
		}
	}

	try {
		callback.success("OK");
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

template<class T>
void mail::addressbook::GetAddressList<T>::fail(std::string failMsg)
{
	try {
		callback.fail(failMsg);
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

template<class T>
void mail::addressbook::GetAddressList<T>
::messageStructureCallback(size_t messageNumber,
			   const mail::mimestruct
			   &messageStructure)
{
	mimeStructure=messageStructure;
}

//
// 3. Read text/x-libmail-addressbook content.
//

template<class T>
void mail::addressbook::GetAddressList<T>::messageTextCallback(size_t n,
							    string text)
{
	size_t i;

	while ((i=text.find('\n')) != std::string::npos)
	{
		string line=linebuffer + text.substr(0, i);

		linebuffer="";
		text=text.substr(i+1);
		addressBookLine(line);
	}

	linebuffer=text;
}

template<class T>
void mail::addressbook::GetAddressList<T>
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,
		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
	callback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				messagesCompleted, messagesEstimatedTotal);
}

// Process a single x-libmail-addressbook line.

template<class T>
void mail::addressbook::GetAddressList<T>::addressBookLine(string text)
{
	string::iterator b=text.begin(), e=text.end();
	string hdr;

	if (b == e)
		return;

	if ( unicode_isspace((unsigned char)*b))
	{
		while (b != e &&
		       unicode_isspace((unsigned char)*b))
			b++;

		text= " " + string(b, e);

		hdr=lastAddressBookLine;
	}
	else
	{
		size_t i=text.find(':');

		if (i == std::string::npos)
			return;

		hdr=text.substr(0, i);

		text=text.substr(i+1);

		while (text.size() > 0 &&
		       unicode_isspace((unsigned char)text[0]))
			text=text.substr(1);
		mail::upper(hdr);
		lastAddressBookLine=hdr;
	}

	map<string, string>::iterator p=addressBookLineMap.find(hdr);

	if (p != addressBookLineMap.end())
	{
		text= p->second + " " + text;
		addressBookLineMap.erase(p);
	}

	// Sanity check

	if (text.size() > 8000)
		text.erase(text.begin()+8000, text.end());

	if (addressBookLineMap.size() > 100)
		return;

	addressBookLineMap.insert(make_pair(hdr, text));
}

template class mail::addressbook::GetAddressList<mail::address>;
template class mail::addressbook::GetAddressList<mail::emailAddress>;
