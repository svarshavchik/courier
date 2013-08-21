/*
** Copyright 2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "addmessage.H"
#include "imap.H"
#include "misc.H"
#include "imapacl.H"
#include "imapfolder.H"
#include "imapfolders.H"
#include "imaphandler.H"
#include "imaplisthandler.H"
#include "maildir/maildiraclt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sstream>
#include <algorithm>

using namespace std;


LIBMAIL_START

/*
** ACL1 MYRIGHTS response.
*/

class imapGetMyRights::parseMyRights : public imapHandlerStructured {

	string folder;
	string &rights;

	string curfolder;
	void (parseMyRights::*next_func)(imap &, Token);

public:
	parseMyRights(string folderArg, string &rightsArg);
	~parseMyRights();
	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *);
	void process(imap &imapAccount, Token t);

	void getFolder(imap &imapAccount, Token t);
	void getRights(imap &imapAccount, Token t);

};


/*
** ACL2 LIST(MYRIGHTS) handler.
*/

class imapGetMyRights::listMyRights : private vector<imapFolder>,
		       public imapLIST {

	string folderName;
	string &rights;

public:
	listMyRights(string folderNameArg,
		     string &rightsArg);
	~listMyRights();

	void processExtendedAttributes(imapFolder &);
};

LIBMAIL_END

mail::imapGetMyRights::imapGetMyRights(string folderNameArg, string &rightsArg,
				       mail::callback &callbackArg)
	: folderName(folderNameArg),
	  rights(rightsArg),
	  callback(callbackArg)
{
}

mail::imapGetMyRights::~imapGetMyRights()
{
}

void mail::imapGetMyRights::installed(imap &imapAccount)
{
	if (imapAccount.getCapability("ACL") == "ACL")
	{
		/* ACL 1 */
	
		imapAccount.imapcmd("MYRIGHTS",
				    "MYRIGHTS "
				    + imapAccount.quoteSimple(folderName)
				    + "\r\n");
	}
	else
	{
		imapAccount.imapcmd("MYRIGHTS",
				    "LIST (MYRIGHTS) \"\" "
				    + imapAccount.quoteSimple(folderName)
				    + "\r\n");
	}
}

const char *mail::imapGetMyRights::getName()
{
	return "MYRIGHTS";
}

void mail::imapGetMyRights::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

bool mail::imapGetMyRights::untaggedMessage(imap &imapAccount,
					    string msgname)
{
	if (msgname == "MYRIGHTS")
	{
		imapAccount
			.installBackgroundTask(new parseMyRights(folderName,
								 rights));
		return true;
	}

	if (msgname == "LIST")
	{
		imapAccount
			.installBackgroundTask(new listMyRights(folderName,
								rights));
		return true;
	}
	return false;
}

bool mail::imapGetMyRights::taggedMessage(imap &imapAccount, string msgname,
					  string message,
					  bool okfail, string errmsg)
{
	if (!okfail)
	{
		callback.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}
	callback.success(message);
	imapAccount.uninstallHandler(this);
	return true;
}

/* ---- Parse ACL1 MYRIGHTS response ----- */

mail::imapGetMyRights::parseMyRights::parseMyRights(string folderArg,
						    string &rightsArg)
	: folder(folderArg), rights(rightsArg),
	  next_func(&mail::imapGetMyRights::parseMyRights::getFolder)
{
	rights="";
}

mail::imapGetMyRights::parseMyRights::~parseMyRights()
{
}

void mail::imapGetMyRights::parseMyRights::installed(imap &imapAccount)
{
}

const char *mail::imapGetMyRights::parseMyRights::getName()
{
	return "LISTMYRIGHTS_FOLDER";
}

void mail::imapGetMyRights::parseMyRights::timedOut(const char *dummy)
{
}

void mail::imapGetMyRights::parseMyRights::process(imap &imapAccount, Token t)
{
	(this->*next_func)(imapAccount, t);
}

void mail::imapGetMyRights::parseMyRights::getFolder(imap &imapAcct, Token t)
{
	if (t != ATOM && t != STRING)
	{
		error(imapAcct);
		return;
	}
	curfolder=t.text;
	next_func=&mail::imapGetMyRights::parseMyRights::getRights;
}

void mail::imapGetMyRights::parseMyRights::getRights(imap &imapAcct, Token t)
{
	if (t == EOL)
	{
		done();
		return;
	}

	if (t != ATOM && t != STRING)
	{
		error(imapAcct);
		return;
	}

	string s=t.text;

	size_t p=s.find(ACL_DELETE_SPECIAL[0]);

	if (p != std::string::npos)
		s.replace(s.begin()+p, s.begin()+p+1,
			  ACL_DELETEMSGS ACL_DELETEFOLDER ACL_EXPUNGE);
	sort(s.begin(), s.end());

	if (curfolder == folder)
		rights=s;
}

/* ---- Parse ACL2 LIST(MYRIGHTS) response ----- */

mail::imapGetMyRights::listMyRights::listMyRights(string folderNameArg,
						  string &rightsArg)
	: imapLIST( *this, 0), folderName(folderNameArg),
	  rights(rightsArg)
{
}

mail::imapGetMyRights::listMyRights::~listMyRights()
{
}

void mail::imapGetMyRights::listMyRights
::processExtendedAttributes(imapFolder &f)
{
	if (f.getPath() != folderName)
		return; /* Sanity check */

	vector<mail::imapparsefmt *>::iterator b=xattributes.children.begin(),
		e=xattributes.children.end();

	while (b != e)
	{
		mail::imapparsefmt *node= *b++;

		if (node->children.size() < 2)
			continue;

		string name=(*node->children.begin())->value;

		mail::upper(name);

		if (name != "MYRIGHTS")
			continue;

		rights=node->children.begin()[1]->value;

		size_t p=rights.find(ACL_DELETE_SPECIAL[0]);

		if (p != std::string::npos)
			rights.replace(rights.begin()+p, rights.begin()+p+1,
				  ACL_DELETEMSGS ACL_DELETEFOLDER ACL_EXPUNGE);

		sort(rights.begin(), rights.end());
		break;
	}

}

/***** GET RIGHTS *****/

LIBMAIL_START

/*
** ACL1 "ACL" response.
*/

class imapGetRights::parseGetRights : public imapHandlerStructured {

	string folder;
	list<pair<string,string> > &rights;

	string curfolder;
	string curidentifier;
	void (parseGetRights::*next_func)(imap &, Token);

public:
	parseGetRights(string folderArg, list<pair<string,string> >&rightsArg);
	~parseGetRights();
	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *);
	void process(imap &imapAccount, Token t);

	void getFolder(imap &imapAccount, Token t);
	void getIdentifier(imap &imapAccount, Token t);
	void getRights(imap &imapAccount, Token t);

};

LIBMAIL_END

/*
** ACL2 LIST(ACL) handler.
*/

class mail::imapGetRights::listGetRights : private vector<mail::imapFolder>,
	    public mail::imapLIST {

	string folderName;
	list< pair< string, string> > &rights;

public:
	listGetRights(string folderNameArg,
		      list<pair< string, string> > &rightsArg);
	~listGetRights();

	void processExtendedAttributes(mail::imapFolder &);
};

mail::imapGetRights::imapGetRights(string folderNameArg,
				   list<pair<string,string> >&rightsArg,
				   mail::callback &callbackArg)
	: folderName(folderNameArg), rights(rightsArg),
	  callback(callbackArg)
{
}

mail::imapGetRights::~imapGetRights()
{
}

void mail::imapGetRights::installed(imap &imapAccount)
{
	if (imapAccount.getCapability("ACL") == "ACL")
	{
		/* ACL 1 */
	
		imapAccount.imapcmd("GETACL",
				    "GETACL "
				    + imapAccount.quoteSimple(folderName)
				    + "\r\n");
	}
	else
	{
		imapAccount.imapcmd("GETACL",
				    "LIST (ACL) \"\" "
				    + imapAccount.quoteSimple(folderName)
				    + "\r\n");
	}

}

const char *mail::imapGetRights::getName()
{
	return "GETACL";
}

void mail::imapGetRights::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

bool mail::imapGetRights::untaggedMessage(imap &imapAccount, string msgname)
{
	if (msgname == "LIST")
	{
		imapAccount
			.installBackgroundTask(new listGetRights(folderName,
								 rights));
		return true;
	}

	if (msgname == "ACL")
	{
		imapAccount
			.installBackgroundTask(new parseGetRights(folderName,
								  rights));
		return true;
	}
	return false;
}

bool mail::imapGetRights::taggedMessage(imap &imapAccount, string msgname,
					string message,
					bool okfail, string errmsg)
{
	if (!okfail)
	{
		callback.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}
	callback.success(message);
	imapAccount.uninstallHandler(this);
	return true;
}

/* --- Process ACL1 "ACL" response --- */

mail::imapGetRights::parseGetRights::parseGetRights(string folderArg,
						  list<pair<string,string> >
						  &rightsArg)
	: folder(folderArg), rights(rightsArg),
	  next_func(&mail::imapGetRights::parseGetRights::getFolder)
{
	rights.clear();
}

mail::imapGetRights::parseGetRights::~parseGetRights()
{
}

void mail::imapGetRights::parseGetRights::installed(imap &imapAccount)
{
}

const char *mail::imapGetRights::parseGetRights::getName()
{
	return "GETACL_FOLDER";
}

void mail::imapGetRights::parseGetRights::timedOut(const char *dummy)
{
}

void mail::imapGetRights::parseGetRights::process(imap &imapAccount, Token t)
{
	(this->*next_func)(imapAccount, t);
}

void mail::imapGetRights::parseGetRights::getFolder(imap &imapAcct, Token t)
{
	if (t != ATOM && t != STRING)
	{
		error(imapAcct);
		return;
	}
	curfolder=t.text;
	next_func=&mail::imapGetRights::parseGetRights::getIdentifier;
}

void mail::imapGetRights::parseGetRights::getIdentifier(imap &imapAcct,Token t)
{
	if (t == EOL)
	{
		done();
		return;
	}

	if (t != ATOM && t != STRING)
	{
		error(imapAcct);
		return;
	}

	curidentifier=t.text;

	/* Fudge ACL 1 identifiers into ACL 2 identifiers */

	if (curidentifier != "anyone" &&
	    curidentifier != "anonymous" &&
	    curidentifier != "authuser" &&
	    curidentifier != "owner" &&
	    curidentifier != "administrators")
		curidentifier="user=" + curidentifier;

	next_func=&mail::imapGetRights::parseGetRights::getRights;
}

void mail::imapGetRights::parseGetRights::getRights(imap &imapAcct, Token t)
{
	if (t == EOL)
	{
		done();
		return;
	}

	if (t != ATOM && t != STRING)
	{
		error(imapAcct);
		return;
	}

	string s=t.text;

	size_t p=s.find(ACL_DELETE_SPECIAL[0]);

	if (p != std::string::npos)
		s.replace(s.begin()+p, s.begin()+p+1,
			  ACL_DELETEMSGS ACL_DELETEFOLDER ACL_EXPUNGE);
	sort(s.begin(), s.end());

	if (curfolder == folder)
		rights.push_back(make_pair(curidentifier, s));
	next_func=&mail::imapGetRights::parseGetRights::getIdentifier;
}

/* --- Process ACL2 LIST(ACL) response --- */

mail::imapGetRights
::listGetRights::listGetRights(string folderNameArg,
			       list<pair<string,string> > &rightsArg)
	: imapLIST(*this, 0), folderName(folderNameArg),
	  rights(rightsArg)
{
}

mail::imapGetRights::listGetRights::~listGetRights()
{
}

void mail::imapGetRights::listGetRights
::processExtendedAttributes(imapFolder &f)
{
	if (f.getPath() != folderName)
		return; /* Sanity check */

	vector<mail::imapparsefmt *>::iterator b=xattributes.children.begin(),
		e=xattributes.children.end();

	while (b != e)
	{
		mail::imapparsefmt *node= *b++;

		if (node->children.size() < 2)
			continue;

		string name=(*node->children.begin())->value;

		mail::upper(name);

		if (name != "ACL")
			continue;

		mail::imapparsefmt *rights_list=node->children.begin()[1];

		vector<mail::imapparsefmt *>::iterator
			bb=rights_list->children.begin(),
			ee=rights_list->children.end();

		while (bb != ee)
		{
			mail::imapparsefmt *node= *bb++;

			if (node->children.size() < 2)
				continue;

			string identifier=node->children.begin()[0]->value;
			string r=node->children.begin()[1]->value;

			size_t p=r.find(ACL_DELETE_SPECIAL[0]);

			if (p != std::string::npos)
				r.replace(r.begin()+p, r.begin()+p+1,
					  ACL_DELETEMSGS ACL_DELETEFOLDER
					  ACL_EXPUNGE);

			sort(r.begin(), r.end());
			rights.push_back(make_pair(identifier, r));
		}
		break;
	}
}

/* --- DELETE or SET ACL rights */


LIBMAIL_START

/* ACL2 RIGHTS-INFO and ACLFAILED untagged messages */

class imapSetRights::parseRightsInfo : public imapHandlerStructured {

	string &folderName;
	string &identifier;
	vector<string> &rights;

	void (parseRightsInfo::*next_func)(imap &, Token);

public:
	parseRightsInfo(string &folderNameArg,
			string &identifierArg,
			vector<string> &rightsArg);
	~parseRightsInfo();

	void installed(imap &imapAccount);
	const char *getName();
	void timedOut(const char *);
	void process(imap &imapAccount, Token t);

	void get_folder(imap &imapAccount, Token t);
	void get_identifier(imap &imapAccount, Token t);
	void get_rights(imap &imapAccount, Token t);

};

class imapSetRights::parseAclFailed : public imapHandlerStructured {

	string &folderName, &errmsg;
	bool wantErrorMessage;
	void (parseAclFailed::*next_func)(imap &, Token);
public:
	parseAclFailed(string &folderNameArg, string &errmsgArg);
	~parseAclFailed();

	void installed(imap &imapAccount);
	const char *getName();
	void timedOut(const char *);
	void process(imap &imapAccount, Token t);

private:
	int process(imap &imapAccount, std::string &buffer);
};
LIBMAIL_END

mail::imapSetRights::imapSetRights(string folderNameArg,
				   string identifierArg,
				   string rightsArg,
				   bool delIdentifierArg,
				   string &errorIdentifierArg,
				   vector<string> &errorRightsArg,
				   mail::callback &callbackArg)
	: folderName(folderNameArg),
	  identifier(identifierArg),
	  rights(rightsArg),
	  delIdentifier(delIdentifierArg),
	  errorIdentifier(errorIdentifierArg),
	  errorRights(errorRightsArg),
	  callback(callbackArg)
{
}

mail::imapSetRights::~imapSetRights()
{
}

void mail::imapSetRights::installed(imap &imapAccount)
{
	if (imapAccount.getCapability("ACL") == "ACL")
	{
		/* ACL 1 */

		string n=identifier;

		if (n != "anyone" &&
		    n != "anonymous" &&
		    n != "authuser" &&
		    n != "owner" &&
		    n != "administrators")
		{
			if (strncmp(n.c_str(), "user=", 5))
			{
				callback.fail("Invalid ACL identifier.");
				imapAccount.uninstallHandler(this);
				return;
			}

			n=n.substr(5);
		}

		// If all three "etx" were originally set, it would've been
		// replaced with a "d".  If we still have one or more of them
		// it means that not all three were specified.

		if (rights.find(ACL_DELETEMSGS[0]) != std::string::npos ||
		    rights.find(ACL_DELETEFOLDER[0]) != std::string::npos ||
		    rights.find(ACL_EXPUNGE[0]) != std::string::npos)
		{
			errorIdentifier=identifier;
			errorRights.push_back("");
			errorRights.push_back(ACL_DELETEMSGS
					      ACL_EXPUNGE
					      ACL_DELETEFOLDER);
			errorRights.push_back(ACL_LOOKUP);
			errorRights.push_back(ACL_READ);
			errorRights.push_back(ACL_SEEN);
			errorRights.push_back(ACL_WRITE);
			errorRights.push_back(ACL_INSERT);
			errorRights.push_back(ACL_POST);
			errorRights.push_back(ACL_CREATE);
			errorRights.push_back(ACL_ADMINISTER);
			callback.fail("Invalid access right combination.");
			imapAccount.uninstallHandler(this);
			return;
		}

		imapAccount.imapcmd("SETACL",
				    delIdentifier ?
				    "DELETEACL "
				    + imapAccount.quoteSimple(folderName)
				    + " "
				    + imapAccount.quoteSimple(n)
				    + "\r\n"
				    :
				    "SETACL "
				    + imapAccount.quoteSimple(folderName)
				    + " "
				    + imapAccount.quoteSimple(n)
				    + " "
				    + imapAccount.quoteSimple(rights)
				    + "\r\n");
	}
	else
	{
		imapAccount.imapcmd("SETACL",
				    delIdentifier ?
				    "ACL DELETE "
				    + imapAccount.quoteSimple(folderName)
				    + " "
				    + imapAccount.quoteSimple(identifier)
				    + "\r\n"
				    :
				    "ACL STORE "
				    + imapAccount.quoteSimple(folderName)
				    + " "
				    + imapAccount.quoteSimple(identifier)
				    + " "
				    + imapAccount.quoteSimple(rights)
				    + "\r\n");
	}
}

const char *mail::imapSetRights::getName()
{
	return "SETACL";
}

void mail::imapSetRights::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

bool mail::imapSetRights::untaggedMessage(imap &imapAccount, string msgname)
{
	if (msgname == "LIST")
	{
		dummy_rights.clear();
		imapAccount
			.installBackgroundTask(new imapGetRights
					       ::listGetRights(folderName,
							       dummy_rights));
		return true;
	}

	if (msgname == "ACLFAILED")
	{
		imapAccount
			.installBackgroundTask(new parseAclFailed(dummy,
								  dummy));
		return true;
	}

	if (msgname == "RIGHTS-INFO")
	{
		imapAccount
			.installBackgroundTask(new
					       parseRightsInfo(dummy,
							       errorIdentifier,
							       errorRights));
		return true;
	}

	return false;
}

bool mail::imapSetRights::taggedMessage(imap &imapAccount, string msgname,
					string message,
					bool okfail, string errmsg)
{
	if (okfail && errorIdentifier.size() > 0)
	{
		okfail=false;
		message="Invalid access rights.";
	}

	if (!okfail)
	{
		callback.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}
	callback.success(message);
	imapAccount.uninstallHandler(this);
	return true;
}


mail::imapSetRights::parseRightsInfo::parseRightsInfo(string &folderNameArg,
						      string &identifierArg,
						      vector<string> &rightsArg)
	: folderName(folderNameArg),
	  identifier(identifierArg),
	  rights(rightsArg), next_func(&mail::imapSetRights::parseRightsInfo
				       ::get_folder)
{
}

mail::imapSetRights::parseRightsInfo::~parseRightsInfo()
{
}

void mail::imapSetRights::parseRightsInfo::installed(imap &imapAccount)
{
}

const char *mail::imapSetRights::parseRightsInfo::getName()
{
	return "RIGHTS-INFO";
}

void mail::imapSetRights::parseRightsInfo::timedOut(const char *dummy)
{
}

void mail::imapSetRights::parseRightsInfo::process(imap &imapAccount, Token t)
{
	(this->*next_func)(imapAccount, t);
}

void mail::imapSetRights::parseRightsInfo::get_folder(imap &imapAccount,
						      Token t)
{
	if (t != ATOM && t != STRING)
	{
		error(imapAccount);
		return;
	}
	folderName=t.text;
	next_func= &mail::imapSetRights::parseRightsInfo::get_identifier;
}

void mail::imapSetRights::parseRightsInfo::get_identifier(imap &imapAccount,
							  Token t)
{
	if (t != ATOM && t != STRING)
	{
		error(imapAccount);
		return;
	}
	identifier=t.text;
	next_func= &mail::imapSetRights::parseRightsInfo::get_rights;
}

void mail::imapSetRights::parseRightsInfo::get_rights(imap &imapAccount,
						      Token t)
{
	if (t == EOL)
	{
		done();
		return;
	}
	if (t != ATOM && t != STRING)
	{
		error(imapAccount);
		return;
	}
	rights.push_back(t.text);
}

mail::imapSetRights::parseAclFailed::parseAclFailed(string &folderNameArg,
						    string &errmsgArg)
	: folderName(folderNameArg),
	  errmsg(errmsgArg),
	  wantErrorMessage(false)
{
}

mail::imapSetRights::parseAclFailed::~parseAclFailed()
{
}

void mail::imapSetRights::parseAclFailed::installed(imap &imapAccount)
{
}

const char *mail::imapSetRights::parseAclFailed::getName()
{
	return "ACLFAILED";
}

void mail::imapSetRights::parseAclFailed::timedOut(const char *dummy)
{
}

void mail::imapSetRights::parseAclFailed::process(imap &imapAccount,
						  Token t)
{
	if (t != ATOM && t != STRING)
	{
		error(imapAccount);
		return;
	}

	folderName=t.text;
	wantErrorMessage=true;
}

int mail::imapSetRights::parseAclFailed::process(imap &imapAccount,
						 string &buffer)
{
	if (!wantErrorMessage)
		return imapHandlerStructured::process(imapAccount, buffer);

	size_t p=buffer.find('\n');

	if (p == std::string::npos)
		return 0;

	string msg=buffer.substr(0, p);

	size_t cr=msg.find('\r');
	if (cr != std::string::npos)
		msg=msg.substr(0, cr);
	errmsg=msg;
	done();
	return (int)p;
}
