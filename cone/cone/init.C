/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "curses/cursesdialog.H"
#include "curses/curseslabel.H"
#include "curses/cursesfield.H"
#include "curses/cursesfilereq.H"
#include "curses/cursesmoronize.H"

#include <courier-unicode.h>
#include "gettext.H"
#include "messagesize.H"
#include <string>
#include <iostream>
#include <climits>

#include "init.H"
#include "buildversion.H"
#include "libmail/mail.H"

using namespace std;

extern Gettext::Key key_MORE;
extern Gettext::Key key_YANK;
extern Gettext::Key key_CLREOL;

/*
** Common init code between cone and leaf.
*/

CursesScreen *cursesScreen=NULL;
CursesMainScreen *mainScreen=NULL;
CursesStatusBar *statusBar=NULL;
CursesTitleBar *titleBar=NULL;
SpellCheckerBase *spellCheckerBase=NULL;

bool usesampm;

//
// Open/Save dialog indicator of file size or directory.

static string myFmtDirEntrySize(unsigned long size, bool isDir)
{
	if (isDir)
		return _(" (Dir)");

	return Gettext(_(" (%1%)")) << (string)MessageSize(size, false);
}

void outofmemory()
{
	cerr << "Out of memory" << flush;
	LIBMAIL_THROW(_("Out of memory."));
}

void init()
{
	setlocale(LC_ALL, "");
	textdomain("cone");
	signal(SIGCHLD, SIG_DFL);

	// Try to guess whether the local system uses a 24 hour clock, or
	// AM/PM.

	{
		time_t t=time(NULL);
		struct tm tmptr= *localtime(&t);

		tmptr.tm_hour=21;
		tmptr.tm_min=0;
		tmptr.tm_sec=0;

		char buf[80];

		usesampm=false;

		if (strftime(buf, sizeof(buf), "%X", &tmptr) > 0 &&
		    strchr(buf, '9'))
			usesampm=true;
	}

#if 0
	close(2);
	if (open("debug.log", O_CREAT|O_TRUNC|O_WRONLY|O_APPEND, 0666)
	    != 2)
		exit(1);
#endif

	{
		string chset=unicode_default_chset();

		vector<unicode_char> dummy;

		if (!unicode::iconvert::convert("", chset, dummy))
		{
			cerr << (string)
				(Gettext(_("ERROR: Your display appears to be set to the %1% character set.\n"
					   "This application cannot display this character set.  If this application did\n"
					   "not read the display character set name correctly, the name of the display's\n"
					   "character set name can be manually specified using the CHARSET environment\n"
					   "variable.  Otherwise reconfigure your display to use a supported character\n"
					   "set and try again.\n"))
				 << chset) << flush;
			exit(1);
		}
	}

	{
		size_t i;

		for (i=0; CursesMoronize::moronizationList[i].keycode; i++)
		{
			size_t j;

			for (j=0; CursesMoronize::moronizationList[i]
				     .replacements[j]; ++j)
				;

			vector<unicode_char>
				uc(CursesMoronize::moronizationList[i]
				   .replacements,
				   CursesMoronize::moronizationList[i]
				   .replacements+j);

			bool err;
			string s=
				unicode::iconvert::convert(uc,
							unicode_default_chset(),
							err);

			if (s.size() == 0 || err)
				CursesMoronize::moronizationList[i].replacements
					=NULL;
		}
	}

	// Initialize global locate-specific text.

	CursesStatusBar::extendedErrorPrompt=
		_("Press SPACE to continue.");
	CursesStatusBar::shortcut_next_key= _("^O");
	CursesStatusBar::shortcut_next_descr= _("mOre");

#define TOKEYCODE(s) ( ((const vector<unicode_char> &)(s)).size() > 0 ? \
		((const vector<unicode_char> &)(s))[0]:0)

	CursesStatusBar::shortcut_next_keycode=TOKEYCODE(key_MORE);

	unicode::iconvert::convert(std::string(_("yY")),
				unicode_default_chset(), CursesField::yesKeys);
	unicode::iconvert::convert(std::string(_("nN")),
				unicode_default_chset(), CursesField::noKeys);
	CursesField::yankKey=TOKEYCODE(key_YANK);
	CursesField::clrEolKey=TOKEYCODE(key_CLREOL);

	CursesFileReq::fmtSizeFunc= &myFmtDirEntrySize;


}

void version()
{
	cout << "Cone " VERSION "/" BUILDVERSION << endl;
	exit(0);
}

string curdir()
{
	char b[PATH_MAX+1];

	b[sizeof(b)-1]=0;

	if (getcwd(b, sizeof(b)-1) == NULL)
	{
		cerr << "getcwd: " << strerror(errno) << endl;
		exit(1);
	}

	return b;
}

