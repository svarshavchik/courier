/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "opendialog.H"
#include "gettext.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursesstatusbar.H"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

using namespace std;

extern CursesMainScreen *mainScreen;
extern CursesStatusBar *statusBar;

OpenDialog::OpenDialog() : CursesFileReq(mainScreen,
					 _("Open: "),
					 _("Directory: ")), closing(false),
			   noMultiplesFlag(false)
{
}

OpenDialog::~OpenDialog()
{
}

void OpenDialog::selected(vector<string> &filenamesArg)
{
	filenames=filenamesArg;

	if (filenamesArg.size() == 0)
		return;

	if (noMultiplesFlag && filenames.size() != 1)
	{
		statusBar->clearstatus();
		statusBar->status(_("Ambiguous filename pattern (more than one file matches)"),
				  statusBar->SYSERROR);
		statusBar->beepError();
		return;
	}

	Curses::keepgoing=false;
	closing=true;
}

void OpenDialog::abort()
{
	filenames.clear();
	keepgoing=false;
	closing=true;
}

// We always call this function, so take the opportunity to reset keepgoing.

std::vector<std::string> &OpenDialog::getFilenameList()
{
	if (closing)
		keepgoing=true;
	return filenames;
}

