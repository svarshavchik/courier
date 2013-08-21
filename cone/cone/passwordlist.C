/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "gettext.H"
#include "passwordlist.H"
#include "myserver.H"
#include "myserverpromptinfo.H"
#include "curses/cursesstatusbar.H"
#include <unistd.h>

using namespace std;

extern CursesStatusBar *statusBar;

PasswordList PasswordList::passwordList;

PasswordList::PasswordList()
{
}

PasswordList::~PasswordList()
{
}

bool PasswordList::check(string url, string &pwd)
{
	string promptStr=_("Master Password (CTRL-C: cancel): ");

	// The following while() is really an if()

	while (!hasMasterPassword() && hasMasterPasswordFile())
	{
		myServer::promptInfo response=
			myServer::prompt(myServer::promptInfo(promptStr)
					 .password());

		if (response.abortflag)
			break;

		string p=response;

		if (p.size() == 0)
			break;

		if (myServer::loadpasswords(Gettext::toutf8(p)))
		{
			masterPassword=p;
			break;
		}

		promptStr=_("Invalid password, try again (CTRL-C: cancel): ");
		statusBar->beepError();
	}

	return get(url, pwd);
}

bool PasswordList::get(string url, string &pwd)
{
	map<string, string>::iterator p=list.find(url);

	if (p == list.end())
		return false;

	pwd= p->second;

	return true;
}

void PasswordList::save(string url, string pwd)
{
	map<string, string>::iterator p=list.find(url);

	if (p != list.end())
	{
		if (p->second == pwd)
			return;
		list.erase(p);
	}

	list.insert(make_pair(url, pwd));
	save();
}

void PasswordList::save()
{
	if (hasMasterPassword())
		myServer::savepasswords(masterPassword);
}

void PasswordList::remove(string url)
{
	map<string, string>::iterator p=list.find(url);

	if (p == list.end())
		return;

	list.erase(p);

	if (hasMasterPassword())
		myServer::savepasswords(masterPassword);
}

/////////////////////////////////////////////////////////////////////////////
//
// Master password support.

bool PasswordList::hasMasterPassword()
{
	return masterPassword.size() > 0;
}

bool PasswordList::hasMasterPasswordFile()
{
	string f=masterPasswordFile();

	return access(f.c_str(), R_OK) == 0;
}

void PasswordList::setMasterPassword(std::string newPassword)
{
	if (newPassword.size() == 0)
	{
		string n=masterPasswordFile();

		unlink(n.c_str());
	}
	else
	{
		myServer::savepasswords(newPassword);
	}

	masterPassword=newPassword;
}

void PasswordList::loadPasswords(const char * const *uids,
				 const char * const *pw)
{
	myServer::certs->certs.clear();

	for (; *uids && *pw; ++uids, ++pw)
	{
		if (strncmp(*uids, "ssl:", 4) == 0)
		{
			const char *name, *cert;

			std::string id=*uids + 4;

			name= *pw;
			cert=strchr(name, '\n');
			if (!cert)
				continue;

			Certificates::cert newCert;

			newCert.name=Gettext::fromutf8(std::string(name,
								   cert));
			newCert.cert=++cert;
			myServer::certs->certs[id]=newCert;
			continue;
		}

		list.insert(make_pair(string(*uids), string(*pw)));
	}
}


