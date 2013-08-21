/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"

#include "search.H"
#include "envelope.H"
#include "structure.H"
#include "rfcaddr.H"
#include "rfc2047decode.H"
#include "unicode/unicode.h"

#include "rfc822/rfc822.h"
#include "rfc822/rfc822hdr.h"
#include "rfc822/rfc2047.h"

#include <sstream>
#include <iomanip>
#include <errno.h>
#include <string.h>

using namespace std;

mail::searchParams::searchParams() : searchNot(false), criteria(replied),
				     scope(search_all)
{
}

mail::searchParams::~searchParams()
{
}

static const struct {
	mail::searchParams::Criteria code;
	const char *name;
} searchCriteriaTable[] = {
	{mail::searchParams::replied,	"replied"},
	{mail::searchParams::deleted,	"deleted"},
	{mail::searchParams::draft,	"draft"},
	{mail::searchParams::unread,	"unread"},
	{mail::searchParams::from,	"from"},
	{mail::searchParams::to,	"to"},
	{mail::searchParams::cc,	"cc"},
	{mail::searchParams::bcc,	"bcc"},
	{mail::searchParams::subject,	"subject"},
	{mail::searchParams::header,	"header"},
	{mail::searchParams::body,	"body"},
	{mail::searchParams::text,	"text"},
	{mail::searchParams::before,	"before"},
	{mail::searchParams::on,	"on"    },
	{mail::searchParams::since,	"since"},
	{mail::searchParams::sentbefore,	"sentbefore"},
	{mail::searchParams::senton,	"senton"},
	{mail::searchParams::sentsince,	"sentsince"},
	{mail::searchParams::larger,	"larger"},
	{mail::searchParams::smaller,	"smaller"},
	{mail::searchParams::replied,	NULL},
};
	
mail::searchParams::searchParams(string s)
	: searchNot(false), criteria(replied),
	  scope(search_all), rangeLo(0), rangeHi(0)
{
	string::iterator b=s.begin(), c;

	if (b != s.end())
		switch (*b) {
		case 'A':
			scope=search_all;
			break;
		case 'M':
			scope=search_marked;
			break;
		case 'U':
			scope=search_unmarked;
		default:
			{
				istringstream i(s);
				char dummy;

				i >> rangeLo >> dummy >> rangeHi;

				if (!i.fail() && dummy == '-')
					scope=search_range;
			}
			break;
		}

	while (b != s.end() && *b != ' ')
		b++;

	while (b != s.end() && *b == ' ')
		b++;

	if (b != s.end() && *b == '!')
	{
		searchNot=true;
		++b;
	}

	c=b;
	while (c != s.end() && *c != ' ')
		++c;

	string n(b, c);

	size_t i;

	for (i=0; searchCriteriaTable[i].name; i++)
		if (n == searchCriteriaTable[i].name)
		{
			criteria=searchCriteriaTable[i].code;
			break;
		}

	if (c != s.end())
		++c;

	s=string(c, s.end());

	s=decode(s, param1);
	s=decode(s, param2);
	decode(s, charset);
}

string mail::searchParams::searchParams::decode(string s, string &v)
{
	ostringstream o;

	string::iterator b=s.begin(), e=s.end();

	while (b != e)
	{
		if (*b == ' ')
		{
			++b;
			break;
		}

		if (*b != '%')
		{
			o << (char)*b;
			++b;
			continue;
		}

		++b;

		char hx[3];

		if (b != e)
		{
			hx[0]=*b++;
			if (b != e)
			{
				hx[1]=*b++;
				hx[2]=0;

				istringstream i(hx);

				unsigned n;

				i >> hex >> n;

				if (!i.fail())
					o << (char)n;
			}
		}
	}

	v=o.str();
	return string(b, e);
}

mail::searchParams::operator std::string() const
{
	string s;

	size_t i;

	for (i=0; searchCriteriaTable[i].name; i++)
		if (criteria == searchCriteriaTable[i].code)
		{
			s=searchCriteriaTable[i].name;
			break;
		}

	if (searchNot)
		s = "!" +s;

	switch (scope) {
	case search_all:
		s= "A " + s;
		break;
	case search_marked:
		s= "M " + s;
		break;
	case search_unmarked:
		s = "U " + s;
		break;
	case search_range:
		{
			ostringstream o;

			o << rangeLo << '-' << rangeHi << ' ';

			s=o.str() + s;
		}
	}

	return s + " " + encode(param1) + " " + encode(param2) + " "
		+ encode(charset);
}


string mail::searchParams::encode(std::string s)
{
	ostringstream o;

	string::iterator b, e;

	b=s.begin();
	e=s.end();

	while (b != e)
	{
		if (*b <= ' ' || *b >= 127 || *b == '%')
		{
			o << '%' << hex << setw(2) << setfill('0') <<
				(int)(unsigned char)*b;
		}
		else
			o << (char)*b;
		++b;
	}

	return o.str();
}


///////////////////////////////////////////////////////////////////////////

int mail::searchOneMessage::Searcher::converted(const char *str,
						size_t n)
{
	const unicode_char *uc=reinterpret_cast<const unicode_char *>(str);
	n /= sizeof(unicode_char);

	while (n)
	{
		mail::Search::operator<<(*uc);
		++uc;
		--n;
	}
	return 0;
}

mail::searchCallback::searchCallback()
{
}

mail::searchCallback::~searchCallback()
{
}

mail::searchOneMessage::Callback::Callback()
{
}

mail::searchOneMessage::Callback::~Callback()
{
}

void mail::searchOneMessage::Callback::success(string message)
{
	(me->*nextFunc)();
}

void mail::searchOneMessage::Callback::fail(string message)
{
	me->callback.fail(message);
	delete me;
}

void mail::searchOneMessage
::Callback::reportProgress(size_t bytesCompleted,
			   size_t bytesEstimatedTotal,

			   size_t messagesCompleted,
			   size_t messagesEstimatedTotal)
{
	me->callback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				    me->alreadyCompleted,
				    me->alreadyCompleted+1);
}

void mail::searchOneMessage::Callback
::messageEnvelopeCallback(size_t messageNumber,
			  const mail::envelope &envelope)
{
	me->search(envelope);
}

void mail::searchOneMessage::Callback
::messageArrivalDateCallback(size_t messageNumber,
			     time_t datetime)
{
	me->search(datetime);
}

void mail::searchOneMessage::Callback
::messageSizeCallback(size_t messageNumber,
		      unsigned long size)
{
	me->search(size);
}

void mail::searchOneMessage::Callback
::messageStructureCallback(size_t messageNumber,
			   const mail::mimestruct &messageStructure)
{
	me->search(messageStructure);
}

void mail::searchOneMessage::Callback
::messageTextCallback(size_t n, string text)
{
	me->search(text);
}

mail::searchOneMessage::searchOneMessage(mail::searchCallback
					 &callbackArg,
					 mail::searchParams &searchInfoArg,
					 mail::account *ptrArg,
					 size_t messageNumArg,
					 size_t alreadyCompletedArg)
	: callback(callbackArg),
	  searchInfo(searchInfoArg),
	  ptr(ptrArg),
	  messageNum(messageNumArg),
	  alreadyCompleted(alreadyCompletedArg)
{
	my_callback.me=this;
	uid=ptr->getFolderIndexInfo(messageNum).uid;
}

mail::searchOneMessage::~searchOneMessage()
{
}

void mail::searchOneMessage::go()
{
	char *c=libmail_u_convert_tocase(searchInfo.param2.c_str(),
					 searchInfo.charset.c_str(),
					 unicode_uc,
					 NULL);

	if (c == NULL)
	{
		my_callback.fail("Unknown search charset.");
		return;
	}

	try
	{
		searchInfo.param2=c;
		free(c);
	} catch (...) {
		free(c);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	c=libmail_u_convert_tocase(searchInfo.param1.c_str(),
				   "iso-8859-1",
				   unicode_uc,
				   NULL);
	if (!c)
	{
		my_callback.fail(strerror(errno));
		return;
	}

	try {
		searchInfo.param1=c;
		free(c);
	} catch (...) {
		free(c);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	searchCharset=searchInfo.charset;
	searchFlag=false;

	try {
		vector<size_t> messageNumVector;

		messageNumVector.push_back(messageNum);

		size_t n;

		switch (searchInfo.criteria) {
		case searchParams::larger:
		case searchParams::smaller:

			my_callback.nextFunc=
				&mail::searchOneMessage::checkNextHeader;
			ptr->readMessageAttributes(messageNumVector,
						   mail::account::MESSAGESIZE,
						   my_callback);
			return;

		case searchParams::before:
		case searchParams::on:
		case searchParams::since:
		case searchParams::sentbefore:
		case searchParams::senton:
		case searchParams::sentsince:

			while (searchInfo.param2.size() > 0 &&
			       unicode_isspace((unsigned char)searchInfo
					       .param2[0]))
				searchInfo.param2=searchInfo.param2.substr(1);

			n=searchInfo.param2.find(' ');

			if (n != std::string::npos)
				searchInfo.param2=
					searchInfo.param2.substr(0,n);

			for (n=0; n<searchInfo.param2.size(); n++)
				if (searchInfo.param2[n] == '-')
					searchInfo.param2[n]=' ';

			searchInfo.param2 += " 00:00:00 -0000";

			cmpDate=rfc822_parsedt(searchInfo.param2.c_str());

			if (cmpDate == 0)
			{
				my_callback.fail("Invalid date specified");
				return;
			}

			my_callback.nextFunc=
				&mail::searchOneMessage::checkFwdEnvelope;
			ptr->readMessageAttributes(messageNumVector,

						   (searchInfo.criteria ==
						    searchInfo.before ||
						    searchInfo.criteria ==
						    searchInfo.on ||
						    searchInfo.criteria ==
						    searchInfo.since ?

						    mail::account::ARRIVALDATE:
						    mail::account::ENVELOPE),
						   my_callback);
			return;

		case searchParams::from:
		case searchParams::to:
		case searchParams::cc:
		case searchParams::bcc:
		case searchParams::subject:
			my_callback.nextFunc=
				&mail::searchOneMessage::checkFwdEnvelope;
			ptr->readMessageAttributes(messageNumVector,
						   mail::account::ENVELOPE,
						   my_callback);
			return;

		case searchParams::header:
		case searchParams::body:
		case searchParams::text:

			while (!mimeSearch.empty())
				mimeSearch.pop();

			headerSearchBuffer="";
			my_callback.nextFunc=
				&mail::searchOneMessage::checkNextHeader;
			ptr->readMessageAttributes(messageNumVector,
						   mail::account::MIMESTRUCTURE,
						   my_callback);
			return;

		case searchParams::replied:
		case searchParams::deleted:
		case searchParams::draft:
		case searchParams::unread:
			break;
		}
	} catch (...) {
		my_callback.fail("An exception occured in mail::searchOneMessage.");
		return;
	}

	checkSearch();
}

bool mail::searchOneMessage::sanityCheck()
{
	if (!ptr.isDestroyed() && messageNum < ptr->getFolderIndexSize() &&
	    uid == ptr->getFolderIndexInfo(messageNum).uid)
		return true;
	checkSearch();
	return false;
}

void mail::searchOneMessage::checkFwdEnvelope()
{
	if (!sanityCheck())
		return;

	if (searchFlag)
	{
		checkSearch(); // No need
		return;
	}

	try {
		vector<size_t> messageNumVector;

		messageNumVector.push_back(messageNum);

		my_callback.nextFunc=
			&mail::searchOneMessage::checkSearch;
		ptr->readMessageAttributes(messageNumVector,
					   mail::account::MIMESTRUCTURE,
					   my_callback);
	} catch (...) {
		my_callback.fail("An exception occured in mail::searchOneMessage.");
		return;
	}
}

void mail::searchOneMessage::checkNextHeader()
{
	if (searchInfo.criteria == searchParams::header &&
	    headerSearchBuffer.size() > 0)
		search("\n");

	searchEngine.end();

	if (searchEngine)
		searchFlag=true;

	if (!sanityCheck())
		return;

	mail::mimestruct *hdrs;
	std::string bodyCharset("iso-8859-1");

	for (;;) {

		if (searchFlag || mimeSearch.empty())
		{
			checkSearch();
			return;
		}

		hdrs=mimeSearch.front();

		mimeSearch.pop();

		my_callback.nextFunc=&mail::searchOneMessage::checkNextHeader;
		headerSearchBuffer="";

		switch (searchInfo.criteria) {
		case searchParams::header:

			if (!searchEngine.setString(searchInfo.param2,
						    searchCharset))
			{
				my_callback.fail(strerror(errno));
				return;
			}
			bodyCharset="utf-8";
			break;

		case searchParams::body:
		case searchParams::text:

			if (!searchEngine.setString(searchInfo.param2,
						    searchCharset))
			{
				my_callback.fail(strerror(errno));
				return;
			}

			if (hdrs->type == "TEXT" &&
			    hdrs->type_parameters.exists("CHARSET"))
			{
				bodyCharset=hdrs->type_parameters
					.get("CHARSET");
			}
			break;
		default:
			checkSearch(); // Shouldn't happen.
			return;
		}

		if (searchEngine.getSearchLen() == 0) // Empty srch string
		{
			checkSearch();
			return;
		}
		break;
	}

	searchEngine.begin(bodyCharset.c_str(),
			   libmail_u_ucs4_native);

	switch (searchInfo.criteria) {
	case searchParams::header:
		ptr->readMessageContent(messageNum, true, *hdrs,
					mail::readHeadersFolded,
					my_callback);
		break;
	case searchParams::body:
	case searchParams::text:
		ptr->readMessageContentDecoded(messageNum, true, *hdrs,
					       my_callback);
		break;
	default:
		break;
	}
}

void mail::searchOneMessage::checkSearch()
{
	if (searchInfo.criteria == searchInfo.text && !searchFlag)
	{
		// Not found, look in the headers.

		searchInfo.criteria=searchInfo.header;
		searchInfo.param1="";
		headerSearchBuffer="";
		searchFwdEnvelope(structureBuffer);
		checkNextHeader();
		return;
	}

	if (searchInfo.searchNot)
		searchFlag= !searchFlag;

	success(callback, messageNum, searchFlag);
}

void mail::searchOneMessage::success(mail::searchCallback &callback,
				     size_t n, bool result)
{
	delete this;

	try {
		vector<size_t> msgSet;

		if (result)
			msgSet.push_back(n);

		callback.success(msgSet);
	} catch (...) {
		my_callback.fail("An exception has occured processing search results.");
		return;
	}
}

void mail::searchOneMessage::search(const mail::envelope &envelope)
{
	switch (searchInfo.criteria) {
	case searchParams::from:
	case searchParams::to:
	case searchParams::cc:
	case searchParams::bcc:
	case searchParams::subject:
	case searchParams::sentbefore:
	case searchParams::senton:
	case searchParams::sentsince:

		searchEnvelope(envelope);
		break;
	default:
		break;
	}
}

void mail::searchOneMessage::searchEnvelope(const mail::envelope &envelope)
{
	string searchStr;
	const char *header="Subject";

	switch (searchInfo.criteria) {
	case searchParams::from:
		searchStr= mail::address::toString("", envelope.from);
		header="From";
		break;
	case searchParams::to:
		searchStr= mail::address::toString("", envelope.to);
		header="To";
		break;
	case searchParams::cc:
		searchStr= mail::address::toString("", envelope.cc);
		header="Cc";
		break;
	case searchParams::bcc:
		searchStr= mail::address::toString("", envelope.bcc);
		header="Bcc";
		break;
	case searchParams::subject:
		searchStr=envelope.subject;
		break;

	case searchParams::sentbefore:
	case searchParams::senton:
	case searchParams::sentsince:

		doDateCmp(envelope.date);
		return;
	default:
		return;
	}

	char *q=rfc822_display_hdrvalue_tobuf(header,
					      searchStr.c_str(),
					      "utf-8",
					      NULL, NULL);

	if (!q)
		return;

	if (!searchEngine.setString(searchInfo.param2, searchCharset))
	{
		free(q);
		return;
	}

	if (searchEngine.getSearchLen() == 0)
	{
		free(q);
		return;
	}

	std::vector<unicode_char> uc;

	if (!mail::iconvert::convert(q, "utf-8", uc))
	{
		free(q);
		return;
	}
	free(q);

	for (std::vector<unicode_char>::iterator
		     b(uc.begin()), e(uc.end()); b != e; ++b)
		searchEngine << *b;

	if (searchEngine)
		searchFlag=true;
}

static const char spaces[]=" \t\r\n";

void mail::searchOneMessage::search(time_t internaldate)
{
	switch (searchInfo.criteria) {
	case searchParams::before:
	case searchParams::on:
	case searchParams::since:
		doDateCmp(internaldate);
		break;
	default:
		break;
	}
}

void mail::searchOneMessage::search(unsigned long messageSize)
{
	unsigned long b=0;

	istringstream i(searchInfo.param2.c_str());

	i >> b;

	if (!i.fail())
		switch (searchInfo.criteria) {
		case searchParams::larger:
			if (messageSize > b)
				searchFlag=true;
			break;
		case searchParams::smaller:
			if (messageSize < b)
				searchFlag=true;
			break;
		default:
			break;
		}
}

void mail::searchOneMessage::search(const mail::mimestruct &structureInfo)
{
	switch (searchInfo.criteria) {
	case searchParams::from:
	case searchParams::to:
	case searchParams::cc:
	case searchParams::bcc:
	case searchParams::subject:
	case searchParams::header:
	case searchParams::body:
	case searchParams::text:

		structureBuffer=structureInfo;

		searchFwdEnvelope(structureBuffer);
		break;
	default:
		break;
	}
}

// Search envelopes of attached messages.

void mail::searchOneMessage::searchFwdEnvelope(mail::mimestruct &structureInfo)
{
	if (searchFlag)
		return;

	if (searchInfo.criteria == searchInfo.header)
		mimeSearch.push(&structureInfo); // Queue up header searches
	else if (searchInfo.criteria == searchInfo.body ||
		 searchInfo.criteria == searchInfo.text)
	{
		if (structureInfo.getNumChildren() == 0)
			// Only leafs receive the content search.
			mimeSearch.push(&structureInfo);
	}
	else if (structureInfo.messagerfc822())
		searchEnvelope( structureInfo.getEnvelope());

	size_t n=structureInfo.getNumChildren();
	size_t i;

	for (i=0; i<n; i++)
		searchFwdEnvelope(*structureInfo.getChild(i));
}

void mail::searchOneMessage::search(string text)
{
	if (searchFlag)
		return;

	if (searchInfo.criteria == searchInfo.header)
	{
		text=headerSearchBuffer + text;

		size_t n;

		while (!searchFlag && (n=text.find('\n')) != std::string::npos)
		{
			searchEngine.reset();

			string s=text.substr(0, n);

			text=text.substr(n+1);

			std::string headername;

			size_t colon=s.find(':');

			if (colon != std::string::npos)
			{
				headername=s.substr(0, colon);
				s=s.substr(colon+1);
			}

			if (searchInfo.param1.size() > 0 &&
			    rfc822hdr_namecmp(headername.c_str(),
					      searchInfo.param1.c_str()))
				continue;

			char *value=
				rfc822_display_hdrvalue_tobuf(headername
							      .c_str(),
							      s.c_str(),
							      "utf-8",
							      NULL,
							      NULL);

			if (!value)
				continue;


			unicode_char *uc;
			size_t ucsize;

			if (libmail_u_convert_tou_tobuf(value,
							strlen(value),
							"utf-8",
							&uc,
							&ucsize,
							NULL))
			{
				free(value);
				continue;
			}
			free(value);

			size_t n;

			for (n=0; n<ucsize; ++n)
			{
				searchEngine << uc[n];
			}
			free(uc);

			if (searchEngine)
				searchFlag=true;
		}
		if (!searchFlag)
			headerSearchBuffer=text;
	}
	else if (searchInfo.criteria == searchInfo.body ||
		 searchInfo.criteria == searchInfo.text)
	{
		searchEngine(text.c_str(), text.size());

		if (searchEngine)
			searchFlag=true;
	}
}

void mail::searchOneMessage::doDateCmp(time_t t)
{
	struct tm *tmptr=gmtime(&cmpDate);

	if (!tmptr)
		return;

	int y=tmptr->tm_year;
	int m=tmptr->tm_mon;
	int d=tmptr->tm_mday;

	tmptr=localtime(&t);

	int diff=0;

	if (tmptr->tm_year > y)
		diff=1;
	else if (tmptr->tm_year < y)
		diff= -1;
	else if (tmptr->tm_mon > m)
		diff=1;
	else if (tmptr->tm_mon < m)
		diff= -1;
	else if (tmptr->tm_mday > d)
		diff=1;
	else if (tmptr->tm_mday < d)
		diff= -1;

	switch (searchInfo.criteria) {
	case searchParams::on:
	case searchParams::senton:
		if (diff == 0)
			searchFlag=true;
		break;

	case searchParams::before:
	case searchParams::sentbefore:

		if (diff < 0)
			searchFlag=true;
		break;

	case searchParams::since:
	case searchParams::sentsince:

		if (diff >= 0)
			searchFlag=true;
		break;
	default:
		break;
	}
}

///////////////////////////////////////////////////////////////////////

mail::searchMessages::searchMessages(mail::searchCallback
				     &callbackArg,
				     const mail::searchParams
				     &searchInfoArg,
				     mail::account *ptrArg)
	: callback(callbackArg), server(ptrArg), searchInfo(searchInfoArg)
{
	search_callback.me=this;

	nextMsgNum=0;
}

void mail::searchMessages::nextSearch()
{
	try {
		RunLater();
	} catch (...) {
		callback.fail("An exception occured in mail::searchMessages.");
	}
}

void mail::searchMessages::RunningLater()
{
	if (server.isDestroyed())
	{
		callback.fail("Folder closed while a search was in progress.");
		delete this;
		return;
	}

	size_t cnt=server->getFolderIndexSize();

	while (nextMsgNum < cnt)
	{
                mail::messageInfo i=server->getFolderIndexInfo(nextMsgNum);

		switch (searchInfo.scope) {
		case searchParams::search_all:
			break;

		case searchParams::search_unmarked:
                        if (i.marked)
			{
				++nextMsgNum;
                                continue;
			}
			break;

		case searchParams::search_marked:
                        if (!i.marked)
			{
				++nextMsgNum;
                                continue;
			}
			break;

		case searchParams::search_range:
			if (nextMsgNum < searchInfo.rangeLo
			    || nextMsgNum >= searchInfo.rangeHi)
			{
				++nextMsgNum;
                                continue;
			}
			break;
                }

		uid=i.uid;

		switch (searchInfo.criteria) {
		case searchParams::replied:
			if (searchInfo.searchNot ? !i.replied:i.replied)
				successArray.push_back(uid);
			++nextMsgNum;
			continue;
		case searchParams::deleted:
			if (searchInfo.searchNot ? !i.deleted:i.deleted)
				successArray.push_back(uid);
			++nextMsgNum;
			continue;
		case searchParams::draft:
			if (searchInfo.searchNot ? !i.draft:i.draft)
				successArray.push_back(uid);
			++nextMsgNum;
			continue;
		case searchParams::unread:
			if (searchInfo.searchNot ? !i.unread:i.unread)
				successArray.push_back(uid);
			++nextMsgNum;
			continue;
		default:
			break;
		}
		break;
	}

	if (nextMsgNum >= cnt)
	{
		vector<size_t> msgNumArray;

		size_t n=server->getFolderIndexSize();

		size_t i=0;
		size_t j;

		for (j=0; j<n && i < successArray.size(); j++)
			if (server->getFolderIndexInfo(j).uid
			    == successArray[i])
			{
				msgNumArray.push_back(j);
				i++;
			}

		callback.success(msgNumArray);
		delete this;
		return;
	}

	searchInfoCpy=searchInfo;

	mail::searchOneMessage *m=NULL;

	try {
		m=new mail::searchOneMessage(search_callback,
					     searchInfoCpy,
					     server,
					     nextMsgNum,
					     nextMsgNum);
		nextMsgNum++;

		if (!m)
		{
			callback.fail("Out of memory.");
			delete m;
			return;
		}

		m->go();
	} catch (...) {
		delete m;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

mail::searchMessages::~searchMessages()
{
}

mail::searchMessages::Callback::Callback()
{
}

mail::searchMessages::Callback::~Callback()
{
}

void mail::searchMessages::Callback::fail(string message)
{
	me->callback.fail(message);
	delete me;
}

void mail::searchMessages::Callback::success(const vector<size_t> &found)
{
	try {
		if (found.size() > 0)
			me->successArray.push_back(me->uid);
		me->nextSearch();
	} catch (...) {
		fail("Exception occured during a search.");
	}
}

void mail::searchMessages
::Callback::reportProgress(size_t bytesCompleted,
			   size_t bytesEstimatedTotal,

			   size_t messagesCompleted,
			   size_t messagesEstimatedTotal)
{
	me->callback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				    messagesCompleted,
				    me->server.isDestroyed() ?
				    messagesCompleted+1:
				    me->server->getFolderIndexSize());
}

void mail::searchMessages::search(mail::searchCallback
				  &callbackArg,
				  const mail::searchParams &searchInfoArg,
				  mail::account *ptrArg)
{
	mail::searchMessages *m=NULL;

	try {
		m=new mail::searchMessages(callbackArg, searchInfoArg,
					   ptrArg);

		if (!m)
		{
			callbackArg.fail("Out of memory.");
			return;
		}

		m->nextSearch();
	} catch (...) {
		if (m)
			delete m;
		callbackArg.fail("Exception occured trying to start a search.");
	}
}


