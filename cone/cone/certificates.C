/*
** Copyright 2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include "unicode/unicode.h"
#include "tcpd/libcouriertls.h"

#include "curses/cursesscreen.H"
#include "curses/cursesbutton.H"
#include "curses/cursestitlebar.H"
#include "curses/cursesstatusbar.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursesdialog.H"
#include "curses/curseslabel.H"
#include "curses/cursesfield.H"
#include "curses/curseskeyhandler.H"

#include "libmail/logininfo.H"
#include "libmail/rfc2047encode.H"
#include "libmail/rfc2047decode.H"

#include "gettext.H"
#include "certificates.H"
#include "myserver.H"
#include "myserverpromptinfo.H"
#include "globalkeys.H"
#include "opendialog.H"
#include "mainmenu.H"
#include "certificates.H"
#include "passwordlist.H"
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <list>
#include <set>
#include <map>
#include <algorithm>

extern CursesMainScreen *mainScreen;
extern CursesStatusBar *statusBar;
extern CursesTitleBar *titleBar;
extern Gettext::Key key_ABORT;
extern Gettext::Key key_DELETECERTIFICATE;
extern Gettext::Key key_IMPORTCERTIFICATE;
extern Gettext::Key key_RENAMECERTIFICATE;

extern void mainMenu(void *);
void certificatesScreenInternal(void *);
void certificatesScreen(void *);

class CertificatesScreen : public CursesContainer ,
			   public CursesKeyHandler {

	CursesLabel title;
	CursesDialog menu;

public:
	static std::string jumpto;

	class Certificate : public CursesButton {

	public:
		CertificatesScreen *parent;
		std::list<Certificate>::iterator myIter;
		std::string id;

		Certificate(CertificatesScreen *parentScreen,
			    std::string name,
			    std::string id);
		~Certificate();

		bool processKeyInFocus(const Key &key);

		static void isused(std::string name);
	};
private:
	std::list<Certificate> certificate_list;

	std::list<Certificate>::iterator focusTo;
	bool foundfocus;

public:
	CertificatesScreen(CursesMainScreen *parent);
	~CertificatesScreen();

	void requestFocus();

	void addCertificate(std::string name, std::string id);
	void removeCertificate(std::list<Certificate>::iterator);

private:
	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string,std::string> > &list);
};

std::string CertificatesScreen::jumpto;

CertificatesScreen::Certificate::Certificate(CertificatesScreen *parentScreen,
					     std::string name,
					     std::string idArg)
	: CursesButton(parentScreen, name), parent(parentScreen),
	  id(idArg)
{
	setStyle(MENU);
}

void CertificatesScreen::Certificate::isused(std::string name)
{
	statusBar->clearstatus();
	statusBar->status(Gettext(_("This certificate is used by %1%"))
			  << name,
			  statusBar->SYSERROR);
	statusBar->beepError();
	mainScreen->draw();
}

bool CertificatesScreen::Certificate::processKeyInFocus(const Key &key)
{
	if (key == key_DELETECERTIFICATE)
	{
		std::vector<myServer *>::iterator b, e;

		for (b=myServer::server_list.begin(),
			     e=myServer::server_list.end();
		     b != e; ++b)
		{
			if ((*b)->certificate == id)
			{
				isused( (*b)->serverName);
				return true;
			}
		}

		if (id == myServer::smtpServerCertificate)
		{
			mail::loginInfo SMTPServerLoginInfo;

			mail::loginUrlDecode(myServer::smtpServerURL,
					     SMTPServerLoginInfo);

			isused(Gettext(_("%1% (SMTP server)")) <<
			       SMTPServerLoginInfo.server);
			return true;

		}

		myServer::promptInfo askdel(_("Delete this certificate? (Y/N) "));

		askdel=myServer::prompt(askdel.yesno());

		if (askdel.abortflag || (std::string)askdel != "Y")
			return true;

		parent->removeCertificate(myIter);
		return true;
	}

	if (key == key_RENAMECERTIFICATE)
	{
		myServer::promptInfo
			newname(_("New certificate name: "));

		newname=myServer::prompt(newname);

		if (newname.abortflag)
			return true;

		std::string newname_s=newname;

		if (newname_s.size() == 0)
			return true;

		myServer::certs->certs[id].name=newname_s;
		setText(newname_s);
		PasswordList::passwordList.save();
		return true;
	}

	return CursesButton::processKeyInFocus(key);
}

CertificatesScreen::Certificate::~Certificate()
{
}

CertificatesScreen::~CertificatesScreen()
{
}

CertificatesScreen::CertificatesScreen(CursesMainScreen *parent)
	: CursesContainer(parent),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  title(this, _("Certificates")),
	  menu(this)
{
	setRow(2);
	setAlignment(Curses::PARENTCENTER);
	title.setAlignment(Curses::PARENTCENTER);

	menu.setRow(2);

	titleBar->setTitles(_("CERTIFICATES"), "");

	foundfocus=false;

	std::map<std::string, Certificates::cert>::iterator p;

	for (p=myServer::certs->certs.begin();
	     p != myServer::certs->certs.end(); ++p)
		addCertificate(p->second.name, p->first);
}

void CertificatesScreen::addCertificate(std::string name, std::string id)
{
	certificate_list.push_back(Certificate(this, name, id));

	std::list<Certificate>::iterator new_cert= --certificate_list.end();

	new_cert->myIter=new_cert;

	menu.addPrompt(NULL, &*new_cert);

	if (jumpto == id)
	{
		foundfocus=true;
		focusTo=new_cert;
	}
}

void CertificatesScreen::removeCertificate(std::list<Certificate>::iterator i)
{
	std::map<std::string, Certificates::cert>::iterator ei=
		myServer::certs->certs.find(i->id);

	if (ei == myServer::certs->certs.end()) return; // ???

	std::list<Certificate>::iterator p=i;

	myServer::nextScreen= &certificatesScreenInternal;
	myServer::nextScreenArg=NULL;

	if (++p == certificate_list.end())
	{
		p=i;
		if (p != certificate_list.begin())
			--p;
	}

	keepgoing=false;
	if (p == certificate_list.end())
		myServer::nextScreen= &certificatesScreen;
	else
		jumpto=p->id;

	myServer::certs->certs.erase(ei);
	PasswordList::passwordList.save();
}

void CertificatesScreen::requestFocus()
{
	if (foundfocus)
	{
		focusTo->requestFocus();
		foundfocus=false;
		return;
	}

	if (!certificate_list.empty())
		certificate_list.front().requestFocus();
}

bool CertificatesScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		keepgoing=false;
		myServer::nextScreen= &mainMenu;
		myServer::nextScreenArg=NULL;
		PreviousScreen::previousScreen();
		return true;
	}

	if (key == key_IMPORTCERTIFICATE)
	{
		std::string filename;

		{
			OpenDialog open_dialog;

			open_dialog.requestFocus();
			myServer::eventloop();

			std::vector<std::string> &filenameList=
				open_dialog.getFilenameList();

			if (filenameList.size())
				filename=filenameList[0];
		}

		mainScreen->erase();
		mainScreen->draw();
		requestFocus();
		if (filename.size() == 0)
			return true;

		std::string cert;

		{
			std::stringstream ss;
			std::ifstream ifs(filename.c_str());

			if (!ifs.is_open())
			{
				statusBar->clearstatus();
				statusBar->status(strerror(errno),
						  statusBar->SYSERROR);
				statusBar->beepError();
				mainScreen->draw();
				return true;
			}

			ss << ifs.rdbuf();
			cert=ss.str();
		}

		if (cert.size() >= 0xFFF0)
		{
			statusBar->clearstatus();
			statusBar->status(_("Certificate file too large"),
					  statusBar->SYSERROR);
			statusBar->beepError();
			return true;
		}

		if (!tls_validate_pem_cert(cert.c_str(), cert.size()))
		{
			statusBar->clearstatus();
			statusBar->status(_("Cannot parse certificate file"),
					  statusBar->SYSERROR);
			statusBar->beepError();
			return true;
		}

		std::string name="XXX";

		char *s=tls_cert_name(cert.c_str(), cert.size());

		try {
			if (s)
			{
				name=s;
				free(s);
			}
		} catch (...) {
			if (s)
				free(s);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		std::map<std::string, Certificates::cert>::iterator i;

		for (i=myServer::certs->certs.begin();
		     i != myServer::certs->certs.end(); ++i)
		{
			std::string name2="YYY";

			s=tls_cert_name(i->second.cert.c_str(),
					i->second.cert.size());

			try {
				if (s)
				{
					name2=s;
					free(s);
					if (name2 == name)
						break;
				}
			} catch (...) {
				if (s)
					free(s);
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}
		}

		// Most names are very long

		if (name.size() > 60)
		{
			std::string::iterator first=name.begin();

			while (first != name.end())
			{
				if (*first == '\\')
				{
					if (++first == name.end())
						break;
					++first;
					continue;
				}

				if (*first == ',')
					break;
				++first;
			}

			std::string::iterator comma_sep=first;

			if (comma_sep != name.end()) ++comma_sep;

			size_t l=first-name.begin();

			if (l < 57)
			{
				l=57-l;

				if (l < (size_t)(name.end() - comma_sep))
				{
					while (comma_sep != name.end())
					{
						if (*comma_sep == '\\')
						{
							if (++comma_sep ==
							    name.end())
								break;
							++comma_sep;
							continue;
						}

						if (*comma_sep == ',' &&
						    comma_sep >= name.end()-l)
							break;
						++comma_sep;
					}

					if (comma_sep != name.end())
					{
						name=std::string(name.begin(),
								 first)
							+ "..."
							+ std::string(comma_sep,
								      name.end()
								      );
					}
				}
			}
		}

		std::string tryid;
		Certificates::cert c;

		if (i != myServer::certs->certs.end())
		{
			myServer::promptInfo
				ask(_("Replace existing cert? (Y/N) "));

			ask=myServer::prompt(ask.yesno());

			if (ask.abortflag)
				return true;

			if ((std::string)ask != "Y")
				return true;

			tryid=i->first;
			c=i->second;
			name=c.name;
		}
		else
		{
			size_t n=0;
			time_t t=time(NULL);

			do
			{
				std::ostringstream o;

				o << t << "-" << std::setw(3)
				  << std::setfill('0') << ++n;

				tryid=o.str();
			} while (myServer::certs->certs.find(tryid)
				 != myServer::certs->certs.end());
		}

		c.name=name;
		c.cert=cert;

		myServer::certs->certs[tryid]=c;

		PasswordList::passwordList.save();

		keepgoing=false;
		myServer::nextScreen= &certificatesScreenInternal;
		myServer::nextScreenArg=NULL;
		jumpto=tryid;
		return true;
	}

	return GlobalKeys::processKey(key, GlobalKeys::CERTIFICATESCREEN,
				      NULL);
}

bool CertificatesScreen::listKeys( std::vector< std::pair<std::string,
				   std::string> > &list)
{
	list.push_back( std::make_pair(Gettext::keyname(_("DELETECERT:D")),
				       _("Delete Certificate")));
	list.push_back( std::make_pair(Gettext::keyname(_("IMPORTCERT:I")),
				       _("Import Certificate")));
	list.push_back( std::make_pair(Gettext::keyname(_("RENAMECERT:R")),
				       _("Rename Certificate")));

	GlobalKeys::listKeys(list, GlobalKeys::CERTIFICATESCREEN);
	return false;
}

void certificatesScreenInternal(void *)
{
	CertificatesScreen screen(mainScreen);

	screen.draw();
	screen.requestFocus();
	myServer::eventloop();
}

void certificatesScreen(void *)
{
	CertificatesScreen::jumpto="";

	certificatesScreenInternal(0);
}

class ChooseCertificateScreen : public CursesContainer,
				public CursesKeyHandler {

	CursesLabel title;
	CursesDialog menu;
public:
	class cert : public CursesButton {

	public:
		std::string id;
		ChooseCertificateScreen *parent;

		cert(ChooseCertificateScreen *parent, std::string label,
		     std::string id);
		~cert();
		void clicked();
	};

	std::list<cert> certs;
	std::string selected;

	bool isDialog() const;	// Yes we are
	bool processKey(const Curses::Key &key);
	void requestFocus();

	ChooseCertificateScreen();
	~ChooseCertificateScreen();
};

ChooseCertificateScreen::cert::cert(ChooseCertificateScreen *parentArg,
				    std::string label, std::string idArg)
	: CursesButton(parentArg, label), id(idArg), parent(parentArg)
{
	setStyle(MENU);
}

ChooseCertificateScreen::cert::~cert()
{
}

void ChooseCertificateScreen::cert::clicked()
{
	parent->selected=id;
	keepgoing=false;
}

ChooseCertificateScreen::ChooseCertificateScreen()
	: CursesContainer(mainScreen),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  title(this, _("Certificates")),
	  menu(this)
{
	setRow(2);
	setAlignment(Curses::PARENTCENTER);
	title.setAlignment(Curses::PARENTCENTER);

	menu.setRow(2);

	std::map<std::string, Certificates::cert>::iterator p;

	for (p=myServer::certs->certs.begin();
	     p != myServer::certs->certs.end(); ++p)
	{
		certs.push_back(cert(this, p->second.name, p->first));
		menu.addPrompt(NULL, &certs.back());
	}
}

void ChooseCertificateScreen::requestFocus()
{
	if (!certs.empty())
		certs.front().requestFocus();
}

ChooseCertificateScreen::~ChooseCertificateScreen()
{
	keepgoing=true;
}

bool ChooseCertificateScreen::isDialog() const
{
	return true;
}

bool ChooseCertificateScreen::processKey(const Curses::Key &key)
{
	if (key == key_ABORT)
	{
		keepgoing=false;
		return true;
	}

	return GlobalKeys::processKey(key, GlobalKeys::CHOOSECERTIFICATESCREEN,
				      NULL);
}

std::string ChooseCertificate()
{
	std::string s;

	{
		ChooseCertificateScreen screen;

		screen.requestFocus();

		myServer::eventloop();

		s=screen.selected;
	}

	mainScreen->erase();
	mainScreen->draw();
	return s;
}
