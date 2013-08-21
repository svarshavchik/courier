/*
** Copyright 2003-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "smap.H"
#include "misc.H"
#include "smapfetchattr.H"
#include "imapfolder.H"

#include "rfcaddr.H"
#include "envelope.H"

#include "rfc822/rfc822.h"

#include <iostream>
#include <sstream>
#include <stdio.h>

using namespace std;

const char *mail::smapFETCHATTR::getName()
{
	return "FETCHATTR";
}

mail::smapFETCHATTR::smapFETCHATTR(const vector<size_t> &messages,
				   mail::account::MessageAttributes attributesArg,
				   mail::callback::message &callbackArg,
				   mail::imap &imapAccount)
	: uidSet(imapAccount, messages),
	  attributes(attributesArg), msgcallback(callbackArg),
	  nextPtr(0)
{
	defaultCB= &msgcallback;
}

mail::smapFETCHATTR::~smapFETCHATTR()
{
}

void mail::smapFETCHATTR::installed(imap &imapAccount)
{
	fetchList.init(imapAccount, uidSet);
	uidSet.clear();
	if (go())
	{
		return;
	}
	ok("OK");
	imapAccount.uninstallHandler(this);
}


bool mail::smapFETCHATTR::ok(string msg)
{
	if (doFetchingStructure)
	{
		checkMimeVersion();
		msgcallback.messageStructureCallback(fetchingMessageNum-1,
						     fetchingStructure);
	}

	if (go())
	{
		doDestroy=false;
		return true;
	}

	return smapHandler::ok(msg);
}

bool mail::smapFETCHATTR::go()
{
	ostringstream msgList;

	doFetchingStructure=NULL;
	fetchingMessageNum=0;
	fetchingStructCount=0;
	seenMimeVersion=false;

	if (!myimap->currentFolder ||
	    myimap->currentFolder->closeInProgress ||
	    !(fetchList >> msgList))
	{
		return false;
	}

	bool hasAttr=false;

	if (attributes & mail::account::ARRIVALDATE)
	{
		msgList << " INTERNALDATE";
		hasAttr=true;
	}

	if (attributes & mail::account::MESSAGESIZE)
	{
		msgList << " SIZE";
		hasAttr=true;
	}

	if (attributes & mail::account::MIMESTRUCTURE)
	{
		msgList << " CONTENTS.PEEK=MIME(:MIME)";
		hasAttr=true;
		fetchingStructure=mail::mimestruct();
		fetchingStructure.type="TEXT";
		fetchingStructure.subtype="PLAIN";
	}
	else /* MIMESTRUCTURE can also be used to build an envelope */

		if (attributes & mail::account::ENVELOPE)
	{
		msgList << " CONTENTS.PEEK=HEADERS(:ENVELOPE)";
		hasAttr=true;
	}

	if (!hasAttr)
		return false;

	doFetchingEnvelope=false;
	myimap->imapcmd("", "FETCH" + msgList.str() + "\n");
	return true;
}

void mail::smapFETCHATTR::fetchedMessageSize(size_t msgNum,
					     unsigned long bytes)
{
	msgcallback.messageSizeCallback(msgNum, bytes);
}

void mail::smapFETCHATTR::fetchedInternalDate(size_t msgNum,
					      time_t internalDate)
{
	msgcallback.messageArrivalDateCallback(msgNum, internalDate);
}

// New FETCH data coming back.

void mail::smapFETCHATTR::beginProcessData(imap &imapAccount,
					   vector<const char *> &words,
					   unsigned long estimatedSize)
{
	doFetchingEnvelope=false;

	if (words.size() >= 2 && strcasecmp(words[0], "FETCH") == 0 &&
	    (attributes & (mail::account::ENVELOPE
			   |mail::account::MIMESTRUCTURE)))
	{
		size_t oldFetchingMessageNum=fetchingMessageNum;

		// Requested a FETCH for an envelope

		string n=words[1];
		istringstream i(n);

		fetchingMessageNum=0;

		i >> fetchingMessageNum;

		if (fetchingMessageNum > 0 &&
		    fetchingMessageNum <=
		    (imapAccount.currentFolder &&
		     !imapAccount.currentFolder->closeInProgress ?
		     imapAccount.currentFolder->exists:0))
		{
			if (attributes & mail::account::MIMESTRUCTURE)
			{
				if (doFetchingStructure &&
				    oldFetchingMessageNum
				    != fetchingMessageNum)
				{
					checkMimeVersion();
					msgcallback
						.messageStructureCallback(oldFetchingMessageNum-1,
									  fetchingStructure);
					fetchingStructure= mail::mimestruct();
					fetchingStructure.type="TEXT";
					fetchingStructure.subtype="PLAIN";
				}

				unsigned long mimeSize=0;
				string mimeid="";
				string mimeparent="";

				vector<const char *>::iterator
					b=words.begin()+2, e=words.end();

				while (b != e)
				{
					const char *c= *b++;

					if (strncasecmp(c, "SIZE=", 5) == 0)
					{
						string n=c+5;

						istringstream i(n);

						i >> mimeSize;
					}

					if (strncasecmp(c, "MIME.ID=", 8) == 0)
						mimeid=c+8;

					if (strncasecmp(c, "MIME.PARENT=", 12)
					    == 0)
					{
						mimeparent=c+12;
					}
				}

#if 0
				cerr << "new MIME struct: id="
				     << mimeid << ", parent=" << mimeparent
				     << endl;
#endif

				seenMimeVersion=false;
				if (fetchingStructCount > 1000)
				{
					doFetchingStructure=NULL;
					// Limit to 1000 MIME sections
				}
				else if (mimeid == "")
				{
					doFetchingStructure=
						&fetchingStructure;
				}
				else
				{
					mail::mimestruct *p=
						findMimeId(&fetchingStructure,
							   mimeparent, 0);

					doFetchingStructure=NULL;
					if (p)
					{
						doFetchingStructure=
							p->addChild();
						doFetchingStructure->type
							="TEXT";
						doFetchingStructure->subtype
							="PLAIN";
					}
				}

				if (doFetchingStructure)
				{
					++fetchingStructCount;

#if 0
					cerr << "added " << mimeid
					     << ", parent's numChildren="
					     << (doFetchingStructure
						 ->getParent()
						 ? doFetchingStructure
						 ->getParent()->getNumChildren()
						 : 0) << endl;
#endif

					doFetchingStructure->mime_id=mimeid;
					doFetchingStructure->content_size=
						mimeSize;
				}
			}

			doFetchingEnvelope=true;
			fetchingEnvelope= mail::envelope();
			fetchingHeader="";
		}
	}
}

mail::mimestruct *mail::smapFETCHATTR::findMimeId(mail::mimestruct *p,
						  string mimeid,
						  size_t recursionLevel)
{
	if (!p || recursionLevel > 20)
		return NULL;
#if 0
	cerr << "findMimeId(" << mimeid << "), current="
	     << p->mime_id << endl;
#endif
	if (p->mime_id == mimeid)
		return p;

	size_t n=p->getNumChildren();
	size_t i;

	for (i=0; i<n; i++)
	{
		mail::mimestruct *q= findMimeId(p->getChild(i), mimeid,
						recursionLevel+1);

		if (q)
			return q;
	}

	return NULL;
}

void mail::smapFETCHATTR::checkMimeVersion()
{
	if (seenMimeVersion)
		return;

	mail::mimestruct *p=doFetchingStructure->getParent();

	if (p && !p->messagerfc822())
		return; // Assumed

	mail::mimestruct dummy;

	dummy.type="TEXT";
	dummy.subtype="PLAIN";

	dummy.content_size= doFetchingStructure->content_size;
	dummy.content_lines= doFetchingStructure->content_lines;
	dummy.mime_id=doFetchingStructure->mime_id;

	*doFetchingStructure=dummy;
}

// As header data comes back, split it at newlines, and parse each individual
// header line.

void mail::smapFETCHATTR::processData(imap &imapAccount,
				      string data)
{
	if (!doFetchingEnvelope)
		return;

	fetchingHeader += data;

	size_t n;

	while ((n=fetchingHeader.find('\n')) != std::string::npos)
	{
		string h=fetchingHeader.substr(0, n);

		fetchingHeader.erase(fetchingHeader.begin(),
				     fetchingHeader.begin()+n+1);

		processFetchedHeader(h);
	}
}

void mail::smapFETCHATTR::endData(imap &imapAccount)
{
	if (doFetchingEnvelope)
	{
		processFetchedHeader(fetchingHeader); // Anything that's left

		if (doFetchingStructure)
		{
			mail::mimestruct *p=doFetchingStructure->getParent();

			if (p && p->messagerfc822())
				p->getEnvelope() = fetchingEnvelope;
		}

		if ( (!doFetchingStructure ||
		      doFetchingStructure->mime_id == "") &&
		     attributes & mail::account::ENVELOPE)
			msgcallback
				.messageEnvelopeCallback(fetchingMessageNum-1,
							 fetchingEnvelope);

	}
}

// Process a FETCHed header line.

void mail::smapFETCHATTR::processFetchedHeader(string hdr)
{
	size_t n=hdr.find(':');

	if (n == std::string::npos)
		return;

	string h=hdr.substr(0, n);

	string::iterator b=hdr.begin()+n+1;

	while (b != hdr.end() && unicode_isspace((unsigned char)*b))
		b++;

	string v=string(b, hdr.end());

	mail::upper(h);
	if (doFetchingEnvelope)
	{
		if (h == "DATE")
		{
			fetchingEnvelope.date=rfc822_parsedt(v.c_str());
		}

		if (h == "SUBJECT")
		{
			fetchingEnvelope.subject=v;
		}

		if (h == "IN-REPLY-TO")
		{
			fetchingEnvelope.inreplyto=v;
		}

		if (h == "MESSAGE-ID")
		{
			fetchingEnvelope.messageid=v;
		}

		if (h == "REFERENCES")
		{
			vector<address> address_list;
			size_t err_index;

			address::fromString(v, address_list, err_index);

			fetchingEnvelope.references.clear();

			vector<address>::iterator
				b=address_list.begin(),
				e=address_list.end();

			while (b != e)
			{
				string s=b->getAddr();

				if (s.size() > 0)
					fetchingEnvelope.references
						.push_back("<" + s + ">");
				++b;
			}
		}

		if (h == "FROM")
		{
			size_t dummy;

			mail::address::fromString(v, fetchingEnvelope.from,
						  dummy);
		}

		if (h == "TO")
		{
			size_t dummy;

			mail::address::fromString(v, fetchingEnvelope.to,
						  dummy);
		}

		if (h == "CC")
		{
			size_t dummy;

			mail::address::fromString(v, fetchingEnvelope.cc,
						  dummy);
		}

		if (h == "BCC")
		{
			size_t dummy;

			mail::address::fromString(v, fetchingEnvelope.bcc,
						  dummy);
		}

		if (h == "SENDER")
		{
			size_t dummy;

			mail::address::fromString(v, fetchingEnvelope.sender,
						  dummy);
		}

		if (h == "REPLY-TO")
		{
			size_t dummy;

			mail::address::fromString(v, fetchingEnvelope.replyto,
						  dummy);
		}

	}

	if (doFetchingStructure)
	{
		if (h == "MIME-VERSION")
			seenMimeVersion=1;

		if (h == "CONTENT-TYPE")
		{
			string t;

			parseMimeHeader(v, t,
					doFetchingStructure->type_parameters);

			size_t n=t.find('/');

			if (n == std::string::npos)
			{
				doFetchingStructure->type=t;
				doFetchingStructure->subtype="";
			}
			else
			{
				doFetchingStructure->type=t.substr(0, n);
				doFetchingStructure->subtype=t.substr(n+1);
			}
		}

		if (h == "CONTENT-ID")
			doFetchingStructure->content_id=v;

		if (h == "CONTENT-DESCRIPTION")
			doFetchingStructure->content_description=v;
		if (h == "CONTENT-TRANSFER-ENCODING")
			doFetchingStructure->content_transfer_encoding=v;
		if (h == "CONTENT-MD5")
			doFetchingStructure->content_md5=v;
		if (h == "CONTENT-LANGUAGE")
			doFetchingStructure->content_language=v;
		if (h == "CONTENT-DISPOSITION")
		{
			parseMimeHeader(v, doFetchingStructure->
					content_disposition,
					doFetchingStructure->
					content_disposition_parameters);
		}
	}
}

void mail::smapFETCHATTR::parseMimeHeader(std::string hdr,
					  std::string &name,
					  mail::mimestruct::parameterList &p)
{
	p=mail::mimestruct::parameterList();

	string::iterator b=hdr.begin(), e=hdr.end();

	name.clear();

	while (b != e && *b != ';')
	{
		if (!unicode_isspace((unsigned char)*b))
			name += *b;
		++b;
	}

	mail::upper(name);

	while (b != e)
	{
		if (unicode_isspace((unsigned char)*b) || *b == ';')
		{
			b++;
			continue;
		}

		string::iterator s=b;

		while (b != e)
		{
			if (*b == ';' ||
			    unicode_isspace((unsigned char)*b) || *b == '=')
				break;
			++b;
		}

		string name(s, b), value;

		while (b != e && unicode_isspace((unsigned char)*b))
			++b;

		if (b != e && *b == '=')
		{
			++b;

			while (b != e && unicode_isspace((unsigned char)*b))
				++b;

			bool inQuote=false;

			while (b != e)
			{
				if (!inQuote && (*b == ';' ||
						 unicode_isspace(*b)))
				{
					b++;
					break;
				}

				if (*b == '"')
				{
					++b;
					inQuote= !inQuote;
					continue;
				}

				if (*b == '\\')
				{
					++b;
					if (b == e)
						break;
				}

				value += *b;
				b++;
			}
		}
		else
			value="1";

		mail::upper(name);

		p.set_simple(name, value);
	}
}
