/*
** Copyright 2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "config.h"
#include "savedialog.H"
#include "myserver.H"
#include "myserverpromptinfo.H"
#include "gettext.H"
#include "curses/cursesmainscreen.H"
#include "init.H"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

extern CursesMainScreen *mainScreen;

SaveDialog::SaveDialog(string defaultName) : CursesFileReq(mainScreen,
							   _("Save As: "),
							   _("Directory: ")),
					     closing(false)
{
	setFilename(defaultName);
}

SaveDialog::~SaveDialog()
{
}

void SaveDialog::selected(std::vector<std::string> &filenames)
{
	struct stat stat_buf;

	if (filenames.size() == 0)
		return;

	if (filenames.size() != 1)
	{
		statusBar->clearstatus();
		statusBar->status(_("Ambiguous filename pattern (more than one file matches)"),
				  statusBar->SYSERROR);
		statusBar->beepError();
		return;
	}

	string filenameArg=filenames[0];

	if (stat(filenameArg.c_str(), &stat_buf) == 0)
	{
		myServer::promptInfo response=
			myServer::prompt(myServer::promptInfo(_("Overwrite existing file? (Y/N) "))
					 .yesno());

		if (response.abortflag || (string)response != "Y")
			return;
	}

	filename=filenameArg;
	Curses::keepgoing=false;
	closing=true;
}

void SaveDialog::abort()
{
	filename="";
	keepgoing=false;
	closing=true;
}

// We always call this function, so take the opportunity to reset keepgoing.

SaveDialog::operator string()
{
	if (closing)
		keepgoing=true;
	return filename;
}

