/*
** Copyright 2003-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "smapsearch.H"
#include "misc.H"
#include <sstream>

using namespace std;

const char *mail::smapSEARCH::getName()
{
	return "SEARCH";
}

mail::smapSEARCH::smapSEARCH(const searchParams &parametersArg,
			     searchCallback &callbackArg,
			     mail::imap &imapAccount)

	: parameters(parametersArg),
	  callback(&callbackArg)
{
}

mail::smapSEARCH::~smapSEARCH()
{
	if (callback)
	{
		searchCallback *c=callback;

		callback=NULL;
		c->fail("Operation aborted");
	}
}

void mail::smapSEARCH::installed(imap &imapAccount)
{
	ostringstream o;

	switch (parameters.scope) {
	case searchParams::search_all:
		o << "SEARCH ALL";
		break;

	case searchParams::search_unmarked:
		o << "SEARCH UNMARKED";
		break;

	case searchParams::search_marked:
		o << "SEARCH MARKED";
		break;
	case searchParams::search_range:
		o << "SEARCH " << parameters.rangeLo+1
		  << "-" << parameters.rangeHi;
		break;
	}

	bool searchNot=parameters.searchNot;
	searchParams::Criteria criteria=parameters.criteria;

	string param1=parameters.param1;
	string param2=mail::toutf8(parameters.param2);

	string::iterator p;

	for (p=param1.begin(); p != param1.end(); p++)
		if (*p == '\n')
			*p=' ';

	for (p=param2.begin(); p != param2.end(); p++)
		if (*p == '\n')
			*p=' ';

	if (criteria == mail::searchParams::unread)
		searchNot= !searchNot;

	if (searchNot)
		o << " NOT";

	bool doParam1=false;
	bool doParam2=false;

	switch (criteria) {
	case mail::searchParams::replied:
		o << " REPLIED\n";
		break;
	case mail::searchParams::deleted:
		o << " DELETED\n";
		break;
	case mail::searchParams::draft:
		o << " DRAFT\n";
		break;
	case mail::searchParams::unread:
		o << " SEEN\n";
		break;
	case mail::searchParams::from:
		o << " FROM ";
		doParam2=true;
		break;
	case mail::searchParams::to:
		o << " TO ";
		doParam2=true;
		break;
	case mail::searchParams::cc:
		o << " CC ";
		doParam2=true;
		break;
	case mail::searchParams::bcc:
		o << " BCC ";
		doParam2=true;
		break;
	case mail::searchParams::subject:
		o << " SUBJECT ";
		doParam2=true;
		break;
	case mail::searchParams::header:
		o << " HEADER ";
		doParam1=true;
		doParam2=true;
		break;
	case mail::searchParams::body:
		o << " BODY ";
		doParam2=true;
		break;
	case mail::searchParams::text:
		o << " TEXT ";
		doParam2=true;
		break;
	case mail::searchParams::before:
		o << " BEFORE ";
		doParam2=true;
		break;
	case mail::searchParams::on:    
		o << " ON ";
		doParam2=true;
		break;
	case mail::searchParams::since:
		o << " SINCE ";
		doParam2=true;
		break;
	case mail::searchParams::sentbefore:
		o << " SENTBEFORE ";
		doParam2=true;
		break;
	case mail::searchParams::senton:
		o << " SENTON ";
		doParam2=true;
		break;
	case mail::searchParams::sentsince:
		o << " SENTSINCE ";
		doParam2=true;
		break;
	case mail::searchParams::larger:
		o << " LARGER ";
		doParam2=true;
		break;
	case mail::searchParams::smaller:
		o << " SMALLER ";
		doParam2=true;
		break;
	}

	if (doParam1)
		o << mail::imap::quoteSMAP(param1) << " ";
	if (doParam2)
		o << mail::imap::quoteSMAP(param2) << "\n";

	imapAccount.imapcmd("", o.str());
}

bool mail::smapSEARCH::ok(std::string msg)
{
	searchCallback *c=callback;

	callback=NULL;

	if (c)
		c->success(found);

	return true;
}


bool mail::smapSEARCH::fail(std::string msg)
{
	searchCallback *c=callback;

	callback=NULL;

	if (c)
		c->fail(msg);

	return true;
}

bool mail::smapSEARCH::processLine(imap &imapAccount,
				 vector<const char *> &words)
{
	if (words.size() >= 2 &&
	    strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "SEARCH") == 0)
	{
		vector<const char *>::iterator b=words.begin()+2,
			e=words.end();

		while (b != e)
		{
			size_t first=0;
			size_t last=0;
			char dummy=0;

			string w(*b++);
			istringstream i(w);

			i >> first >> dummy >> last;

			if (dummy != '-')
				last=first;

			if (first == 0 || last == 0 || last < first)
				continue;
			--first;
			--last;

			do
			{
				if (first >=
				    (imapAccount.currentFolder ?
				     imapAccount.currentFolder->exists:0))
					break;
				found.push_back(first);
			} while (first++ < last);
		}
		return true;
	}
	return smapHandler::processLine(imapAccount, words);
}
