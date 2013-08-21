/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "curses/cursesscreen.H"
#include "curses/cursestitlebar.H"
#include "curses/cursesstatusbar.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursesdialog.H"
#include "curses/curseslabel.H"
#include "curses/cursesfield.H"
#include "curses/cursesfilereq.H"
#include "curses/curseskeyhandler.H"
#include "unicode/unicode.h"
#include "spellchecker.H"
#include "curseseditmessage.H"
#include "myserver.H"
#include "myserverpromptinfo.H"
#include "savedialog.H"
#include "opendialog.H"

#include "init.H"
#include "gettext.H"
#include "htmlentity.h"
#include "htmlparser.H"
#include "macros.H"
#include "colors.H"

#if HAVE_UTIME_H
#include <utime.h>
#endif

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

/*
** Borrow some classes from cone, and get an editor.
*/

using namespace std;

extern Gettext::Key key_OPENFILE;
extern Gettext::Key key_SAVEFILE;
extern Gettext::Key key_SAVEAS;
extern Gettext::Key key_TODIRECTORY;
extern Gettext::Key key_TOGGLEFLOWED;

unicode_char ularr, urarr, ucwrap, ucwrap_visible;

static bool flowedmode;

struct CustomColor color_md_headerName;

void (*myServer::nextScreen)(void *)=NULL; // LEAF dummy
void *myServer::nextScreenArg=NULL; // LEAF dummy

Demoronize *currentDemoronizer;

static void setflowedmode(bool flag)
{
	flowedmode=flag;
	ucwrap=flag ? ucwrap_visible:' ';
}

//
// Subclass CursesEditMessage to get something that looks like an editor.
//

class LeafEditMessage : public CursesEditMessage {

	bool modifiedFlag;

public:
	string filename;

	LeafEditMessage(CursesContainer *parent);
	~LeafEditMessage();

	void modified();

	void setModified(bool);
	void showModified();
	bool getModified() { return modifiedFlag; }
	string getFilenameBase();

	void loadingMessage();
	void savingMessage();
	void savedMessage();
	void saveTo();

	void cursorRowChanged();

	bool checkModified();
};

LeafEditMessage::LeafEditMessage(CursesContainer *parent)
	: CursesEditMessage(parent), modifiedFlag(false)
{
}

LeafEditMessage::~LeafEditMessage()
{
}

void LeafEditMessage::modified()
{
	setModified(true);
}

void LeafEditMessage::setModified(bool flag)
{
	if (flag != modifiedFlag)
	{
		modifiedFlag=flag;
		showModified();
	}
}

void LeafEditMessage::showModified()
{
	string m=getFilenameBase();

	if (modifiedFlag)
		m=Gettext(_("%1% (*)")) << m;

	titleBar->setTitles(m, Gettext(_("%1%/%2%")) << (currentLine()+1)
			    << numLines());
}

bool LeafEditMessage::checkModified()
{
	if (!getModified())
		return true;

	myServer::promptInfo response=
		myServer
		::prompt(myServer
			 ::promptInfo(Gettext(_("Modified file is not saved, lose all changes made? (Y/N) ")))
			 .yesno());

	if (!response.abortflag &&
	    (string)response == "Y")
		return true;
	return false;
}

void LeafEditMessage::cursorRowChanged()
{
	showModified();
}

string LeafEditMessage::getFilenameBase()
{
	size_t n=filename.rfind('/');

	if (n != std::string::npos)
		return filename.substr(n+1);

	return filename;
}

// Loading/saving might take a while.

void LeafEditMessage::loadingMessage()
{
	statusBar->clearstatus();
	statusBar->status(Gettext(_("Loading %1%...")) << getFilenameBase());
	statusBar->flush();
}

void LeafEditMessage::savingMessage()
{
	statusBar->clearstatus();
	statusBar->status(Gettext(_("Saving %1%...")) << getFilenameBase());
	statusBar->flush();
}

void LeafEditMessage::savedMessage()
{
	statusBar->clearstatus();
	statusBar->status(Gettext(_("Saved %1%")) << getFilenameBase());
}

void LeafEditMessage::saveTo()
{
	savingMessage();

	// Make a backup copy of the original file

	ifstream i(filename.c_str());
	struct stat stat_buf;

	if (i.is_open() && stat(filename.c_str(), &stat_buf) == 0)
	{
		string backup=filename + "~";

		unlink(backup.c_str());
		ofstream b(backup.c_str());

		if (!b.is_open() ||
		    chmod(backup.c_str(), stat_buf.st_mode & 0777)<0)
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return;
		}

		char iobuf[2048];

		while (i.good())
		{
			streamsize n=i.readsome(iobuf, sizeof(iobuf));

			if (n <= 0)
				break;
			if (b.write(iobuf, n).fail())
			{
				statusBar->clearstatus();
				statusBar->status(strerror(errno));
				statusBar->beepError();
				b.close();
				unlink(backup.c_str());
				return;
			}
		}

		if (i.fail() || b.flush().fail())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			b.close();
			unlink(backup.c_str());
			return;
		}
		b.close();
		i.close();

		// Preserve modtime of the original file.

#if     HAVE_UTIME
		{
			struct  utimbuf ub;

			ub.actime=stat_buf.st_atime;
			ub.modtime=stat_buf.st_mtime;
			utime(backup.c_str(), &ub);
		}
#else
#if     HAVE_UTIMES
		{
			struct  timeval tv;

			tv.tv_sec=stat_buf.st_mtime;
			tv.tv_usec=0;
			utimes(backup.c_str(), &tv);
		}
#endif
#endif

	}

	ofstream o(filename.c_str());

	if (!o.is_open())
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno));
		statusBar->beepError();
		return;
	}

	save(o, flowedmode);
	o.flush();
	o.close();

	if (o.bad() || o.fail())
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno));
		statusBar->beepError();
		return;
	}

	setModified(false);
	showModified();
	savedMessage();
}

//
// Substitute event loop.
//

void myServer::eventloop()
{
	Curses::keepgoing=true;
	cursesScreen->draw();

	while (Curses::keepgoing)
	{
		bool alarmCalled;

		struct timeval tv=Timer::getNextTimeout(alarmCalled);

		if (alarmCalled)
			continue;

		if (tv.tv_sec == 0 && tv.tv_usec == 0)
			tv.tv_sec= 60 * 60;

		Curses::Key k1=cursesScreen->getKey();

		if (k1.nokey())
		{
			fd_set r;

			FD_ZERO(&r);
			FD_SET(0, &r);

			if (select(1, &r, NULL, NULL, &tv) < 0)
			{
				if (errno != EINTR)
				{
					throw strerror(errno);
					break;
				}
			}
			continue;
		}
		Curses::processKey(k1);
	}
}

// Leaf-specific keys.

class LeafKeyHandler : public CursesKeyHandler {

public:
	LeafKeyHandler();
	~LeafKeyHandler();

	enum Command { NIL, ABORT, OPEN, SAVE, SAVEAS };
private:
	virtual bool processKey(const Curses::Key &key);
	bool listKeys( vector< pair<string, string> > &list);
};

LeafKeyHandler::LeafKeyHandler()
	: CursesKeyHandler(PRI_PRISCREENHANDLER)
{
}

LeafKeyHandler::~LeafKeyHandler()
{
}

bool LeafKeyHandler::processKey(const Curses::Key &key)
{
	if (statusBar->prompting())
		return false;

	if (key == '\x03')
		throw ABORT;

	if (key == key_OPENFILE)
		throw OPEN;

	if (key == key_SAVEFILE)
		throw SAVE;

	if (key == key_SAVEAS)
		throw SAVEAS;

	return false;
}

bool LeafKeyHandler::listKeys( vector< pair<string, string> > &list)
{
	list.push_back(make_pair(Gettext::keyname(_("SAVEAS_K:^A")),
				 _("save As")));

	list.push_back(make_pair(Gettext::keyname(_("OPENFILE_K:^F")),
				 _("open File")));
	list.push_back(make_pair(Gettext::keyname(_("SAVE_K:^X")),
				 _("Save")));
	return false;
}

class LeafCtrlTHandler : public CursesKeyHandler {

	bool ctrlTpressed;

public:
	LeafCtrlTHandler();
	~LeafCtrlTHandler();

	operator bool() { return ctrlTpressed; }
private:
	virtual bool processKey(const Curses::Key &key);
	bool listKeys( vector< pair<string, string> > &list);
};

LeafCtrlTHandler::LeafCtrlTHandler()
	: CursesKeyHandler(PRI_STATUSOVERRIDEHANDLER), ctrlTpressed(false)
{
}

LeafCtrlTHandler::~LeafCtrlTHandler()
{
}

bool LeafCtrlTHandler::processKey(const Curses::Key &key)
{
	if (key == key_TODIRECTORY)
	{
		ctrlTpressed=true;
		statusBar->fieldAbort();
		return true;
	}

	return false;
}

bool LeafCtrlTHandler::listKeys( vector< pair<string, string> > &list)
{
	list.push_back(make_pair(Gettext::keyname(_("TODIRECTORY_K:^T")),
				 _("Directory Listing")));
	return false;
}


class LeafCtrlFHandler : public CursesKeyHandler {

	bool ctrlFpressed;

public:
	LeafCtrlFHandler();
	~LeafCtrlFHandler();

	operator bool() { return ctrlFpressed; }
private:
	virtual bool processKey(const Curses::Key &key);
	bool listKeys( vector< pair<string, string> > &list);
};

LeafCtrlFHandler::LeafCtrlFHandler()
	: CursesKeyHandler(PRI_STATUSOVERRIDEHANDLER), ctrlFpressed(false)
{
}

LeafCtrlFHandler::~LeafCtrlFHandler()
{
}

bool LeafCtrlFHandler::processKey(const Curses::Key &key)
{
	if (key == key_TOGGLEFLOWED)
	{
		ctrlFpressed=true;
		statusBar->fieldAbort();
		return true;
	}

	return false;
}

bool LeafCtrlFHandler::listKeys( vector< pair<string, string> > &list)
{
	list.push_back(make_pair(Gettext::keyname(_("FLOWED_K:^F")),
				 _("Toggle flowed mode format")));
	return false;
}

extern string curdir();
extern void version();


Macros *Macros::getRuntimeMacros()
{
	return NULL;
}

typedef enum {
	open_ok,
	open_directory,
	open_again,
	open_abort } open_action_t;

static open_action_t openAction(std::string &filename,
				bool &flowedmode)
{
	LeafCtrlTHandler ctrl_t;
	LeafCtrlFHandler ctrl_f;

	myServer::promptInfo response=
		myServer::prompt(myServer
				 ::promptInfo(Gettext(flowedmode ?
						      _("Load (flowed text): ")
						      : _("Load (plain text): ")
						      )));

	filename=response;

	if (ctrl_t)
		return open_directory;

	if (ctrl_f)
	{
		flowedmode= !flowedmode;
		return open_again;
	}

	if (response.aborted())
		return open_abort;

	return open_ok;
}

int main(int argc, char **argv)
{
	string dictionaryName;
	int firstLineNum=1;
	int optchar;
	bool fflag=false;

	optarg=getenv("DICTIONARY");

	if (optarg && optarg)
		dictionaryName=optarg;

	while ((optchar=getopt(argc, argv, "fvd:")) != -1)
		switch (optchar) {
		case 'f':
			fflag=true;
			break;
		case 'v':
			version();
			break;
		case 'd':
			dictionaryName=optarg;
			break;
		default:
			exit(1);
		}

	init();


	{
		std::vector<unicode_char> ucbuf;
		bool errflag;

		ucbuf.push_back(8594);

		std::string s(mail::iconvert::convert(ucbuf,
						      unicode_default_chset(),
						      errflag));

		if (s.size() > 0 && !errflag)
			urarr=ucbuf[0];
		else
			urarr='>';

		ucbuf[0]=8592;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(),
					  errflag);

		if (s.size() > 0 && !errflag)
			ularr=ucbuf[0];
		else
			ularr='<';

		ucbuf[0]=8617;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(),
					  errflag);

		if (s.size() > 0 && !errflag)
			ucwrap_visible=ucbuf[0];
		else
			ucwrap_visible='<';

	}

	setflowedmode(fflag);

	CursesFileReq::currentDir=curdir();

	string filename;
	ifstream *loadfile=NULL;

	if (optind < argc && argv[optind][0] == '+')
	{
		istringstream i(argv[optind]+1);

		i >> firstLineNum;
		++optind;
	}

	if (optind < argc)
	{
		filename=argv[optind];
	}

	if (filename.size() > 0) // Open file to load.
	{
		if (filename[0] != '/')
			filename=CursesFileReq::currentDir + "/" + filename;

		filename=CursesFileReq::washfname(filename);

		if ((loadfile=new ifstream(filename.c_str())) == NULL ||
		    (!loadfile->is_open() && errno != ENOENT))
		{
			perror(filename.c_str());
			exit(1);
		}

		if (loadfile && !loadfile->is_open())
		{
			delete loadfile;
			loadfile=NULL;
		}
	}

	string errmsg="";

	currentDemoronizer=new Demoronize(unicode_default_chset(),
					  Demoronize::none);

	try
	{
		CursesScreen curses_screen;

		CursesTitleBar title_bar(&curses_screen, _("LEAF"));

		CursesStatusBar status_bar(&curses_screen);

		CursesMainScreen main_screen(&curses_screen,
					     &title_bar,
					     &status_bar);

		SpellChecker spell_checker("", "utf-8");

		spellCheckerBase= &spell_checker;

		titleBar= &title_bar;
		statusBar= &status_bar;
		cursesScreen= &curses_screen;
		mainScreen= &main_screen;

		if (dictionaryName.size() > 0)
			spellCheckerBase->setDictionary(dictionaryName);

		LeafEditMessage editMessage(mainScreen);

		editMessage.filename=filename;
		editMessage.requestFocus();
		cursesScreen->draw();

		if (loadfile)
		{
			editMessage.loadingMessage();
			editMessage.load(*loadfile, flowedmode, flowedmode);
			cursesScreen->draw();
			delete loadfile;
		}

		if (firstLineNum <= 0)
			firstLineNum=1;
		--firstLineNum;

		editMessage.setCurrentLine(firstLineNum);
		editMessage.setModified(false);
		editMessage.showModified();

		statusBar->clearstatus();
		statusBar->status(Gettext(_("Welcome to LEAF (%1%)"))
				  << unicode_default_chset());

		for (;;)
		{
			LeafKeyHandler::Command cmd=LeafKeyHandler::NIL;

			{
				LeafKeyHandler ctrl_c_handler;

				try {
					myServer::eventloop();
				} catch (LeafKeyHandler::Command c)
				{
					cmd=c;
				}
			}

			if (cmd == LeafKeyHandler::ABORT)
			{
				if (editMessage.checkModified())
					break;
			}

			if (editMessage.filename.size() > 0)
			{
				size_t n=editMessage.filename.rfind('/');

				if (n != std::string::npos)
					CursesFileReq::currentDir=
						editMessage.filename
						.substr(0, n);
			}

			if (cmd == LeafKeyHandler::OPEN)
			{
				if (!editMessage.checkModified())
					continue;

				string defaultName;

				open_action_t action;

				bool open_flowedmode=flowedmode;

				do
				{
					action=openAction(defaultName,
							  open_flowedmode);
				} while (action == open_again);

				switch (action) {
				default:
					break;
				case open_abort:
					continue;

				case open_directory:
					OpenDialog open_dialog;

					open_dialog.noMultiples();

					open_dialog.requestFocus();
					myServer::eventloop();

					vector<string> &filenameList=
						open_dialog.getFilenameList();

					if (filenameList.size() == 0)
						defaultName="";
					else
						defaultName=filenameList[0];

					mainScreen->erase();
					mainScreen->draw();
					editMessage.requestFocus();
				}
				if (defaultName.size() == 0)
					continue;

				if (defaultName[0] != '/')
				{
					defaultName=
						CursesFileReq::currentDir + "/"
						+ defaultName;
				}
				defaultName=CursesFileReq
					::washfname(defaultName);

				ifstream i(defaultName.c_str());

				if (!i.is_open())
				{
					statusBar->clearstatus();
					statusBar->status(strerror(errno));
					statusBar->beepError();
					continue;
				}

				editMessage.filename=defaultName;
				editMessage.loadingMessage();
				setflowedmode(open_flowedmode);

				editMessage.load(i, flowedmode, flowedmode);
				editMessage.setModified(false);
				editMessage.showModified();
				continue;
			}

			if (cmd == LeafKeyHandler::SAVE &&
			    editMessage.filename.size() > 0)
			{
				editMessage.saveTo();
				continue;
			}

			if (cmd == LeafKeyHandler::SAVE ||
			    cmd == LeafKeyHandler::SAVEAS)
			{
				size_t n=editMessage.filename.rfind('/');

				string defaultName=editMessage.filename;

				if (n != std::string::npos)
					defaultName=editMessage.filename
						.substr(n+1);

				bool flag;

				{
					LeafCtrlTHandler ctrl_t;

					myServer::promptInfo response=
						myServer
						::prompt(myServer
							 ::promptInfo(Gettext(_("Save: ")))
							 .initialValue(defaultName));
					flag=ctrl_t;
					defaultName=response;
					if (response.aborted() && !flag)
						continue;
				}

				if (flag)
				{
					SaveDialog save_dialog(editMessage
							       .filename);
					save_dialog.requestFocus();
					myServer::eventloop();

					defaultName=save_dialog;
					mainScreen->erase();
					mainScreen->draw();
					editMessage.requestFocus();
				}

				if (defaultName.size() == 0)
					continue;

				if (defaultName[0] != '/')
				{
					defaultName=
						CursesFileReq::currentDir + "/"
						+ defaultName;
				}
				defaultName=CursesFileReq
					::washfname(defaultName);
				editMessage.filename=defaultName;

				editMessage.saveTo();
			}
		}
	} catch (const char *txt)
	{
		errmsg=txt;
	}

	if (errmsg.size() > 0)
		std::cerr << errmsg << endl;


	return 0;
}
