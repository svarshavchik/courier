/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "searchprompt.H"
#include "gettext.H"
#include "myserver.H"
#include "myserverpromptinfo.H"
#include "unicode/unicode.h"
#include <time.h>

extern Gettext::Key key_ALL;
extern Gettext::Key key_SEARCHANYWHERE;
extern Gettext::Key key_SEARCHCONTENTS;
extern Gettext::Key key_DATE;
extern Gettext::Key key_DATEACCEPT;
extern Gettext::Key key_DAYDEC1;
extern Gettext::Key key_DAYINC1;
extern Gettext::Key key_DELETED;
extern Gettext::Key key_LARGER;
extern Gettext::Key key_MONDEC1;
extern Gettext::Key key_MONINC1;
extern Gettext::Key key_NOT1;
extern Gettext::Key key_RECVBEFORE1;
extern Gettext::Key key_RECVON1;
extern Gettext::Key key_RECVSINCE1;
extern Gettext::Key key_REPLIED;
extern Gettext::Key key_SEARCHBCC;
extern Gettext::Key key_SEARCHCC;
extern Gettext::Key key_SEARCHFROM;
extern Gettext::Key key_SEARCHHEADER;
extern Gettext::Key key_SEARCHSUBJECT;
extern Gettext::Key key_SEARCHTO;
extern Gettext::Key key_SENTBEFORE1;
extern Gettext::Key key_SENTON1;
extern Gettext::Key key_SENTSINCE1;
extern Gettext::Key key_SIZE;
extern Gettext::Key key_SMALLER;
extern Gettext::Key key_STATUS;
extern Gettext::Key key_TEXT;
extern Gettext::Key key_UNREAD;
extern Gettext::Key key_YEARDEC1;
extern Gettext::Key key_YEARINC1;

// Prompt for the search type

bool searchPrompt::prompt(mail::searchParams &searchInfo,
			  bool &selectAll)
{
	selectAll=false;

	std::string prompt1;
	std::string prompt2;

	searchInfo.charset= unicode_default_chset();

	bool rc=searchPromptType(searchInfo, prompt1, prompt2, selectAll);

	if (myServer::nextScreen)
	{
		return false; // Something aborted.
	}

	if (selectAll)
		return true; // SELECT ALL action.

	if (rc)
		return false;

	if (prompt1.size() > 0)
	{
		myServer::promptInfo prompt=
			myServer::prompt(myServer::promptInfo(prompt1));

		if (prompt.abortflag || myServer::nextScreen ||
		    (searchInfo.param1=prompt).size() == 0)
			return false;

		if (searchInfo.criteria == searchInfo.header)
			prompt2=searchInfo.param1 + ": ";
	}

	if (prompt2.size() > 0)
	{
		myServer::promptInfo prompt=
			myServer::prompt(myServer::promptInfo(prompt2));

		if (prompt.abortflag || myServer::nextScreen ||
		    (searchInfo.param2=prompt).size() == 0)
			return false;
	}

	return true;
}

bool searchPrompt::searchPromptType(mail::searchParams &searchInfo,
				    std::string &prompt1,
				    std::string &prompt2,
				    bool &selectAll)
{
	for (;;)
	{
		myServer::promptInfo prompt=
			myServer::prompt(myServer
					 ::promptInfo(searchInfo.searchNot
						      ? _("Search by (NOT): ")
						      : _("Search by: ")
						      )

					 .option(key_NOT1,
						 Gettext::keyname(_("NOT_K:!")),
						 _("NOT"))

					 .option(key_ALL,
						 Gettext::keyname(_("ALL_K:A")),
						 _("select All"))

					 .option(key_DATE,
						 Gettext::keyname(_("DATE_K:D")),
						 _("Date"))

					 .option(key_SIZE,
						 Gettext::keyname(_("SIZE_K:Z")),
						 _("siZe"))

					 .option(key_TEXT,
						 Gettext::keyname(_("TEXT_K:T")),
						 _("Text"))

					 .option(key_STATUS,
						 Gettext::keyname(_("STATUS_K:S")),
						 _("Status"))
					 );


		if (prompt.abortflag || myServer::nextScreen)
			return true;

		unicode_char promptKey=prompt.firstChar();

		if (promptKey == 0)
			return true;

		if (key_NOT1 == promptKey)
		{
			searchInfo.searchNot = !searchInfo.searchNot;
			continue;
		}

		if (key_ALL == promptKey)
		{
			selectAll=true;
			return true;
		}

		if (key_DATE == promptKey)
			return searchPromptDate(searchInfo);
		else if (key_SIZE == promptKey)
			return searchPromptSize(searchInfo);
		else if (key_STATUS == promptKey)
			return searchPromptStatus(searchInfo);
		else if (key_TEXT == promptKey)
			return searchPromptText(searchInfo, prompt1, prompt2);
		else break;

	}
	return true;
}

bool searchPrompt::searchPromptStatus(mail::searchParams &searchInfo)
{
	myServer::promptInfo prompt=
		myServer::prompt(myServer::promptInfo(_("Status: "))
				 .option(key_REPLIED,
					 Gettext::keyname(_("REPLIED_K:R")),
					 _("Replied"))

				 .option(key_DELETED,
					 Gettext::keyname(_("DELETED_K:D")),
					 _("Deleted"))

				 .option(key_UNREAD,
					 Gettext::keyname(_("UNREAD_K:U")),
					 _("Unread"))
				 );

	if (prompt.abortflag || myServer::nextScreen)
		return true;

	unicode_char promptKey=prompt.firstChar();

	if (promptKey == 0)
		return true;

	if (key_REPLIED == promptKey)
	{
		searchInfo.criteria=searchInfo.replied;
		return false;
	}

	if (key_DELETED == promptKey)
	{
		searchInfo.criteria=searchInfo.deleted;
		return false;
	}

	if (key_UNREAD == promptKey)
	{
		searchInfo.criteria=searchInfo.unread;
		return false;
	}
	return true;
}

bool searchPrompt::searchPromptText(mail::searchParams &searchInfo,
				    std::string &prompt1,
				    std::string &prompt2)
{
	myServer::promptInfo prompt=
		myServer::prompt(myServer
				 ::promptInfo(_("Text search in: "))

				 .option(key_SEARCHFROM,
					 Gettext::keyname(_("SEARCHFROM_K:F")),
					 _("From:"))

				 .option(key_SEARCHTO,
					 Gettext::keyname(_("SEARCH_TO_K:T")),
					 _("To:"))

				 .option(key_SEARCHCC,
					 Gettext::keyname(_("SEARCH_CC_K:C")),
					 _("Cc:"))

				 .option(key_SEARCHBCC,
					 Gettext::keyname(_("SEARCH_BCC_K:B")),
					 _("Bcc:"))

				 .option(key_SEARCHSUBJECT,
					 Gettext::keyname(_("SEARCH_SUBJECT_K:S")),
					 _("Subject:"))
				 .option(key_SEARCHHEADER,
					 Gettext::keyname(_("HEADERKEY_K:H")),
					 _("Header"))
				 .option(key_SEARCHCONTENTS,
					 Gettext::keyname(_("CONTENTSKEY_K:O")),
					 _("cOntents"))

				 .option(key_SEARCHANYWHERE,
					 Gettext::keyname(_("ANYWHEREKEY_K:A")),
					 _("All text"))


				 );

	if (prompt.abortflag || myServer::nextScreen)
		return true;

	unicode_char promptKey=prompt.firstChar();

	if (promptKey == 0)
		return true;

	if (key_SEARCHFROM == promptKey)
	{
		searchInfo.criteria=searchInfo.from;
		prompt2=_("From: ");
	}
	else if (key_SEARCHTO == promptKey)
	{
		searchInfo.criteria=searchInfo.to;
		prompt2=_("To: ");
	}
	else if (key_SEARCHCC == promptKey)
	{
		searchInfo.criteria=searchInfo.cc;
		prompt2=_("Cc: ");
	}
	else if (key_SEARCHBCC == promptKey)
	{
		searchInfo.criteria=searchInfo.bcc;
		prompt2=_("Bcc: ");
	}
	else if (key_SEARCHSUBJECT == promptKey)
	{
		searchInfo.criteria=searchInfo.subject;
		prompt2=_("Subject: ");
	}
	else if (key_SEARCHHEADER == promptKey)
	{
		searchInfo.criteria=searchInfo.header;
		prompt1=_("Header name: ");
	}
	else if (key_SEARCHCONTENTS == promptKey)
	{
		searchInfo.criteria=searchInfo.body;
		prompt2=_("Contents: ");
	}
	else if (key_SEARCHANYWHERE == promptKey)
	{
		searchInfo.criteria=searchInfo.text;
		prompt2=_("Contents: ");
	}
	else return true;

	return false;
}

// Additional prompt - date search type

bool searchPrompt::searchPromptDate(mail::searchParams &searchInfo)
{
	myServer::promptInfo prompt=myServer
		::prompt(myServer
			 ::promptInfo(_("Search date: "))
			 .option(key_SENTBEFORE1,
				 Gettext::keyname(_("SENTBEFORE_K:1")),
				 _("Sent before"))

			 .option(key_SENTON1,
				 Gettext::keyname(_("SENTON_K:2")),
				 _("Sent on"))

			 .option(key_SENTSINCE1,
				 Gettext::keyname(_("SENTSINCE_K:3")),
				 _("Sent since"))

			 .option(key_RECVBEFORE1,
				 Gettext::keyname(_("RECVBEFORE_K:4")),
				 _("Recv before"))

			 .option(key_RECVON1,
				 Gettext::keyname(_("RECVON_K:5")),
				 _("Recv on"))

			 .option(key_RECVSINCE1,
				 Gettext::keyname(_("RECVSINCE_K:6")),
				 _("Recv since"))
			 );

	if (prompt.abortflag || myServer::nextScreen)
		return true;

	unicode_char promptKey=prompt.firstChar();

	if (promptKey == 0)
		return true;

	std::string datePrompt="";

	if (key_SENTBEFORE1 == promptKey)
	{
		datePrompt=_("Sent before: %1% ");
		searchInfo.criteria=searchInfo.sentbefore;
	}
	else if (key_SENTON1 == promptKey)
	{
		datePrompt=_("Sent on: %1% ");
		searchInfo.criteria=searchInfo.senton;
	}
	else if (key_SENTSINCE1 == promptKey)
	{
		datePrompt=_("Sent since: %1% ");
		searchInfo.criteria=searchInfo.sentsince;
	}
	else if (key_RECVBEFORE1 == promptKey)
	{
		datePrompt=_("Received before: %1% ");
		searchInfo.criteria=searchInfo.before;
	}
	else if (key_RECVON1 == promptKey)
	{
		datePrompt=_("Received on: %1% ");
		searchInfo.criteria=searchInfo.on;
	}
	else if (key_RECVSINCE1 == promptKey)
	{
		datePrompt=_("Received since: %1% ");
		searchInfo.criteria=searchInfo.since;
	}
	else
		return true;

	return searchPromptDate(searchInfo, datePrompt);
}

// Fun way to enter dates.

bool searchPrompt::searchPromptDate(mail::searchParams &searchInfo,
				    std::string promptStr)
{
	time_t t;

	time(&t);

	struct tm *tmptr=localtime(&t);

	if (!tmptr)
		return true;

	int y=tmptr->tm_year + 1900;
	int m=tmptr->tm_mon + 1;
	int d=tmptr->tm_mday;

	std::string months[12];
	const char *monthsE[12];

	months[0]=_("Jan");
	months[1]=_("Feb");
	months[2]=_("Mar");
	months[3]=_("Apr");
	months[4]=_("May");
	months[5]=_("Jun");
	months[6]=_("Jul");
	months[7]=_("Aug");
	months[8]=_("Sep");
	months[9]=_("Oct");
	months[10]=_("Nov");
	months[11]=_("Dec");

	monthsE[0]="Jan";
	monthsE[1]="Feb";
	monthsE[2]="Mar";
	monthsE[3]="Apr";
	monthsE[4]="May";
	monthsE[5]="Jun";
	monthsE[6]="Jul";
	monthsE[7]="Aug";
	monthsE[8]="Sep";
	monthsE[9]="Oct";
	monthsE[10]="Nov";
	monthsE[11]="Dec";

	for (;;)
	{
		char bufDate[100];

		if (snprintf(bufDate, sizeof(bufDate),
			     "%02d-%s-%04d", d, months[m-1].c_str(), y) < 0)
			bufDate[0]=0;

		myServer::promptInfo prompt=myServer
			::prompt(myServer
				 ::promptInfo(Gettext(promptStr) << bufDate)
				 .option(key_DATEACCEPT,
					 Gettext::keyname(_("DATEACCEPT_K:SP")),
					 _("Accept"))
				 .option(key_DAYDEC1,
					 Gettext::keyname(_("DAYDEC_K:-")),
					 _("Prev day"))

				 .option(key_DAYINC1,
					 Gettext::keyname(_("DAYINC_K:+")),
					 _("Next day"))

				 .option(key_MONDEC1,
					 Gettext::keyname(_("MONDEC_K:[")),
					 _("Prev month"))

				 .option(key_MONINC1,
					 Gettext::keyname(_("MONINC_K:]")),
					 _("Next month"))

				 .option(key_YEARDEC1,
					 Gettext::keyname(_("YEARDEC_K:<")),
					 _("Prev year"))

				 .option(key_YEARINC1,
					 Gettext::keyname(_("YEARINC_K:>")),
					 _("Next year"))
				 );

		if (prompt.abortflag || myServer::nextScreen)
			return true;

		unicode_char promptKey=prompt.firstChar();

		if (key_DAYDEC1 == promptKey)
		{
			dayAdd(d, m, y, -1);
		}
		else if (key_DAYINC1 == promptKey)
		{
			dayAdd(d, m, y, 1);
		}
		else if (key_MONDEC1 == promptKey)
		{
			monAdd(d, m, y, -1);
		}
		else if (key_MONINC1 == promptKey)
		{
			monAdd(d, m, y, 1);
		}
		else if (key_YEARDEC1 == promptKey)
		{
			yearAdd(d, m, y, -1);
		}
		else if (key_YEARINC1 == promptKey)
		{
			yearAdd(d, m, y, 1);
		}
		else
		{
			if (snprintf(bufDate, sizeof(bufDate),
				     "%02d-%s-%04d", d, monthsE[m-1], y) < 0)
				bufDate[0]=0;
			searchInfo.param2=bufDate;
			break;
		}
	}
	return false;
}

void searchPrompt::dayAdd(int &d, int &m, int &y, int howMuch)
{
	struct tm tmDummy;

	memset(&tmDummy, 0, sizeof(tmDummy));

	tmDummy.tm_year=y - 1900;
	tmDummy.tm_mon=m - 1;
	tmDummy.tm_mday=d;

	// Use libc to do date stuff for us.  It's a hack, but I'm proud of it

	time_t t=mktime(&tmDummy);

	if (t == (time_t)-1)
		return; // Shouldn't happen


	if (howMuch < 0)
		t -= 12 * 60 * 60;
	else
		t += 36 * 60 * 60;

	struct tm *tmptr=localtime(&t);

	if (!tmptr)
		return;

	y=tmptr->tm_year + 1900;
	m=tmptr->tm_mon + 1;
	d=tmptr->tm_mday;
}

// Return last day of month

int searchPrompt::ldom(int &mm, int &yy)
{
	int d=1;
	int m=mm;
	int y=yy;

	if (++m > 12)
	{
		m=1;
		++y;
	}

	dayAdd(d, m, y, -1);

	if (d == 1)
		return -1;

	return d;
}

void searchPrompt::monAdd(int &d, int &m, int &y, int howMuch)
{
	bool isEom= ldom(m, y) == d;

	if (howMuch < 0)
	{
		if (--m <= 0)
		{
			m=12;
			--y;
		}

	} else if (++m > 12)
	{
		m=1;
		++y;
	}

	int eom=ldom(m, y);

	if (eom < 0)
	{
		d=1;
		return;
	}

	if (isEom || eom < d)
		d=eom;
}

void searchPrompt::yearAdd(int &d, int &m, int &y, int howMuch)
{
	int n;

	for (n=0; n<12; n++)
		monAdd(d, m, y, howMuch);
}

bool searchPrompt::searchPromptSize(mail::searchParams &searchInfo)
{
	myServer::promptInfo prompt=myServer
		::prompt(myServer
			 ::promptInfo(_("Size: "))
			 .option(key_LARGER,
				 Gettext::keyname(_("LARGER_K:L")),
				 _("Larger than"))

			 .option(key_SMALLER,
				 Gettext::keyname(_("SMALLER_K:S")),
				 _("Smaller than")));


	if (prompt.abortflag || myServer::nextScreen)
		return true;

	unicode_char promptKey=prompt.firstChar();

	if (key_SMALLER == promptKey)
	{
		searchInfo.criteria=searchInfo.smaller;
		return searchPromptSize(searchInfo,
					_("Smaller than: "));
	}

	if (key_LARGER == promptKey)
	{
		searchInfo.criteria=searchInfo.larger;
		return searchPromptSize(searchInfo,
					_("Larger than: "));
	}

	return true;
}

bool searchPrompt::searchPromptSize(mail::searchParams &searchInfo,
				    std::string promptStr)
{
	myServer::promptInfo prompt=myServer
		::prompt(myServer::promptInfo(promptStr));

	if (prompt.abortflag || myServer::nextScreen)
		return true;

	searchInfo.param2=prompt;
	return false;
}
