/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "gettext.H"
#include "myserverpromptinfo.H"
#include "curses/cursesfield.H"
#include "curses/cursesstatusbar.H"
#include "curses/widechar.H"

extern CursesStatusBar *statusBar;

myServer::promptInfo::promptInfo(std::string promptArg)
	: prompt(promptArg), value(""), isPassword(false),
	  isYesNo(false),
	  currentFocus(Curses::currentFocus), abortflag(false)
{
}

myServer::promptInfo::~promptInfo()
{
}

myServer::promptInfo &myServer::promptInfo::initialValue(std::string v)
{
	value=v;
	return *this;
}

myServer::promptInfo &myServer::promptInfo::password()
{
	isPassword=true;
	return *this;
}

myServer::promptInfo &myServer::promptInfo::yesno()
{
	isYesNo=true; return *this;
}


myServer::promptInfo &myServer::promptInfo::option(Gettext::Key &theKey,
						   std::string keyname,
						   std::string keydescr)
{
	optionHelp.push_back(make_pair(keyname, keydescr));

	std::vector<unicode_char> ukv=theKey;
	optionList.insert(optionList.end(), ukv.begin(), ukv.end());
	return *this;

}

myServer::promptInfo myServer::prompt(myServer::promptInfo info)
{
	CursesField *f=statusBar->createPrompt(info.prompt,
					       info.value);

	if (!f)
	{
		info.abortflag=true;
		return info;
	}

	if (info.isPassword)
		f->setPasswordChar();

	if (info.isYesNo)
		f->setYesNo();

	if (info.optionHelp.size() > 0)
		f->setOptionHelp(info.optionHelp);

	if (info.optionList.size() > 0)
		f->setOptionField(info.optionList);

	//	f->setText(info.value);
	while (statusBar->prompting())
		eventloop();

	info.abortflag=statusBar->promptAborted();
	info.value=statusBar->getPromptValue();

	Curses::keepgoing=true;
	if (info.currentFocus)
		info.currentFocus->requestFocus();

	return info;
}

unicode_char myServer::promptInfo::firstChar()
{
	std::vector<unicode_char> buf;

	mail::iconvert::convert(value, unicode_default_chset(), buf);

	if (buf.empty())
		buf.push_back(0);

	return *buf.begin();
}
