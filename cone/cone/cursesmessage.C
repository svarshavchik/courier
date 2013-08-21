/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "myserver.H"
#include "myservercallback.H"
#include "myreadfolders.H"
#include "myfolder.H"
#include "myserverpromptinfo.H"
#include "myserverlogincallback.H"
#include "gettext.H"
#include "addressbook.H"
#include "cursesmessage.H"
#include "curseseditmessage.H"
#include "cursesmessagedisplay.H"
#include "curseshierarchy.H"
#include "cursesmessageflowedtext.H"
#include "cursesmessagehtmlparser.H"
#include "htmlentity.h"
#include "htmlparser.H"
#include "gpg.H"
#include "passwordlist.H"
#include "gpglib/gpglib.h"
#include "messagesize.H"
#include "rfc822/rfc822.h"
#include "rfc822/rfc822hdr.h"
#include "curses/cursesscreen.H"
#include "libmail/addmessage.H"
#include "libmail/rfc2047encode.H"
#include "libmail/rfc2047decode.H"
#include "libmail/mail.H"
#include "libmail/misc.H"
#include "libmail/logininfo.H"
#include "libmail/envelope.H"
#include "libmail/structure.H"
#include "libmail/rfcaddr.H"
#include "libmail/smtpinfo.H"

#include "colors.H"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <set>

#include <sys/types.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#include "curses/cursesstatusbar.H"

extern Gettext::Key key_DSNDELAY;
extern Gettext::Key key_DSNFAIL;
extern Gettext::Key key_DSNNEVER;
extern Gettext::Key key_DSNSUCCESS;
extern Gettext::Key key_ENCRYPT;
extern Gettext::Key key_SELECTDSN;
extern Gettext::Key key_SIGN;

extern struct CustomColor color_md_headerName;
extern struct CustomColor color_md_headerContents;
extern struct CustomColor color_md_formatWarning;

extern Demoronize *currentDemoronizer;
extern CursesStatusBar *statusBar;
extern CursesScreen *cursesScreen;

extern void editScreen(void *dummy);
extern void showCurrentMessage(void *);

bool CursesMessage::fullEnvelopeHeaders=false;

CursesMessageDisplay *CursesMessage::currentDisplay=NULL;

///////////////////////////////////////////////////////////////////

CursesMessage::SaveText::SaveText(std::ostream &oArg) : o(oArg)
{
}

CursesMessage::SaveText::~SaveText()
{
}

void CursesMessage::SaveText::operator()(std::string text)
{
	o.write(&*text.begin(), text.size());
}


///////////////////////////////////////////////////////////////////

CursesMessage::Shownpart::Shownpart() : envelope(0), structure(0),
					filtered_content(false)
{
}

CursesMessage::Shownpart::~Shownpart()
{
}

void CursesMessage::Shownpart::getflowed(bool &flowed, bool &delsp) const
{
	flowed=false;
	delsp=false;

	if (structure->type_parameters.exists("FORMAT") &&
	    rfc822hdr_namecmp(structure->type_parameters.get("FORMAT").c_str(),
			      "FLOWED") == 0)
		flowed=true;

	if (flowed && structure->type_parameters.exists("DELSP") &&
	    rfc822hdr_namecmp(structure->type_parameters.get("DELSP").c_str(),
			      "YES") == 0)
		delsp=true;
}

///////////////////////////////////////////////////////////////////

CursesMessage::reformatter::reformatter()
{
}

CursesMessage::reformatter::~reformatter()
{
}

///////////////////////////////////////////////////////////////////

CursesMessage::CursesMessage(mail::account *s,
			     void (myMessage::*completedFuncArg)())
	: myMessage(s),
	  PreviousScreen(&showCurrentMessage, this),
	  hasMimeId(false),
	  allowExternalOpen(false),
	  displayFile(NULL),
	  indexFile(NULL),
	  textReformatter(NULL),
	  reformatCompletedFunc(completedFuncArg),
	  rot13(false)
{
	reformatTimer.message=this;
	clearlinks();
}

CursesMessage::~CursesMessage()
{
	if (textReformatter)
		delete textReformatter;

	if (displayFile)
		fclose(displayFile);
	if (indexFile)
		fclose(indexFile);
}

static std::string getDisplayFilename()
{
	return myServer::getConfigDir() + "/display.tmp";
}

static std::string getIndexFilename()
{
	return myServer::getConfigDir() + "/index.tmp";
}

bool CursesMessage::isShownMimeId(std::string id)
{
	return hasMimeId && shownMimeId == id;
}

bool CursesMessage::init(std::string mimeId,
			 bool allowExternalOpenArg)
{
	if (!readAttributes())
		return false;

	tmpfiles.push_back(getDisplayFilename());
	tmpfiles.push_back(getIndexFilename());

	shown.clear();

	if (mimeId.size() == 0) // Show entire mesasge
	{
		shown.push_back(Shownpart());
		shown[0].envelope= &envelope; // The top level headers
	}

	grok(structure, mimeId);

	shownMimeId=mimeId;
	hasMimeId=true;
	allowExternalOpen=allowExternalOpenArg;
	size_t n;

	for (n=0; n<shown.size(); n++)
	{
		if (shown[n].envelope)
		{
			std::string filename=getFilename(n, "");

			tmpfiles.push_back(filename);

			shown[n].contents=filename;

			std::ofstream ofs(filename.c_str());

			SaveText save_text(ofs);

			if (!readMessage(shown[n].structure,
					 mail::readHeadersFolded,
					 save_text))
				return false;

			ofs << std::flush;

			if (ofs.fail())
			{
				statusBar->status(strerror(errno),
						  statusBar->LOGINERROR);
				statusBar->beepError();
				return false;
			}
			continue;
		}

		bool (CursesMessage::*grokker)(size_t)=
			strcasecmp(shown[n].disposition.c_str(),
				    "attachment") == 0 ? 0:
			getHandler( *shown[n].structure );

		if (grokker == 0)
			continue;

		if (!(this->*grokker)(n))
			return false;
	}
	statusBar->clearstatus();
	return true;
}

// Recursive parse the message structure to display

void CursesMessage::grok(mail::mimestruct &s, std::string mimeid)
{
	std::string disposition=s.content_disposition;

	if (mimeid.size() > 0 && s.mime_id == mimeid)
	{
		// Found the exclusive attachment to display

		mimeid="";

		disposition="inline";
		// We were explicitly told to show this one, so ignore it
		// if it's marked as an attachment.
	}

	if (s.messagerfc822() && s.getNumChildren() > 0)
	{
		if (mimeid.size() == 0)
		{
			shown.push_back(Shownpart());
			shown[shown.size()-1].structure= s.getChild(0);
			shown[shown.size()-1].envelope= &s.getEnvelope();
			shown[shown.size()-1].disposition=disposition;

			grok(*s.getChild(0), mimeid);
		}
		else
		{
			grok(*s.getChild(0), mimeid);
		}
		return;
	}

	if (s.getNumChildren() == 0)
	{
		if (mimeid.size() == 0)
		{
			shown.push_back(Shownpart());
			shown[shown.size()-1].structure= &s;
			shown[shown.size()-1].disposition=disposition;
		}
		return;
	}

	size_t n;

	if (strcasecmp(s.subtype.c_str(), "alternative") == 0
	    && mimeid.size() == 0)
	{
		size_t i;

		for (n=i=0; n<s.getNumChildren(); n++)
		{
			mail::mimestruct *c=s.getChild(n);

			if (c->multipartsigned() &&
			    c->getNumChildren() > 0)
				c=c->getChild(0);

			if (c->type == "MULTIPART" &&
			    c->subtype == "X-MIMEGPG" &&
			    tmpaccount && usingTmpaccount &&
			    c->getNumChildren() > 1)
				c=c->getChild(1);

			if (getHandler(*c) != 0)
				i=n;
		}

		grok(*s.getChild(i), mimeid);
		return;
	}

	for (n=0; n<s.getNumChildren(); n++)
		grok(*s.getChild(n), mimeid);

	// Mark end of decrypted/verified content with a MULTIPART/X-MIMEGPG

	if (s.subtype == "X-MIMEGPG" && tmpaccount && usingTmpaccount &&
	    mimeid.size() == 0)
	{
		shown.push_back(Shownpart());
		shown[shown.size()-1].structure= &s;
		shown[shown.size()-1].disposition=disposition;
	}

}

bool (CursesMessage::* CursesMessage::getHandler(mail::mimestruct &s))(size_t)
{
	mail::upper(s.type);
	mail::upper(s.subtype);

	if (s.type == "TEXT")
	{
		if (s.subtype == "PLAIN" ||
		    s.subtype == "RFC822-HEADERS" ||
		    s.subtype == "HTML" ||
		    (s.subtype == "X-GPG-OUTPUT" &&
		     tmpaccount && usingTmpaccount))
			return &CursesMessage::readTextPlain;
	}

	if (reformatCompletedFunc)
		// Not showing the message, downloading it for some other
		// purpose
		return NULL;

	if (!allowExternalOpen)
		return NULL;

	// Check for an external filter.

	std::string fn=s.type + "." + s.subtype;

	size_t n;

	while ((n=fn.find('/')) != std::string::npos)
		fn[n]='.'; // Joker

	fn= fn + ".filter";

	pid_t p=fork();

	if (p < 0)
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno));
		statusBar->beepError();
		return NULL;
	}

	if (p == 0)
	{
		// Mitigate fsckups:

		close(1);
		open("/dev/null", O_WRONLY);
		dup2(1, 2);

		std::string n=s.type + "/" + s.subtype;

		std::string execfn = myServer::getConfigDir() + "/" + fn;

		execl(execfn.c_str(), execfn.c_str(), "check",
		      n.c_str(), (char *)NULL);

		execfn = FILTERDIR "/" + fn;

		execl(execfn.c_str(), execfn.c_str(), "check",
		      n.c_str(), (char *)NULL);
		exit(1);
	}

	pid_t p2;
	int waitstat;

	while ((p2=wait(&waitstat)) != p)
	{
		if (p2 < 0 && errno != EINTR)
			break;
	}

	if (p2 == p && WIFEXITED(waitstat) && WEXITSTATUS(waitstat) == 0)
		return &CursesMessage::filterExternal;

	return NULL;
}

std::string CursesMessage::getFilename(size_t n, std::string suffix)
{
	std::string buffer;

	{
		std::ostringstream o;

		o << n;

		buffer=o.str();
	}

	return myServer::getConfigDir() + "/" + buffer + suffix + ".tmp";
}

bool CursesMessage::readTextPlain(size_t n)
{
	std::string filename=getFilename(n, "");

	shown[n].contents=filename;

	mail::mimestruct *structInfo=shown[n].structure;

	tmpfiles.push_back(filename);

	std::ofstream ofs(filename.c_str());

	SaveText save_text(ofs);

	if (!readMessageContent(*structInfo, save_text))
		return false;

	ofs << std::flush;

	if (ofs.fail())
	{
		statusBar->status(strerror(errno),
				  statusBar->LOGINERROR);
		statusBar->beepError();
		return false;
	}
	return true;
}

bool CursesMessage::filterExternal(size_t n)
{
	std::string filename=getFilename(n, "");

	shown[n].contents=filename;

	mail::mimestruct &s= *shown[n].structure;

	tmpfiles.push_back(filename);

	shown[n].contents2=getFilename(n, ".bin");
	shown[n].filtered_content=true;

	tmpfiles.push_back(shown[n].contents2);

	{
		std::ofstream o(shown[n].contents2.c_str());

		SaveText filter_text(o);

		if (!readMessageContent(s, filter_text))
			return false;

		o << std::flush;

		if (o.bad() || o.fail())
		{
			statusBar->status(strerror(errno),
					  statusBar->LOGINERROR);
			statusBar->beepError();
			return false;
		}
	}

	std::string fn=s.type + "." + s.subtype;

	size_t x;

	while ((x=fn.find('/')) != std::string::npos)
		fn[x]='.'; // Joker

	fn= fn + ".filter";

	pid_t p=fork();

	if (p < 0)
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno));
		statusBar->beepError();
		return false;
	}

	if (p == 0)
	{
		// Child's stdin is /dev/null

		FILE *fmtfile=fopen(filename.c_str(), "w");

		if (!fmtfile)
			exit(1);

		close(0);
		if (open("/dev/null", O_RDONLY) != 0)
		{
			fprintf(fmtfile, "<b>%s</b>\n", strerror(errno));
			fflush(fmtfile);
			exit(0);
		}

		close(1);
		close(2);
		if (dup(fileno(fmtfile)) != 1 || dup(1) != 2)
		{
			fprintf(fmtfile, "<b>%s</b>\n", strerror(errno));
			fclose(fmtfile);
			exit(0);
		}
		fclose(fmtfile);

		{
			std::string name;
			std::string filename;

			getDescriptionOf(shown[n].structure,
					 shown[n].envelope,
					 name,
					 filename, true);

			size_t i;

			for (i=name.size(); i; )
			{
				--i;
				if (name[i] == '&')
					name=name.substr(0, i) + "&amp;"
						+ name.substr(i+1);
			}

			while ((i=name.find('<')) != std::string::npos)
				name=name.substr(0, i) + "&lt;" +
					name.substr(i+1);

			std::string s= Gettext(_("<br><center>"
					    "* Inline attachment shown *"
					    "</center>"
					    "<center><b>%1%</b></center>"
					    "<br>\n")) << name;

			printf("%s", s.c_str());
			fflush(stdout);

		}
		std::string mimetype=s.type + "/" + s.subtype;

		std::string execfn = myServer::getConfigDir() + "/" + fn;

		execl(execfn.c_str(), execfn.c_str(), "filter",
		      mimetype.c_str(),
		      shown[n].contents2.c_str(),
		      (char *)NULL);

		execfn = FILTERDIR "/" + fn;

		execl(execfn.c_str(), execfn.c_str(), "filter",
		      mimetype.c_str(),
		      shown[n].contents2.c_str(),
		      (char *)NULL);

		printf("<b>%s</b>\n", strerror(errno));
		fflush(stdout);
		exit(0);
	}

	pid_t p2;
	int waitstat;

	while ((p2=wait(&waitstat)) != p)
	{
		if (p2 < 0 && errno != EINTR)
			break;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////
//

void CursesMessage::beginReformat(size_t w)
{
	lineCache.clear();
	displayWidth=w;
	nlines=0;

	if (displayFile)
		fclose(displayFile);
	displayFile=NULL;
	if (indexFile)
		fclose(indexFile);
	indexFile=NULL;

	std::string displayFilename=getDisplayFilename();
	std::string indexFilename=getIndexFilename();

	displayFile=fopen(displayFilename.c_str(), "w+");
	indexFile=fopen(indexFilename.c_str(), "w+");

	if (displayFile)
		fcntl(fileno(displayFile), F_SETFD, FD_CLOEXEC);
	if (indexFile)
		fcntl(fileno(indexFile), F_SETFD, FD_CLOEXEC);

	// Kick the reformatting process into gear.

	reformat_index=0;
	reformat_whence=0;
	if (reformat_file.is_open())
		reformat_file.close();
	reformatTimer.setTimer(0);
}

#define NEXTPART do { ++reformat_index; conversionErrorFlag=false; } while (0)

////////////////////////////////////////////////////////////////////
//
// Background reformatting

CursesMessage::ReformatTimer::ReformatTimer() : message(NULL)
{
}

CursesMessage::ReformatTimer::~ReformatTimer()
{
}

void CursesMessage::ReformatTimer::alarm()
{
	//
	// This alarm gets set off on each pass through eventloop()
	// Each time we exit we go back to eventloop(), and later along
	// the way the display is flushed.
	//
	// Most calls to CursesMessage::reformat() format a single row,
	// and flushing the display one row at a time is too slow.  Let's
	// be a bit uppity, check the screen height, and call reformat()
	// enough times to generate an entire page.

	size_t cnt=1;

	if (currentDisplay)
		cnt=CursesMessage::currentDisplay->getHeight();

	do
	{
		if (!message)
			return;

		size_t oldLines=message->nLines();

		if (!message->reformat())
		{
			if (currentDisplay)
				currentDisplay->folderUpdated();
			// Show navigation %-age in the title bar
			return;
		}

		size_t newLines=message->nLines();

		size_t linesDone= newLines > oldLines ? newLines - oldLines:1;

		cnt -= cnt > linesDone ? linesDone:cnt;
	} while (cnt);

	setTimer(0);
}

bool CursesMessage::reformatAddLine(std::string l,
				    LineIndex::Flags flags)
{
	LineIndex li;

	memset(&li, 0, sizeof(li));

	li.whence=reformat_whence;
	li.nbytes=l.size();
	li.flags=flags;

	reformat_whence += li.nbytes;

	if (!indexFile || !displayFile ||
	    fwrite((char *)&li, sizeof(li), 1, indexFile) != 1 ||
	    (l.size() > 0 &&
	     fwrite((char *)&*l.begin(), l.size(), 1, displayFile) != 1))

	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return false;
	}

	lineCache.insert(make_pair(nlines, make_pair(li.flags, l)));

	++nlines;

	if (CursesMessage::currentDisplay)
		CursesMessage::currentDisplay->drawLine(nlines-1);

	return true;
}

bool CursesMessage::reformat()
{
	if (reformat_index >= shown.size())
	{
		if (!indexFile || !displayFile ||
		    fflush(indexFile) < 0 ||
		    fflush(displayFile) < 0 ||
		    ferror(indexFile) ||
		    ferror(displayFile))
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno),
					  statusBar->SYSERROR);
			statusBar->beepError();
		}

		void (myMessage::*completedFunc)()=reformatCompletedFunc;

		reformatCompletedFunc=NULL;

		if (completedFunc)
			(this ->* completedFunc)();  // Reply/Forward

		return false;
	}

	Shownpart &part=shown[reformat_index];

	if (!reformat_file.is_open())
	{
		part.content_chset="iso-8859-1"; // Default, for now

		// Start a new section

		if (textReformatter) // Debris
		{
			delete textReformatter;
			textReformatter=NULL;
		}
		conversionErrorFlag=false;

		if (reformat_index > 0)
		{
			// Insert a separator from the previous section

			textAttributes ta;

			ta.reverse=1;

			if (myfolder && messagesortednum < myfolder->size())
			{
				ta.bgcolor=myfolder->getIndex(messagesortednum)
					.tag;
				if (ta.bgcolor)
					ta.reverse=0;
			}
			std::string l=ta;

			l.insert(l.end(), displayWidth, '-');

			if (!reformatAddLine(l, LineIndex::ATTRIBUTES))
				return false;

			l="";
			if (!reformatAddLine(l))
				return false;
		}

		if (part.structure && part.structure->type == "MULTIPART"
		    && part.structure->subtype == "X-MIMEGPG"
		    && !part.envelope)
		{
			// End of decrypted/verified content
			std::string chset(unicode_default_chset());

			HtmlParser fakeHtml(this, chset, chset,
					    *currentDemoronizer,
					    displayWidth);

			reformatter &f=fakeHtml;

			f.parse("<CENTER><b>&laquo; ");
			f.parse(_("End of signed/encrypted content"));

			f.parse(" &raquo;</b></CENTER><br><br>\n");
			if (!f.finish())
				return false;

			NEXTPART;
			return true;
		}

		if (part.contents.size() == 0) // Envelope, or attachment
		{

			// Figure out a meaningful description to show in place

			std::string name;
			std::string filename;

			getDescriptionOf(part.structure,
					 part.envelope,
					 name,
					 filename, true);

			if (!addHeader(_("Attachment: "), name, false))
				return false;

			NEXTPART;
			return true;
		}

		reformat_file.clear();
		reformat_file.open(part.contents.c_str(), std::ios::in);

		if (part.structure &&
		    part.structure->type_parameters.exists("CHARSET"))
		{
			part.content_chset=part.structure->type_parameters
				.get("CHARSET");
		}

		if (part.structure &&
		    part.structure->type == "TEXT" &&
		    part.structure->subtype == "X-GPG-OUTPUT" &&
		    tmpaccount && usingTmpaccount)
		{
			mail::mimestruct *s=part.structure->getParent();

			if (s && s->type == "MULTIPART" &&
			    s->subtype == "X-MIMEGPG")
			{
				std::string rc=s->type_parameters.get("XPGPSTATUS");
				int n=1;
				if (rc.size() > 0)
				{
					std::istringstream i(rc);

					i >> n;
				}

				std::string chset(unicode_default_chset());

				HtmlParser fakeHtml(this, chset, chset,
						    *currentDemoronizer,
						    displayWidth);

				reformatter &f=fakeHtml;

				f.parse("<CENTER><b>&laquo; ");
				f.parse(Gettext(_("Signed/encrypted (%1%)"
						  " content follows"))
					<<
					(n == 0 ?
					 _("succesfully verified")
					 :
					 _("<i>VERIFICATION FAILED</i>")
					 ));
				f.parse(" &raquo;</b></CENTER><br><br>\n");
				if (!f.finish())
					return false;
			}
		}

		if (part.filtered_content || // External filter HTML output
		    ( part.structure &&
		      part.structure->type == "TEXT" &&
		      part.structure->subtype == "HTML"))
		{
			HtmlParser *p=
				new HtmlParser(this, part.content_chset,
					       unicode_default_chset(),
					       *currentDemoronizer,
					       displayWidth);

			if (!p)
				outofmemory();

			textReformatter=p;


			if (!part.filtered_content)
				p->init();
		}
		else if (part.structure)
		{
			bool flowed=false, delsp=false;

			part.getflowed(flowed, delsp);

			FlowedTextParser *p=
				new FlowedTextParser(this, part.content_chset,
						     unicode_default_chset(),
						     displayWidth,
						     flowed,
						     delsp);

			if (!p)
				outofmemory();

			textReformatter=p;
		}
	}

	if (part.envelope && !fullEnvelopeHeaders)
	{
		char dateStr[200];

		dateStr[0]=0;

		if (part.envelope->date)
		{
			rfc822_mkdate_buf(part.envelope->date, dateStr);

			/* Use librfc822 func just to get the tz offset */

			size_t l=strlen(dateStr);

			if (l > 5)
			{
				char suffix[6];

				strcpy(suffix, dateStr+l-5);

				struct tm *tmptr=
					localtime(&part.envelope->date);

				if (strftime(dateStr, sizeof(dateStr),
					     "%a, %d %b %Y %H:%M:%S ", tmptr) <= 0)
					dateStr[0]=0;
				else
					strcat(dateStr, suffix);
			}
		}

		std::string dateString=std::string(       _("       Date: "))+dateStr;

		if (!reformatEnvelopeAddresses( _("       From: "),
					     part.envelope->from) ||
#if 0
		    (part.envelope->sender.size() > 0 &&
		     !reformatEnvelopeAddresses(_("     Sender: "),
					      part.envelope->sender)) ||

		    (part.envelope->replyto.size() > 0 &&
		     !reformatEnvelopeAddresses(_("   Reply-To: "),
					      part.envelope->replyto)) ||
#endif

		    (part.envelope->inreplyto.size() > 0 &&
		     !reformatHeader(           _("In-Reply-To: ")
				     + part.envelope->inreplyto, true)) ||

		    (part.envelope->to.size() > 0 &&
		     !reformatEnvelopeAddresses(_("         To: "),
					      part.envelope->to)) ||

		    (part.envelope->cc.size() > 0 &&
		     !reformatEnvelopeAddresses(_("         Cc: "),
					      part.envelope->cc)) ||

		    (part.envelope->bcc.size() > 0 &&
		     !reformatEnvelopeAddresses(_("        Bcc: "),
					      part.envelope->bcc)) ||

		    !reformatHeader(            _("    Subject: ")
						+ part.envelope->subject,
						true) ||

		    !reformatHeader(            _(" Message-ID: ")
				    + part.envelope->messageid, true) ||

		    !reformatHeader(dateString, false))
			return false;

		reformat_file.close();

		NEXTPART;
		return true;
	}

	std::string line="";

	getline(reformat_file, line);

	if (line.size() > 0 && line.end()[-1] == '\r')
		line=line.substr(0, line.size()-1);

	if (reformat_file.fail() && !reformat_file.eof())
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno),
				  statusBar->SYSERROR);
		statusBar->beepError();
		return false;
	}

	if (reformat_file.eof() && line.size() == 0)
	{
		reformat_file.close();

		bool errflag=conversionErrorFlag;

		std::string orig_charset=part.content_chset;

		NEXTPART;

		if (textReformatter)
		{
			bool rc=textReformatter->finish();

			errflag=textReformatter->conversionError();

			delete textReformatter;
			textReformatter=NULL;

			if (!rc)
				return false;
		}

		if (errflag)
		{
			size_t w=displayWidth;

			if (w > 10)
				w=w - 4;

			reformatAddLine("");

			std::string display_chset=unicode_default_chset();

			std::string msg=
				Gettext(
					rfc822hdr_namecmp(display_chset.c_str(),
							  orig_charset.c_str())
					?
					_("The previous text contained"
					  " some characters that could not be"
					  " converted from the \"%1%\""
					  " character set to your"
					  " display's"
					  " \"%2%\" character set."
					  )
					:
					_("The previous text contained"
					  " some characters that were not valid"
					  " characters in the \"%1%\" character"
					  " set.")
					) << orig_charset
					  << display_chset;

			std::vector< std::vector<unicode_char> > lines;

			{
				std::back_insert_iterator
					<std::vector< std::vector<unicode_char>
						      > > insert_iter(lines);

				std::vector<unicode_char> umsg;

				mail::iconvert::convert(msg,
							unicode_default_chset(),
							umsg);


				unicodewordwrap(umsg.begin(),
						umsg.end(),
						unicoderewrapnone(),
						insert_iter,
						w, true);
			}

			for (std::vector<std::vector<unicode_char> >::iterator
				     b=lines.begin(),
				     e=lines.end(); b != e; ++b)
			{
				std::vector<unicode_char> uc;

				{
					widecharbuf wc;

					wc.init_unicode(b->begin(), b->end());
					wc.expandtabs(0);

					uc=wc.get_unicode_fixedwidth(w, 0);
				}

				uc.insert(uc.begin(), ' ');
				uc.insert(uc.begin(), '[');
				uc.push_back(' ');
				uc.push_back(']');

				textAttributes attr;

				attr.fgcolor=color_md_formatWarning.fcolor;

				reformatAddLine((std::string)attr +

						mail::iconvert::convert(uc,
									unicode_default_chset()),
						CursesMessage::LineIndex
						::ATTRIBUTES);
			}

		}

		return true;
	}

	if (part.envelope) // Showing headers
		return reformatHeader(line, false);

	if (rot13)
	{
		std::string::iterator b=line.begin(), e=line.end();

		while (b != e)
		{
			static const char rot13tab1[]=
				"abcdefghijklmnopqrstuvwxyz"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
			static const char rot13tab2[]=
				"nopqrstuvwxyzabcdefghijklm"
				"NOPQRSTUVWXYZABCDEFGHIJKLM";

			const char *p=strchr(rot13tab1, *b);

			if (p)
				*b= rot13tab2[p-rot13tab1];
			b++;
		}
	}


	if (textReformatter) // Not just yet...
	{
		textReformatter->parse(line + "\n");
		return true;
	}

	bool err;

	if (!toMyCharset_noerror(part.content_chset, unicode_default_chset(),
			 line, err))
	{
		reformat_file.seekg(0, std::ios::end);
		return true;
	}

	if (err)
		conversionErrorFlag=true;

	return reformatLine(line);
}


void CursesMessage::getDescriptionOf(mail::mimestruct *mime,
				     mail::envelope *env,
				     std::string &name,
				     std::string &filename,
				     bool showEncoding)
{
	name="";
	filename="";

	if (mime)
	{
		name=mime->content_description;

		if (env && name.size() == 0)
			name=env->subject;

		name=mail::rfc2047::decoder().decode(name,
						     unicode_default_chset());

		if (name.size() == 0 &&
		    mime->type_parameters.exists("NAME"))
			name=mime->type_parameters.get("NAME",
						       unicode_default_chset());

		if (mime->content_disposition_parameters
		    .exists("FILENAME"))
		{
			filename=mime->content_disposition_parameters
				.get("FILENAME",
				     unicode_default_chset());
		}

		if (name.size() == 0)
			name=filename;
	}
	else if (env)
		name=env->subject;

	{
		std::vector<unicode_char> nameu;

		mail::iconvert::convert(name, unicode_default_chset(), nameu);

		widecharbuf wc;

		wc.init_unicode(nameu.begin(), nameu.end());

		wc.expandtabs(0);

		if (wc.wcwidth(0) > 40)
		{
			nameu=wc.get_unicode_fixedwidth(37, 0);
			nameu.insert(nameu.end(), 3, '.');
			name=mail::iconvert::convert(nameu,
						     unicode_default_chset());
		}
	}

	if (mime)
	{
		std::string mimeType=mime->type
			+ "/" +
			mime->subtype;

		if (showEncoding && mime->content_transfer_encoding.size() > 0)
		{
			mimeType += "; ";
			mimeType += mime->content_transfer_encoding;
		}

		if (name.size() > 0)
			name=Gettext(_("%1%; %2%"))
				<< name << mimeType;
		else
			name=mimeType;
	}

	if (mime && mime->content_size > 0)
	{
		std::string s=MessageSize(mime->content_size, true);

		if (name.size() > 0)
			name=Gettext(_("%1%; %2%"))
				<< name << s;
		else
			name=s;
	}
}

bool CursesMessage::toMyCharset(const std::string &content_chset,
				const std::string &my_chset,
				std::string &line,
				bool &errflag)
{
	if (!toMyCharset_impl(content_chset, my_chset, line, errflag))
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return false;
	}
	return true;
}

bool CursesMessage::toMyCharset_noerror(const std::string &content_chset,
					const std::string &my_chset,
					std::string &line,
					bool &errflag)
{
	if (toMyCharset_impl(content_chset, my_chset, line, errflag))
		return true;

	reformatLine(_("Error: this message is written in a character set that"
		       " cannot be displayed"));
	return false;
}

bool CursesMessage::toMyCharset_impl(const std::string &content_chset,
				     const std::string &my_chset,
				     std::string &line,
				     bool &errflag)
{
	errflag=false;
	bool tounicode_error=false;

	if (content_chset.size() == 0 ||
	    content_chset == my_chset)
		return true;

	std::vector<unicode_char> ucbuf;

	if (!mail::iconvert::convert(line, content_chset, ucbuf))
		tounicode_error=true;

	line=(*currentDemoronizer)(ucbuf, errflag);

	if (tounicode_error)
		errflag=true;
	return true;
}

bool CursesMessage::reformatLine(std::string line)
{
	std::vector<std::string> lines;

	{
		std::vector< std::vector<unicode_char> > ulines;
		std::back_insert_iterator
			<std::vector< std::vector<unicode_char>
				      > > insert_iter(ulines);

		std::vector<unicode_char> uline;

		mail::iconvert::convert(line,
					unicode_default_chset(),
					uline);


		unicodewordwrap(uline.begin(), uline.end(),
				unicoderewrapnone(),
				insert_iter, displayWidth, true);

		for (std::vector<std::vector<unicode_char> >::iterator
			     b=ulines.begin(),
			     e=ulines.end(); b != e; ++b)
		{
			lines.push_back(mail::iconvert::convert(*b,
								unicode_default_chset()));
		}

	}

	reformatAddLines(lines);
	return true;
}

void CursesMessage::reformatAddLines(std::vector<std::string> &lines)
{
	reformatFindUrls(lines, textAttributes());

	std::vector<std::string>::iterator b=lines.begin(), e=lines.end();

	while (b != e)
		reformatAddLine(*b++, LineIndex::ATTRIBUTES);
}

//
// Convert 'lines' which are plain text std::string to attributed std::strings.
// If we sniff out an http or a mailto url, mark it as such.

void CursesMessage::reformatFindUrls(std::vector<std::string> &lines,
				     textAttributes normalAttrs)
{
	std::vector<std::string>::iterator b=lines.begin(), e=lines.end();

	while (b != e)
	{
		std::string &l=*b++;

		std::string enhLine;

		std::string::iterator sb=l.begin(), se=l.end(), ss;

		ss=sb;

		while (sb != se)
		{
			if (*sb == '\x7F')
			{
				enhLine += std::string(ss, sb);
				enhLine += ' ';
				ss= ++sb;
				continue;
			}

			if (!
			    ((se - sb >= 7 &&
			      sb[0] == 'h' &&
			      sb[1] == 't' &&
			      sb[2] == 't' &&
			      sb[3] == 'p' &&
			      sb[4] == ':' &&
			      sb[5] == '/' &&
			      sb[6] == '/')
			     ||
			     (se - sb >= 8 &&
			      sb[0] == 'h' &&
			      sb[1] == 't' &&
			      sb[2] == 't' &&
			      sb[3] == 'p' &&
			      sb[4] == 's' &&
			      sb[5] == ':' &&
			      sb[6] == '/' &&
			      sb[7] == '/')
			     ||
			     (se - sb >= 7 &&
			      sb[0] == 'm' &&
			      sb[1] == 'a' &&
			      sb[2] == 'i' &&
			      sb[3] == 'l' &&
			      sb[4] == 't' &&
			      sb[5] == 'o' &&
			      sb[6] == ':')
			     ))
			{
				++sb;
				continue;
			}

			enhLine += std::string(ss, sb);
			ss=sb;

			textAttributes attrs=normalAttrs;

			while (sb != se)
			{
				if (unicode_isspace(*sb))
					break;
				if (*sb == '>' || *sb == ')')
					break;
				++sb;
			}

			attrs.underline=1;
			attrs.url=std::string(ss, sb);

			if (sb == se) // URL wrapped to next line, be smart
			{
				std::vector<std::string>::const_iterator bb=b;

				while (bb != e)
				{
					std::string::const_iterator p=
						bb->begin(), pe=bb->end();

					while (p != pe)
					{
						if (unicode_isspace(*p))
							break;
						if (*p == '>' || *p == ')')
							break;
						++p;
					}
					attrs.url +=
						std::string(bb->begin(), p);

					if (p != pe)
						break;
					++bb;
				}
			}

			switch (*--attrs.url.end()) {
			case '.':
			case ',':
				// Remove wayward period or comma.

				attrs.url=attrs.url.substr(0,
							   attrs.url.size());
				break;
			}

			enhLine += (std::string)attrs;
			enhLine += std::string(ss, sb);
			enhLine += (std::string)normalAttrs;
			ss=sb;
		}
		enhLine += std::string(ss, sb);
		l=enhLine;
	}
}

static void decode(const std::vector<mail::address> &addrs,
		   size_t width,
		   std::vector<std::string> &lines)
{
	std::vector<mail::emailAddress> addr_cpy;
	std::vector<mail::emailAddress>::iterator b, e;

	addr_cpy.insert(addr_cpy.end(), addrs.begin(), addrs.end());
	// Decode the addresses.

	lines.clear();

	std::vector<unicode_char> current_line;
	size_t current_line_width=0;

	b=addr_cpy.begin();
	e=addr_cpy.end();

	bool first=true;

	std::vector<unicode_char> comma;

	while (b != e)
	{
		std::string s= mail::address(b->getDisplayName(unicode_default_chset()),
					b->getDisplayAddr(unicode_default_chset())).toString();

		std::vector<unicode_char> s_wc;

		mail::iconvert::convert(s, unicode_default_chset(), s_wc);

		size_t addr_width;

		{
			widecharbuf wc;

			wc.init_unicode(s_wc.begin(), s_wc.end());
			wc.expandtabs(0);
			addr_width=wc.wcwidth(0);
			s_wc.clear();
			wc.tounicode(s_wc);
		}

		if (!first && current_line_width 
		    + comma.size() + addr_width > width)
		{
			current_line.insert(current_line.end(),
					    comma.begin(), comma.end());

			lines.push_back(mail::iconvert::convert(current_line,
								unicode_default_chset()));

			current_line.clear();
			current_line_width=0;
			comma.clear();
		}
		first=false;

		current_line.insert(current_line.end(),
				    comma.begin(), comma.end());
		current_line.insert(current_line.end(),
				    s_wc.begin(), s_wc.end());

		current_line_width += addr_width + comma.size();

		comma.clear();
		comma.push_back(',');
		comma.push_back(' ');

		if (b->getAddr().size() == 0)
			comma.clear();
		b++;
	}
	if (current_line.size() > 0)
		lines.push_back(mail::iconvert::convert(current_line,
							unicode_default_chset())
				);
}

bool CursesMessage::reformatEnvelopeAddresses(std::string hdrName,
					      std::vector<mail::address> &addresses)
{
	std::vector<std::string> lines;

	size_t hdr_width;

	{
		std::vector<unicode_char> l;

		mail::iconvert::convert(hdrName, unicode_default_chset(), l);

		widecharbuf wc;

		wc.init_unicode(l.begin(), l.end());
		wc.expandtabs(0);
		hdr_width=wc.wcwidth(0);

		l.clear();
		wc.tounicode(l);
		hdrName=mail::iconvert::convert(l, unicode_default_chset());
	}

	decode(addresses,
	       hdr_width+10 < displayWidth ? displayWidth-hdr_width-10:10,
	       lines);

	if (lines.empty())
		lines.push_back("");

	for (std::vector<std::string>::iterator
		     b(lines.begin()),
		     e(lines.end()); b != e; ++b)
	{
		std::string namecol, valcol;

		if (color_md_headerName.fcolor ||
		    color_md_headerContents.fcolor)
		{
			textAttributes colorAttr;
			textAttributes normalAttr;

			colorAttr.fgcolor=color_md_headerName.fcolor;
			normalAttr.fgcolor=color_md_headerContents.fcolor;

			namecol=colorAttr;
			valcol=normalAttr;
		}

		if (!reformatAddLine(namecol + hdrName + valcol + *b,
				     CursesMessage::LineIndex::ATTRIBUTES))
			return false;

		hdrName.clear();
		hdrName.insert(hdrName.end(), hdr_width, ' ');
	}

	return true;
}


bool CursesMessage::reformatHeader(std::string line,
				   bool rfc2047Encoded)
{
	size_t hdrname=line.find(':');

	if (hdrname == std::string::npos)
		hdrname=line.find(' ');

	if (hdrname != std::string::npos)
	{
		while (++hdrname < line.size() && unicode_isspace(line[hdrname]))
			;
	}
	else hdrname=0;

	return addHeader(line.substr(0, hdrname), line.substr(hdrname),
			 rfc2047Encoded);
}

bool CursesMessage::addHeader(std::string name, std::string value,
			      bool rfc2047Encoded)
{
	if (rfc2047Encoded)
		value=mail::rfc2047::decoder().decode(value,
						      unicode_default_chset());

	std::vector<unicode_char> name_u;

	mail::iconvert::convert(name, unicode_default_chset(), name_u);

	size_t name_width;

	{
		widecharbuf wc;

		wc.init_unicode(name_u.begin(), name_u.end());

		wc.expandtabs(0);
		name_width=wc.wcwidth(0);

		wc.tounicode(name_u);
	}

	std::string filler;

	filler.insert(filler.begin(), name_width, ' ');

	size_t l=name_width + 20 > displayWidth ? 20:
		displayWidth - name_width;

	std::vector<std::string> msg;

	{
		std::vector< std::vector<unicode_char> > wrapped_header;

		std::back_insert_iterator< std::vector<
			std::vector<unicode_char> > >
			insert_iter(wrapped_header);

		std::vector<unicode_char> value_u;

		mail::iconvert::convert(value, unicode_default_chset(),
					value_u);

		unicodewordwrap(value_u.begin(), value_u.end(),
				unicoderewrapnone(),
				insert_iter,
				l,
				true);

		for (std::vector< std::vector<unicode_char> >::iterator
			     b(wrapped_header.begin()),
			     e(wrapped_header.end()); b != e; ++b)
		{
			msg.push_back(mail::iconvert::convert(*b,
							      unicode_default_chset()));
		}
	}

	textAttributes colorAttr;
	textAttributes normalAttr;

	colorAttr.fgcolor=color_md_headerName.fcolor;
	normalAttr.fgcolor=color_md_headerContents.fcolor;

	if (name.substr(0, 5) == "List-")
		reformatFindUrls(msg, normalAttr);

	std::vector<std::string>::iterator b=msg.begin(), e=msg.end();

	while (b != e)
	{
		*b = (std::string)colorAttr + name
			+ (std::string)normalAttr + *b;
		b++;
		name=filler;
	}

	b=msg.begin();
	e=msg.end();

	while (b != e)
		if (!reformatAddLine(*b++,
				     CursesMessage::LineIndex::ATTRIBUTES))
			return false;
	return true;
}


size_t CursesMessage::nLines()
{
	return nlines;
}

CursesMessage::LineIndex::LineIndex()
	: whence(0), nbytes(0), flags(NORMAL)
{
}

CursesMessage::LineIndex::~LineIndex()
{
}

void CursesMessage::drawLine(size_t lineNum,
			     Curses *window,
			     size_t windowLineNum)
{
	std::vector<std::pair<textAttributes, std::string> > line;

	getLineImage(lineNum, line);

	Curses::CursesAttr attr;
	size_t left=window->getWidth();
	size_t pos=0;

	bool wasLink=false;
	bool wasFirstLink=false;
	bool wasLastLink=false;
	size_t linkCol=0;

	if (hasLink() && linkRow == windowLineNum)
	{
		wasLink=true;
		linkCol=linkPos->column;
	}

	if (hasLink())
	{
		if (linkPos == links[linkRow].begin())
		{
			size_t r=linkRow;

			while (r)
			{
				--r;

				std::map<size_t, std::list<link> >::iterator
					p=links.find(r);

				if (p != links.end() &&
				    !p->second.empty())
					break;
			}

			if (!r)
				wasFirstLink=true;
		}

		if (linkPos == --links[linkRow].end())
		{
			size_t n=window->getHeight();

			size_t r=linkRow;

			while (++r < n)
			{
				std::map<size_t, std::list<link> >::iterator
					p=links.find(r);

				if (p != links.end() &&
				    !p->second.empty())
					break;
			}

			if (r == n)
				wasLastLink=true;
		}

		if (wasFirstLink)
			wasLastLink=false;
	}

	std::list<link> &linklist=links[windowLineNum];

	linklist.clear();

	std::vector<std::pair<textAttributes, std::string> >::iterator lineb, linee;

	lineb=line.begin();
	linee=line.end();

	std::string prevurl;

	while (left && lineb != linee)
	{
		if (lineb->first.url != prevurl)
		{
			prevurl=lineb->first.url;

			if (prevurl.size() > 0)
			{
				link l;

				l.column=pos;
				l.url=lineb->first.url;
				linklist.push_back(l);
			}
		}

		std::vector<unicode_char> ul;

		mail::iconvert::convert(lineb->second,
					unicode_default_chset(), ul);

		size_t n;

		{
			widecharbuf wc;

			wc.init_unicode(ul.begin(), ul.end());

			wc.expandtabs(0);

			n=wc.wcwidth(0);

			wc.tounicode(ul);

			if (n > left)
				n=left;
		}

		Curses::CursesAttr attr;

		if (lineb->first.bold)
			attr.setBgColor(1);

		if (lineb->first.underline || lineb->first.italic)
			attr.setUnderline(true);

		if (lineb->first.reverse)
			attr.setReverse(true);

		if (lineb->first.bgcolor)
			attr.setBgColor(lineb->first.bgcolor);

		if (lineb->first.fgcolor)
			attr.setFgColor(lineb->first.fgcolor);

		window->writeText(ul, windowLineNum, pos, attr);

		pos += n;
		left -= n;
		lineb++;
	}

	if (left)
	{
		std::vector<unicode_char> l;

		l.insert(l.end(), left, ' ');
		window->writeText(l, windowLineNum, pos,
				  Curses::CursesAttr());
	}

	if (!wasLink && !linklist.empty())
	{
		// First link being added to the display.

		wasLink=true;
		linkCol=0;
		wasFirstLink=true;
	}

	if (!wasLink)
		return;

	linkRow= (size_t)-1;

	if (wasLastLink)
	{
		toLastLink();
		return;
	}

	if (!wasFirstLink)
	{
		std::map<size_t, std::list<link> >::iterator
			p=links.find(windowLineNum);

		if (p != links.end())
		{
			linkPos=p->second.begin();

			while (linkPos != p->second.end())
			{
				if (linkPos->column == linkCol)
				{
					// Found the same spot we were before

					linkRow=windowLineNum;
					return;
				}
				++linkPos;
			}
		}
	}

	// Default to the first link

	size_t n=window->getHeight();
	size_t r=0;

	while (r < n)
	{
		std::map<size_t, std::list<link> >::iterator p=links.find(r);

		if (p != links.end() && !p->second.empty())
		{
			linkRow=r;
			linkPos=p->second.begin();
			return;
		}
		++r;
	}
}

void CursesMessage::getLineImage(size_t lineNum,
				 std::vector<std::pair<textAttributes,
				 std::string> > &line)
{
	std::string s;
	LineIndex::Flags flags;

	line.clear();

	if (lineNum >= nlines)
		return;

	if (lineCache.count(lineNum) > 0)
	{
		std::pair<LineIndex::Flags, std::string> v=
			lineCache.find(lineNum)->second;

		flags=v.first;
		s=v.second;
	}
	else
	{
		LineIndex li;

		if (!indexFile || !displayFile ||
		    fseek(indexFile, sizeof(li) * lineNum, SEEK_SET) < 0 ||
		    fread((char *)&li, sizeof(li), 1, indexFile) != 1 ||
		    fseek(displayFile, li.whence, SEEK_SET) < 0 ||
		    (li.nbytes > 0 &&
		     (s.insert(s.begin(), li.nbytes, ' '),
		      fread(&*s.begin(), li.nbytes, 1, displayFile)) != 1))
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno),
				  statusBar->SYSERROR);
			statusBar->beepError();
			s="";
			flags=LineIndex::NORMAL;
		}
		else
			flags=li.flags;
	}

	if (!(flags & CursesMessage::LineIndex::ATTRIBUTES))
	{
		line.push_back(make_pair(textAttributes(), s));
	}
	else
	{
		textAttributes::getAttributedText(s, line);
	}
}

class CursesMessage::newmsgformatter : public mail::textplainparser {

	std::string my_chset;
	std::ostream &o;
	size_t quoteLevelAdjustment;

	size_t quoteLevel;
	std::vector<unicode_char> line;
	bool written;

	class writeoiter : public std::iterator<std::vector<unicode_char>,
						void, void, void, void> {

		newmsgformatter *r;

	public:
		writeoiter(newmsgformatter *rArg) : r(rArg)
		{
		}

		writeoiter &operator++() { return *this; }
		writeoiter &operator++(int) { return *this; }
		writeoiter &operator*() { return *this; }

		writeoiter &operator=(const std::vector<unicode_char> &line)
		{
			if (r->written)
			{
				r->o << " \n";
			}
			else
				r->written=true;

			for (size_t i=0; i<r->quoteLevel; ++i)
				r->o << '>';
			if (r->quoteLevel || (!line.empty() &&
					      (line.front() == ' ' ||
					       line.front() == '>')))
				r->o << ' ';

			r->o << mail::iconvert::convert(line, r->my_chset);
			return *this;
		}
	};
public:

	newmsgformatter(const std::string &my_chsetArg,
			size_t quoteLevelAdjustmentArg,
			std::ostream &oArg)
		: my_chset(my_chsetArg), o(oArg),
		  quoteLevelAdjustment(quoteLevelAdjustmentArg)
	{
	}

	void line_begin(size_t quoteLevelArg)
	{
		quoteLevel=quoteLevelArg+quoteLevelAdjustment;
		line.clear();
	}

	void line_contents(const unicode_char *uc, size_t cnt)
	{
		line.insert(line.end(), uc, uc+cnt);
	}

	void line_end()
	{
		size_t n=quoteLevel;

		if (n) ++n;

		if (n + 20 < LINEW)
			n=LINEW-n;
		else
			n=20;

		written=false;

		std::vector<unicode_char>::iterator b(line.begin()),
			e(line.end());

		while (b != e && e[-1] == ' ')
			--e;

		writeoiter oiter(this);

		unicodewordwrap(b, e,
				unicoderewrapnone(),
				oiter,
				LINEW,
				false);

		if (!written)
			oiter=std::vector<unicode_char>();

		o << "\n";
	}

	void read(std::istream &ifs)
	{
		std::string line;

		do
		{
			getline(ifs, line);
			if (ifs.fail() && !ifs.eof())
				break;

			if (line.size() == 0 && ifs.eof())
				break;

			operator<<(line);
			operator<<("\n");
		} while (!ifs.eof());
	}
};

//////////////////////////////////////////////////////////////////////
//
// Manufacture a reply.

void CursesMessage::reply()
{
	bool cc_reply=false; // Post-n-mail put addresses into cc, not to.

	if (reformatting())
		return;

	size_t n=0;

	if (n >= shown.size() ||
	    shown[n].envelope == NULL || shown[n].contents.size() == 0)
		return;

	clearAttFiles();
	std::ifstream ifs(shown[n].contents.c_str());

	std::string line;

	std::string newsgroups;
	std::string followupto;

	std::vector<mail::address> references;

	while (!getline(ifs, line).fail())
	{
		if (line.size() == 0)
			break;

		std::string::iterator colon=std::find(line.begin(),
						 line.end(), ':');

		if (colon != line.end())
			++colon;

		std::string name=std::string(line.begin(), colon);

		std::transform(name.begin(), name.end(),
			       name.begin(), std::ptr_fun(::tolower));

		std::string::iterator value_start=
			std::find_if(colon, line.end(),
				     std::not1(std::ptr_fun(::unicode_isspace))),
			value_end=value_start, p;

		for (p=value_start; p != line.end(); ++p)
			if (!unicode_isspace(*p))
				value_end=p+1;


		std::string value=std::string(value_start, value_end);
		
		if (name == "newsgroups:")
		{
			newsgroups=value;
		}

		if (name == "followup-to:")
		{
			followupto=value;
		}


		if (name == "references:")
		{
			size_t dummy;

			mail::address::fromString(value,
						  references, dummy);
		}
	}
	ifs.close();

	if (followupto.size() > 0)
	{
		myServer::promptInfo response=
			myServer
			::prompt(myServer::
				 promptInfo(Gettext(_("Follow up to: %1%? (Y/N) "))
					    << followupto)
				 .yesno());

		if (response.abortflag)
			return;

		if ( (std::string)response != "Y")
			followupto="";
	}

	if (followupto.size() > 0)
	{
		if (strcasecmp(followupto.c_str(), "poster") == 0)
			newsgroups="";
		else
			newsgroups=followupto;
	}

	if (shown[n].envelope->messageid.size() > 0)
	{
		size_t dummy;

		mail::address::fromString(shown[n].envelope->messageid,
					  references, dummy);
	}

	std::set<std::string> seenAddresses;
	// Keep track of addresses processed, to eliminate dupes

	std::vector<mail::address> to, cc;

	std::vector<mail::address> &sender=shown[n].envelope->replyto.size() > 0
		? shown[n].envelope->replyto:shown[n].envelope->from;

	std::vector<mail::address>::iterator seenb, seene;

	size_t j;

	std::vector<mail::address> me;

	std::vector<mail::address> mailingLists;

	// Create the To: header

	for (seenb=sender.begin(), seene=sender.end(); seenb != seene; seenb++)
	{
		std::string a= seenb->getCanonAddress();

		if (seenAddresses.count(a) > 0)
			continue;

		// Compile a list of mailing list addresses seen on the way

		for (j=0; j<myServer::myListAddresses.size(); j++)
			if (*seenb ==
			    mail::address("",myServer::myListAddresses[j]))
			{
				mailingLists.push_back( *seenb );
				break;
			}

		// Do not include myself in the recipient list

		if (myServer::isMyAddress(*seenb))
		{
			if (me.size() == 0)
				me.push_back(*seenb);
		}

		if (j < myServer::myListAddresses.size())
			continue;

		seenAddresses.insert(a);
		to.push_back( *seenb );

	}

	// Similarly create the Cc header from all the addresses found in the
	// old To and Cc headers, except the ones that are already in the new
	// To: header.

	std::vector<mail::address> tocc;

	tocc=shown[n].envelope->to;

	tocc.insert(tocc.end(), shown[n].envelope->cc.begin(),
		    shown[n].envelope->cc.end());

	for (seenb=tocc.begin(), seene=tocc.end(); seenb != seene; seenb++)
	{
		std::string a= seenb->getCanonAddress();

		if (seenAddresses.count(a) > 0)
			continue;

		for (j=0; j<myServer::myListAddresses.size(); j++)
			if (*seenb ==
			    mail::address("",myServer::myListAddresses[j]))
			{
				mailingLists.push_back( *seenb );
				break;
			}

		for (j=0; j<myServer::myAddresses.size(); j++)
			if (*seenb ==
			    mail::address("",myServer::myAddresses[j]))
			{
				if (me.size() == 0)
					me.push_back(*seenb);

				break;
			}

		if (j < myServer::myAddresses.size())
			continue;

		seenAddresses.insert(a);
		cc.push_back( *seenb );
	}

	// If the new To: or Cc: are both empty, add my address to the To:

	if (to.size() == 0 && cc.size() == 0)
		to.insert(to.end(), me.begin(), me.end());

	char *p=rfc822_coresubj_keepblobs(shown[n].envelope->subject.c_str());

	std::string subject="";

	if (p)
		try {
			subject=p;
			free(p);
		} catch (...) {
			free(p);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

	std::string senderName="";

	std::string content_chset="iso-8859-1";

	std::string my_chset=unicode_default_chset();

	if (shown[n].envelope->from.size() > 0)
	{
		mail::emailAddress addr(shown[n].envelope->from[0]);

		senderName=addr.getDisplayName(unicode_default_chset());

		if (senderName.size() == 0)
			senderName=addr.getDisplayAddr(unicode_default_chset());
	}

	//
	// Read everything we need from the message object now, before
	// any prompts (which may result in the message object going away
	// if the event loop detects a closed server connection.

	ifs.clear();

	std::string tmpfile=myServer::getConfigDir() + "/message.tmp";
	std::string msgfile=myServer::getConfigDir() + "/message.txt";

	std::string r_server=mail::rfc2047::encode(myfolder->getServer()->url,
					      "iso-8859-1");

	std::string r_folder=mail::rfc2047::encode(myfolder->getFolder()->getPath(),
					    "iso-8859-1");

	std::string r_uid=mail::rfc2047::encode(myfolder->getIndex(messagesortednum)
					 .uid,
					 "iso-8859-1");

	std::string content_type="TEXT/PLAIN";

	++n;

	// Skip stuff we get after decrypting.

	while (n < shown.size() &&
	       tmpaccount && usingTmpaccount &&
	       shown[n].structure &&
	       shown[n].structure->type == "TEXT" &&
	       shown[n].structure->subtype == "X-GPG-OUTPUT")
		++n;

	bool flowed=false;
	bool delsp=false;

	if (n < shown.size() && shown[n].contents.size() > 0)
	{
		Shownpart &part=shown[n];

		content_chset=part.content_chset;

		ifs.open(part.contents.c_str());

		if (part.structure)
		{
			content_type=part.structure->type + "/" +
				part.structure->subtype;

			part.getflowed(flowed, delsp);
		}
	}

	bool post=myfolder->getServer()->server &&
		myfolder->getServer()->server
		->getCapability(LIBMAIL_SERVERTYPE) == "nntp";

	if (post)
	{
		std::string responseStr;

		switch (myServer::postAndMail) {
		case myServer::POSTANDMAIL_YES:
			responseStr="Y";
			break;
		case myServer::POSTANDMAIL_NO:
			responseStr="N";
			break;
		default:
			{
				myServer::promptInfo response=myServer
					::promptInfo(_("Cc original author?"
						       " (Y/N) "))
					.yesno();

				response=myServer::prompt(response);
				if (response.abortflag)
					return;

				responseStr=response;
			}
			break;
		}

		cc_reply=true;
		if ( responseStr != "Y" )
		{
			mailingLists.clear();
			cc.clear();
			to.clear();
		}
	}

	if (mailingLists.size() > 0)
	{
		myServer::promptInfo response=
			myServer
			::prompt(myServer::
				 promptInfo(_("Reply to mailing list? (Y/N) "))
				 .yesno());

		if (response.abortflag)
			return;

		if ((std::string)response == "Y")
		{
			to=mailingLists;
			cc.clear();
		}
	}

	if (to.size() > 0 && cc.size() > 0)
	{
		myServer::promptInfo response=
			myServer
			::prompt(myServer::
				 promptInfo(_("Reply to all? (Y/N) "))
				 .yesno());

		if (response.abortflag)
			return;

		if ((std::string)response != "Y")
			cc.clear();
	}

	if (to.size() == 0)
	{
		to=cc;
		cc.clear();
	}

	std::string from, replyto, fcc, customheaders;

	if (!myMessage::getDefaultHeaders(myfolder->getFolder(),
					  myfolder->getServer(),
					  from, replyto, fcc,
					  customheaders))
	{
		return;
	}

	std::ofstream otmpfile(tmpfile.c_str());

	if (post)
		otmpfile << "Newsgroups: " << newsgroups << std::endl;

	otmpfile << "From: " << from << std::endl;

	if (customheaders.size() > 0)
		otmpfile << customheaders << std::endl;

	if (replyto.size() > 0)
		otmpfile << "Reply-To: " << replyto << std::endl;

	otmpfile << (to.size() > 0 ?
		     mail::address::toString(cc_reply ? "Cc:":"To: ", to)
		     : (cc_reply ? "Cc:":"To: ")) << std::endl;

	if (cc.size() > 0)
		otmpfile << mail::address::toString("Cc: ", cc) << std::endl;

	if (fcc.size() > 0)
		otmpfile << "X-Fcc: "
			 << (std::string)mail::rfc2047::encode(fcc, "UTF-8")
			 << std::endl;

	otmpfile << "X-Server: " << r_server << std::endl
		 << "X-Folder: " << r_folder << std::endl
		 << "X-UID: " << r_uid << std::endl
		 << "Subject: Re: " << subject << std::endl
		 << "References: ";

	size_t i;

	for (i=0; i<references.size(); i++)
	{
		if (i > 0) otmpfile << ' ';
		otmpfile << "<" << references[i].getAddr() << ">";
	}

	otmpfile << std::endl
		 << "Mime-Version: 1.0" << std::endl
		 << "Content-Type: text/plain; format=flowed; delsp=yes; charset="
		 << unicode_default_chset() << std::endl
		 << "Content-Transfer-Encoding: 8bit" << std::endl << std::endl;

	otmpfile << (std::string)(Gettext(_("%1% writes:")) << senderName)
		 << std::endl << std::endl;

	reformatter *reformatterPtr=NULL;

	if (content_type == "TEXT/HTML")
	{
		HtmlParser *p=new HtmlParser(otmpfile,
					     true,
					     content_chset,
					     my_chset,
					     *currentDemoronizer,
					     LINEW);

		if (!p)
			outofmemory();

		reformatterPtr=p;
	}

	try {
		if (reformatterPtr)
		{
			while (ifs.is_open())
			{
				getline(ifs, line);
				if (ifs.fail() && !ifs.eof())
					break;

				if (line.size() == 0 && ifs.eof())
					break;

				if (line.size() > 0 && *--line.end() == '\r')
				{
					line=line.substr(0, line.size()-1);
				}

				reformatterPtr->parse(line + "\n");

				if (ifs.eof())
					break;
			}
		}
		else
		{
			newmsgformatter fmtreply(my_chset, 1, otmpfile);

			if (fmtreply.begin(content_chset, flowed, delsp))
			{
				if (ifs.is_open())
					fmtreply.read(ifs);
				fmtreply.end();
			}
		}

		if (reformatterPtr)
		{
			bool rc=reformatterPtr->finish();
			delete reformatterPtr;

			if (!rc)
				return;
		}
	} catch (...) {
		if (reformatterPtr)
			delete reformatterPtr;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	otmpfile << std::flush;

	if (otmpfile.fail() ||
	    (otmpfile.close(), rename(tmpfile.c_str(), msgfile.c_str())) < 0)
	{
		statusBar->status(strerror(errno),
				  statusBar->LOGINERROR);
		statusBar->beepError();
		return;
	}

	Curses::keepgoing=false;
	myServer::nextScreen= &editScreen;
	myServer::nextScreenArg=NULL;
}

void CursesMessage::forward()
{
	if (reformatting())
		return;

	size_t n=0;

	if (shown[n].envelope == NULL || shown[n].contents.size() == 0)
		return;

	MONITOR(CursesMessage);

	myServer::promptInfo response=
		myServer
		::prompt(myServer::
			 promptInfo(_("Fwd entire message as an attachment? (Y/N) "))
			 .yesno());

	if (response.abortflag || DESTROYED())
		return;

	char *p=rfc822_coresubj_keepblobs(shown[n].envelope->subject.c_str());

	std::string subject="";

	if (p)
		try {
			subject=p;
			free(p);
		} catch (...) {
			free(p);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

	bool isMimeAttachment=(std::string)response == "Y";

	if (isMimeAttachment)
	{
		std::string tmpfile, attfile;

		createAttFilename(tmpfile, attfile);

		std::ofstream otmpfile(tmpfile.c_str());

		otmpfile << "Content-Type: message/rfc822" << std::endl
			 << "Content-Description: "
			 << shown[n].envelope->subject << std::endl
			 << std::endl;

		statusBar->clearstatus();
		statusBar->status(_("Downloading message..."),
				  statusBar->INPROGRESS);

		SaveText save_attachment(otmpfile);

		if (shown[n].structure == NULL ||
		    shown[n].structure->getParent() == NULL)
		{
			if (!readMessage(save_attachment))
				return;
		}
		else
		{
			if (!readMessage(shown[n].structure,
					 mail::readBoth,
					 save_attachment))
				return;
		}

		otmpfile << std::flush;

		if (otmpfile.fail() ||
		    (otmpfile.close(), rename(tmpfile.c_str(),
					      attfile.c_str())) < 0)
		{
			statusBar->status(strerror(errno),
					  statusBar->LOGINERROR);
			statusBar->beepError();
			return;
		}
		statusBar->clearstatus();
	}

	std::string tmpfile=myServer::getConfigDir() + "/message.tmp";
	std::string msgfile=myServer::getConfigDir() + "/message.txt";

	std::string from, replyto, fcc, customheaders;

	if (!myMessage::getDefaultHeaders(myfolder->getFolder(),
					  myfolder->getServer(),
					  from, replyto, fcc,
					  customheaders))
	{
		return;
	}

	std::ofstream otmpfile(tmpfile.c_str());

	if (fcc.size() > 0)
		otmpfile << "X-Fcc: "
			 << (std::string)mail::rfc2047::encode(fcc, "UTF-8")
			 << std::endl;

	std::string r_server=mail::rfc2047::encode(myfolder->getServer()->url,
					    "iso-8859-1");

	std::string r_folder=mail::rfc2047::encode(myfolder->getFolder()->getPath(),
					    "iso-8859-1");

	otmpfile << "X-Server: " << r_server << std::endl
		 << "X-Folder: " << r_folder << std::endl;

	if (customheaders.size() > 0)
		otmpfile << customheaders << std::endl;

	otmpfile << "From: " << from << std::endl;

	if (replyto.size() > 0)
		otmpfile << "Reply-To: " << replyto << std::endl;

	otmpfile << "Subject: " << subject << " (fwd)" << std::endl
		 << "Mime-Version: 1.0" << std::endl
		 << "Content-Type: text/plain; charset=\""
		 << unicode_default_chset() << "\"" << std::endl
		 << "Content-Transfer-Encoding: 8bit" << std::endl << std::endl << std::flush;

	bool good=true;

	if (!isMimeAttachment)
	{
		otmpfile << std::endl
			 << _("---------- Forwarded message ----------")
			 << std::endl;

		{
			std::ifstream i(shown[n].contents.c_str());

			std::string line;

			for (;;)
			{
				if (getline(i, line).fail() && !i.eof())
					break;

				if (line.size() == 0 && i.eof())
					break;

				if (line.size() > 0 && *--line.end() == '\r')
				{
					line=line.substr(0, line.size()-1);
				}

				otmpfile << line << std::endl;
				if (i.eof())
					break;
			}
		}

		otmpfile << std::endl;

		++n;

		// Skip stuff we get after decrypting.

		while (n < shown.size() &&
		       tmpaccount && usingTmpaccount &&
		       shown[n].structure &&
		       shown[n].structure->type == "TEXT" &&
		       shown[n].structure->subtype == "X-GPG-OUTPUT")
			++n;

		if (n < shown.size() && shown[n].contents.size())
		{
			std::ifstream i(shown[n].contents.c_str());

			std::string content_chset
				=shown[n].content_chset;

			std::string content_type="TEXT/PLAIN";
			bool flowed=false;
			bool delsp=false;

			if (shown[n].structure)
			{
				content_type=shown[n].structure->type + "/" +
					shown[n].structure->subtype;
				shown[n].getflowed(flowed, delsp);
			}
			std::string line;

			good=false;

			reformatter *reformatterPtr=NULL;

			if (content_type == "TEXT/HTML")
			{
				HtmlParser *p=
					new HtmlParser(otmpfile,
						       false,
						       content_chset,
						       unicode_default_chset(),
						       *currentDemoronizer,
						       LINEW);

				if (!p)
					outofmemory();

				reformatterPtr=p;
			}

			try {
				if (reformatterPtr)
				{
					for (;;)
					{
						if (getline(i, line).fail()
						    && !i.eof())
							break;

						if (line.size() == 0 && i.eof())
						{
							good=true;
							break;
						}

						if (line.size() > 0 &&
						    *--line.end() == '\r')
						{
							line=line.substr(0,
									 line
									 .size()
									 -1);
						}

						reformatterPtr
							->parse(line + "\n");

						if (i.eof())
						{
							good=true;
							break;
						}
					}
				}
				else
				{
					std::string my_chset=
						unicode_default_chset();

					newmsgformatter
						fmtforward(my_chset, 0,
							   otmpfile);

					if (fmtforward.begin(content_chset,
							     flowed, delsp))
					{
						fmtforward.read(i);
						fmtforward.end();
						good=true;
					}
				}

				if (reformatterPtr)
				{
					bool rc=reformatterPtr->finish();
					delete reformatterPtr;

					if (!rc)
						good=false;
				}
			} catch (...) {
				if (reformatterPtr)
					delete reformatterPtr;
			}
			otmpfile << std::flush;
		}
	}

	if (!good || otmpfile.fail() ||
	    (otmpfile.close(), rename(tmpfile.c_str(), msgfile.c_str())) < 0)
	{
		statusBar->status(strerror(errno),
				  statusBar->LOGINERROR);
		statusBar->beepError();
		return;
	}
	Curses::keepgoing=false;
	myServer::nextScreen= &editScreen;
	myServer::nextScreenArg=NULL;
}

bool CursesMessage::getBounceTo(mail::smtpInfo &smtpInfo)
{
	myServer::promptInfo response=
		myServer::prompt(myServer::promptInfo(_("Blind forward to: ")));

	if (response.abortflag)
		return false;

	std::vector<mail::emailAddress> addrList, addrListRet;

	{
		std::vector<mail::address> addrBuf;

		size_t errIndex;

		if (!mail::address::fromString((std::string)response, addrBuf, errIndex)
		    || errIndex < std::string::npos)
		{
			statusBar->clearstatus();
			statusBar->status(_("Error - invalid mail list format"));
			statusBar->beepError();
			return false;
		}

		addrList.reserve(addrBuf.size());

		std::vector<mail::address>::iterator b, e;

		for (b=addrBuf.begin(), e=addrBuf.end(); b != e; ++b)
		{
			mail::emailAddress addr;

			addr.setDisplayName(b->getName(),
					    unicode_default_chset());
			addr.setDisplayAddr(b->getAddr(),
					    unicode_default_chset());
			addrList.push_back(addr);
		}
	}

	if (!AddressBook::searchAll(addrList, addrListRet))
		return false;

	if (addrListRet.size() == 0)
		return false;

	std::vector<mail::emailAddress>::iterator b=addrListRet.begin(),
		e=addrListRet.end();

	while (b != e)
	{
		smtpInfo.recipients.push_back( b->getAddr() );
		++b;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
// Create a libmail object for sending mail.
//
// smtpInfo - sender/recipient information
//
// smtpServer - initialized to non-NULL if sending mail requires opening
// a new account object (usually the case unless SMAP is used).
//
// *folderptr - not NULL if a copy of the message should be Fcced to this
// folder.  If the returned folder object can take care of that automatically,
// *folderptr will be reset to NULL.  Otherwise an FCC copy of the message
// will have to be manually saved there.
//
// disconnectStub - the account callback object if smtpServer needs to be
// created
//
// Returns: a new folder object, whose addMessage() method should be used
// to send mail, or NULL if an error occured.

mail::folder *CursesMessage::getSendFolder(mail::smtpInfo &smtpInfo,
					   mail::account * &smtpServer,
					   mail::folder * *folderptr,
					   mail::callback::disconnect
					   &disconnectStub)
{
	smtpServer=NULL;

	mail::folder *folder;

	if (!myServer::useIMAPforSending)
	{
		folder=NULL;
	}
	else
	{
		std::string errmsg;

		myServer *s= *myServer::server_list.begin();

		if (!CursesHierarchy::autologin(s))
			return NULL;

		folder=s->server->getSendFolder(smtpInfo,
						folderptr ? *folderptr:NULL,
						errmsg);

		if (folder && folderptr)
			*folderptr=NULL; /* Simultaneous save/send */
	}

	if (!folder)
	{
		std::string pwd="";

		std::string url=myServer::smtpServerURL;

		mail::loginInfo SMTPServerLoginInfo;
		mail::account::openInfo loginInfo;

		myServerLoginCallback loginPrompt;
		myServer::Callback callback(CursesStatusBar::LOGINERROR);

		if (url.size() > 0 &&
		    mail::loginUrlDecode(url, SMTPServerLoginInfo)
		    && SMTPServerLoginInfo.uid.size() > 0)
		{

			loginPrompt.myCallback= &callback;
			loginInfo.loginCallbackObj= &loginPrompt;
			
			if (myServer::smtpServerPassword.size() > 0)
				pwd=myServer::smtpServerPassword;
			else if (!PasswordList::passwordList
				 .check(myServer::smtpServerURL, pwd))
			{
				pwd="";
			}
		}

		myServer::smtpServerPassword="";
		// If fail to log in, ask next time

		if (url.size() == 0)
			url="sendmail://localhost";


		loginInfo.url=url;
		loginInfo.pwd=pwd;
		myServer::find_cert_by_id(loginInfo.certificates,
					  myServer::smtpServerCertificate);

		smtpServer=mail::account::open( loginInfo,
						callback,
						disconnectStub);

		try {
			std::string errmsg;

			for (;;)
			{
				if (callback.interrupted)
				{
					loginPrompt
						.promptPassword(Gettext(_("mail server on %1%"))
								<< SMTPServerLoginInfo.server,
								pwd);
				}

				statusBar->clearstatus();
				statusBar->status(Gettext(_("Connecting to %1%..."))
						  << SMTPServerLoginInfo.server);

				bool rc=myServer::eventloop(callback);

				if (callback.interrupted)
					continue;

				if (!rc)
				{
					delete smtpServer;
					smtpServer=NULL;
					PasswordList::passwordList
						.remove(myServer::smtpServerURL);
					return NULL;
				}
				break;
			}

			folder=smtpServer->getSendFolder(smtpInfo,
							 NULL, errmsg);

			if (!folder)
			{
				statusBar->status(errmsg,
						  statusBar->SERVERERROR);
				statusBar->beepError();
				delete smtpServer;
				smtpServer=NULL;
				return NULL;
			}
		} catch (...) {
			delete smtpServer;
			smtpServer=NULL;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		myServer::smtpServerPassword=pwd;
		// Don't ask next time.

		if (myServer::smtpServerURL.size() > 0)
			PasswordList::passwordList.save(myServer::smtpServerURL,
							pwd);
	}

	return folder;
}

//
// Initialze smtpInfo with mail sending parameters (DSN, etc...)
//

CursesMessage::EncryptionInfo::EncryptionInfo()
{
}

CursesMessage::EncryptionInfo::~EncryptionInfo()
{
}

bool CursesMessage::getSendInfo(std::string promptStr,
				std::string promptStr2,
				mail::smtpInfo &smtpInfo,
				CursesMessage::EncryptionInfo *encryptInfo)
{
	// Prompt for basic encryption and DSN settings, below.

	if (!getSendInfo2(promptStr, promptStr2, smtpInfo,
			  encryptInfo))
		return false; // Cancelled, for some reason.

	// If encryption or signing was selected, prompt for additional
	// stuff.

	if (!encryptInfo)
		return true;

	return setEncryptionOptions(encryptInfo);
}

bool CursesMessage::setEncryptionOptions(CursesMessage::EncryptionInfo *encryptInfo)
{
	std::string gpgurl="gpg:" + encryptInfo->signKey;

	if (encryptInfo->signKey.size() > 0 &&
	    !PasswordList::passwordList.check(gpgurl,
					      encryptInfo->passphrase))
	{
		myServer::promptInfo passphrase=
			myServer
			::prompt( myServer
				  ::promptInfo(_("Passphrase (if required): "))
				  .password());

		if (passphrase.abortflag)
			return false;

		encryptInfo->passphrase=passphrase;

		PasswordList::passwordList.save(gpgurl,
						encryptInfo->passphrase);
	}

	if (encryptInfo->isUsing())
	{
		std::string::iterator b=GPG::gpg.extraEncryptSignOptions.begin();

		while (b != GPG::gpg.extraEncryptSignOptions.end())
		{
			if (unicode_isspace((int)(unsigned char)*b))
			{
				b++;
				continue;
			}

			std::string::iterator s=b;

			while (b != GPG::gpg.extraEncryptSignOptions.end())
			{
				if (unicode_isspace((int)(unsigned char)*b))
					break;
				b++;
			}

			encryptInfo->otherArgs.push_back(std::string(s, b));
		}
	}
	return true;
}

bool CursesMessage::getSendInfo2(std::string promptStr,
				 std::string promptStr2,
				 mail::smtpInfo &smtpInfo,
				 CursesMessage::EncryptionInfo *encryptInfo)
{
	bool dsnNever=false, dsnSuccess=false, dsnDelay=false,
		dsnFail=false;

	std::vector<std::string> saveEncryptionKeys;

	initEncryptInfo(encryptInfo, saveEncryptionKeys);

	myServer::promptInfo promptDSN("");
	for (;;)
	{
		if (encryptInfo &&
		    (encryptInfo->signKey.size() > 0 ||
		     encryptInfo->encryptionKeys.size() > 0))
		{
			std::string s= encryptInfo->signKey.size() > 0
				? encryptInfo->encryptionKeys.size() > 0
				? _("Sign, encrypt, then %1%")
				: _("Sign, then %1%")
				: _("Encrypt, then %1%");

			promptDSN=myServer::promptInfo(Gettext(s)
						       << promptStr2);
		}
		else
			promptDSN=myServer::promptInfo(promptStr);

		promptDSN=promptDSN.yesno()
			.option(key_SELECTDSN,
				Gettext::keyname(_("SELECTDSN_K:D")),
				_("Delivery notifications"));

		if (encryptInfo)
			promptDSN=promptDSN
				.option(key_SIGN,
					Gettext::keyname(_("SIGN_K:S")),
					_("Sign"))
				.option(key_ENCRYPT,
					Gettext::keyname(_("ENCRYPT_K:E")),
					_("Encrypt"));


		promptDSN=myServer::prompt(promptDSN);

		if (promptDSN.abortflag || (std::string)promptDSN == "N")
			return false;

		if (encryptInfo)
		{
			std::vector<unicode_char> ka;

			mail::iconvert::convert((std::string)promptDSN,
						unicode_default_chset(),
						ka);

			if (ka.size() == 0)
				ka.push_back(' ');

			if (key_SIGN == ka[0])
			{
				initEncryptSign(encryptInfo);
				if (!Curses::keepgoing)
					return false;
				continue;
			}

			if (key_ENCRYPT == ka[0])
			{
				initEncryptEncrypt(encryptInfo, saveEncryptionKeys);
				if (!Curses::keepgoing)
					return false;
				continue;
			}

		}
		break;
	}

	{
		std::vector<unicode_char> ka;

		mail::iconvert::convert((std::string)promptDSN,
					unicode_default_chset(),
					ka);

		if (ka.size() == 0)
			ka.push_back(' ');

		if (key_SELECTDSN == ka[0])
		{
			dsnSuccess=dsnDelay=dsnFail=true;
		}
	}

	bool firstTime=true;

	while ((std::string)promptDSN != "Y")
	{
		std::vector<unicode_char> ka;

		mail::iconvert::convert((std::string)promptDSN,
					unicode_default_chset(),
					ka);

		if (ka.size() == 0)
			ka.push_back(' ');

		if (firstTime)
			ka[0]=' '; // Skip the following checks
		firstTime=false;

		if (key_DSNNEVER == ka[0])
		{
			dsnNever= !dsnNever;
			dsnFail=dsnSuccess=dsnDelay=false;
		}

		if (key_DSNSUCCESS == ka[0])
		{
			dsnSuccess= !dsnSuccess;
			dsnNever=false;
		}

		if (key_DSNDELAY == ka[0])
		{
			dsnDelay= !dsnDelay;
			dsnNever=false;
		}

		if (key_DSNFAIL == ka[0])
		{
			dsnFail= !dsnFail;
			dsnNever=false;
		}

		std::string notifyString="";
			
		if (dsnSuccess)
		{
			if (notifyString.size() > 0)
				notifyString += ", ";

			notifyString += _("success");
		}
		if (dsnDelay)
		{
			if (notifyString.size() > 0)
				notifyString += ", ";
			notifyString += _("delay");
		}
		if (dsnFail)
		{
			if (notifyString.size() > 0)
				notifyString += ", ";
			notifyString += _("fail");
		}
		if (dsnNever)
		{
			if (notifyString.size() > 0)
				notifyString += ", ";
			notifyString += _("never");
		}
		promptDSN=myServer::
			prompt(myServer::
			       promptInfo(Gettext(_("Send message (NOTIFY=%1%)? (Y) ")) <<
					  notifyString).yesno()
			       .option(key_DSNNEVER,
				       Gettext::
				       keyname(_("DSNNEVER_K:N")),
				       _("Never"))
			       .option(key_DSNSUCCESS,
				       Gettext::
				       keyname(_("DSNSUCCESS_K:S")),
				       _("Success"))
			       .option(key_DSNDELAY,
				       Gettext::
				       keyname(_("DSNDELAY_K:D")),
				       _("Delay"))
			       .option(key_DSNFAIL,
				       Gettext::
				       keyname(_("DSNFAIL_K:F")),
				       _("Fail")));

		if (promptDSN.abortflag || (std::string)promptDSN == "N")
			return false;
	}

	std::string dsnopt="";

	if (dsnSuccess)
	{
		if (dsnopt.size() > 0)
			dsnopt += ",";

		dsnopt += "success";
	}
	if (dsnDelay)
	{
		if (dsnopt.size() > 0)
			dsnopt += ",";
		dsnopt += "delay";
	}
	if (dsnFail)
	{
		if (dsnopt.size() > 0)
			dsnopt += ",";
		dsnopt += "fail";
	}
	if (dsnNever)
	{
		if (dsnopt.size() > 0)
			dsnopt += ",";
		dsnopt += "never";
	}
	if (dsnopt.size() != 0)
		smtpInfo.options.insert(make_pair(std::string("DSN"), dsnopt));
	return true;
}


void CursesMessage::initEncryptInfo(CursesMessage::EncryptionInfo *&encryptInfo,
				    std::vector<std::string> &saveEncryptionKeys)
{
	if (encryptInfo && ! GPG::gpg_installed())
	{
		encryptInfo->signKey="";
		encryptInfo->encryptionKeys.clear();
		encryptInfo=NULL;
	}

	if (encryptInfo && encryptInfo->signKey.size() > 0 &&
	    GPG::gpg.get_secret_key(encryptInfo->signKey)
	    == GPG::gpg.secret_keys.end())
	{
		// For some reason this fingerprint does not exist.  Nuke it.

		encryptInfo->signKey="";
	}

	// Save the default encryption keys

	if (encryptInfo)
	{
		saveEncryptionKeys=encryptInfo->encryptionKeys;
		encryptInfo->encryptionKeys.clear();
	}
}

void CursesMessage::initEncryptSign(CursesMessage::EncryptionInfo *encryptInfo)
{
	encryptInfo->signKey=GPG::gpg
		.select_private_key(encryptInfo->signKey,
				    _("SELECT PRIVATE KEY"),
				    _("Select key to sign this message with, and press ENTER"),
				    _("Sign with this key"),
				    _("Cancel, do not sign message"));
}

void CursesMessage::initEncryptEncrypt(CursesMessage::EncryptionInfo *encryptInfo,
				       std::vector<std::string> &saveEncryptionKeys)
{
	if (encryptInfo->encryptionKeys.size() == 0)
		encryptInfo->encryptionKeys=saveEncryptionKeys;

	GPG::gpg.select_public_keys(encryptInfo->encryptionKeys,
				    _("SELECT PUBLIC KEYS"),
				    _("Select recipients' encryption keys, then press ENTER"),
				    _("Encrypt to selected keys"),
				    _("Cancel, do not encrypt"));
}

////////////////////////////////////////////////////////////////////////////
//
// Helper class saves message to decrypt into a temp file

class CursesMessage::DecryptSaveText : public ReadText {
public:
	FILE *fp;
	FILE *passphrase_fp;

	mail::addMessage *add;

	DecryptSaveText();
	~DecryptSaveText();
	void operator()(std::string text); // Callback from ReadText

	// Interface to libmail_gpg:

 	static int input_func(char *buf, size_t cnt, void *vp);
	static void output_func(const char *output, size_t nbytes,
				void *output_arg);
	static void err_func(const char *errmsg, void *errmsg_arg);

	std::string errmsg;

 	int input_func(char *buf, size_t cnt);
	void output_func(const char *output, size_t nbytes);
	

};

int CursesMessage::DecryptSaveText::input_func(char *buf, size_t cnt,
					       void *vp)
{
	return ((CursesMessage::DecryptSaveText *)vp)->input_func(buf, cnt);
}

void CursesMessage::DecryptSaveText::output_func(const char *output,
						 size_t nbytes,
						 void *output_arg)
{
	return ((CursesMessage::DecryptSaveText *)output_arg)
		->output_func(output, nbytes);
}

void CursesMessage::DecryptSaveText::err_func(const char *errmsg,
					      void *errmsg_arg)
{
	((CursesMessage::DecryptSaveText *)errmsg_arg)->errmsg=errmsg;
}


int CursesMessage::DecryptSaveText::input_func(char *buf, size_t cnt)
{
	return libmail_gpg_inputfunc_readfp(buf, cnt, fp);
}

void CursesMessage::DecryptSaveText::output_func(const char *output,
						 size_t nbytes)
{
	if (add)
		add->saveMessageContents(std::string(output, output+nbytes));
}

CursesMessage::DecryptSaveText::DecryptSaveText() : fp(tmpfile()),
						    passphrase_fp(NULL),
						    add(NULL)
{
}

CursesMessage::DecryptSaveText::~DecryptSaveText()
{
	if (fp)
		fclose(fp);

	if (passphrase_fp)
		fclose(passphrase_fp);

	if (add)
		add->fail("Cancelled.");
}

void CursesMessage::DecryptSaveText::operator()(std::string s)
{
	if (fp && s.size() > 0)
		if (fwrite(&s[0], s.size(), 1, fp) != 1)
			; // Ignore gcc warning.
}

bool CursesMessage::decrypt(std::string passphrase, std::vector<std::string> &opts,
			    bool &decryptFailed)
{
	decryptFailed=false;

	// Prepare the temp account.

	if (tmpaccount)
	{
		delete tmpaccount;
		tmpaccount=NULL;
	}
	usingTmpaccount=false;

	// Must be declared AFTER add_callback, because DecryptSaveText's
	// callback gets rid of addMessage, which refers to add_callback
	myServer::Callback add_callback;
	DecryptSaveText tempSaveText;

	{
		mail::folder *f;

		{
			myServer::Callback openCallback;

			mail::account::openInfo openInfo;

			openInfo.url="tmp:";

			tmpaccount=mail::account::open(openInfo,
						       openCallback, *this);

			if (!myServer::eventloop(openCallback) ||
			    !tmpaccount)
			{
				return false;
			}

			myReadFolders tlFolders;
			myServer::Callback readTopLevelCallback;

			tmpaccount->readTopLevelFolders(tlFolders,
							readTopLevelCallback);

			if (!myServer::eventloop(readTopLevelCallback) ||
			    tlFolders.size() <= 0)
				return false;

			f=tmpaccount->folderFromString(tlFolders[0]);

			if (!f)
			{
				statusBar->clearstatus();
				statusBar->status(strerror(errno));
				statusBar->beepError();
				return false;
			}
		}

		try {
			myServer::Callback openCallback;

			f->open(openCallback, NULL, *this);

			if (!myServer::eventloop(openCallback))
			{
				delete f;
				return false;
			}

			if ((tempSaveText.add=f->addMessage(add_callback))
			    == NULL)
			{
				statusBar->clearstatus();
				statusBar->status(strerror(errno));
				statusBar->beepError();
				delete f;
				return false;
			}

			delete f;
		} catch (...) {
			delete f;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}

	statusBar->clearstatus();
	statusBar->status(_("Reading message..."));

	if (shownMimeId.size() > 0) // message/rfc822 is shown.
	{
		mail::mimestruct *s=decrypt_find(structure, shownMimeId);

		if (!s)
			return false;

		if (!readMessage(s, mail::readContents, tempSaveText))
			return false;
	}
	else if (!readMessage(tempSaveText)) // entire msg is shown
		return false;

	if (!tempSaveText.fp || fflush(tempSaveText.fp) < 0 ||
	    fseek(tempSaveText.fp, 0L, SEEK_SET) < 0)
	{
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return false;
	}

	statusBar->clearstatus();
	statusBar->status(_("Decrypting/verifying message..."));
	statusBar->flush();

	{
		std::string passphrase_fd;

		struct libmail_gpg_info gi;

		memset(&gi, 0, sizeof(gi));

		if (passphrase.size() > 0)
		{
			std::ostringstream o;

			if ((tempSaveText.passphrase_fp=tmpfile()) == NULL ||
			    fprintf(tempSaveText.passphrase_fp, "%s",
				    passphrase.c_str()) < 0 ||
			    fflush(tempSaveText.passphrase_fp) < 0 ||
			    fseek(tempSaveText.passphrase_fp, 0L, SEEK_SET)<0)
			{
				statusBar->status(strerror(errno),
						  statusBar->SYSERROR);
				statusBar->beepError();
				return false;
			}

			o << fileno(tempSaveText.passphrase_fp);

			passphrase_fd=o.str();

			gi.passphrase_fd=passphrase_fd.c_str();
		}

		gi.input_func= &tempSaveText.input_func;
		gi.input_func_arg= &tempSaveText;

		gi.output_func= &tempSaveText.output_func;
		gi.output_func_arg= &tempSaveText;

		gi.errhandler_func= &tempSaveText.err_func;
		gi.errhandler_arg= &tempSaveText;

		opts.push_back(std::string("--no-tty"));

		std::vector< std::vector<char> > argv_ptr;
		std::vector< char *> argv_cp;

		GPG::create_argv(opts, argv_ptr, argv_cp);

		gi.argv=&argv_cp[0];
		gi.argc=argv_cp.size();

		if (libmail_gpg_decode(LIBMAIL_GPG_UNENCRYPT|
				       LIBMAIL_GPG_CHECKSIGN, &gi))
		{
			if (tempSaveText.errmsg.size() == 0)
				tempSaveText.errmsg=strerror(errno);

			statusBar->clearstatus();
			statusBar->status(tempSaveText.errmsg);
			statusBar->beepError();
			return false;
		}

		tempSaveText.add->go();
		tempSaveText.add=NULL;

		if (gi.errstatus & LIBMAIL_ERR_DECRYPT)
			decryptFailed=true;
	}

	if (!myServer::eventloop(add_callback))
		return false;

	{
		myServer::Callback checkNew;

		tmpaccount->checkNewMail(checkNew);
		if (!myServer::eventloop(add_callback) ||
		    tmpaccount->getFolderIndexSize() == 0)
			return false;
	}

	// Now, reparse.

	usingTmpaccount=true;
	haveAttributes=false;

	mail::envelope saveEnvelope=envelope;
	mail::mimestruct saveStructure=structure;

	if (!init("", false)) // Something went wrong, try again
	{
		envelope=saveEnvelope;
		structure=saveStructure;
		usingTmpaccount=false;
		return false;
	}

	return true;
}

mail::mimestruct *CursesMessage::decrypt_find(mail::mimestruct &m, std::string s)
{
	if (s.size() == 0 || s == m.mime_id)
		return &m;

	size_t n=m.getNumChildren();
	size_t i=0;

	mail::mimestruct *p;

	for (i=0; i<n; i++)
		if ((p=decrypt_find(*m.getChild(i), s)) != NULL)
			return p;

	return NULL;
}

//

bool CursesMessage::nextLink()
{
	if (!hasLink())
		return false;

	if (++linkPos != links[linkRow].end())
		return true; // Next link, same row
	--linkPos;

	std::map<size_t, std::list<link> >::iterator p, q=links.end();

	for (p=links.begin(); p != links.end(); ++p)
	{
		if (p->second.empty())
			continue;

		if (p->first <= linkRow)
			continue;

		if (q == links.end() || p->first < q->first)
			q=p;
	}

	if (q == links.end())
		return false;

	linkRow=q->first;
	linkPos=q->second.begin();
	return true;
}

bool CursesMessage::prevLink()
{
	if (!hasLink())
		return false;

	if (linkPos != links[linkRow].begin())
	{
		--linkPos;
		return true; // Prev link, same row
	}

	std::map<size_t, std::list<link> >::iterator p, q=links.end();

	for (p=links.begin(); p != links.end(); ++p)
	{
		if (p->second.empty())
			continue;

		if (p->first >= linkRow)
			continue;

		if (q == links.end() || p->first > q->first)
			q=p;
	}

	if (q == links.end())
		return false;

	linkRow=q->first;
	linkPos=q->second.end();
	--linkPos;
	return true;
}

void CursesMessage::toLastLink()
{
	linkRow= (size_t)-1;

	std::map<size_t, std::list<link> >::iterator p, q=links.end();

	for (p=links.begin(); p != links.end(); ++p)
	{
		if (p->second.empty())
			continue;

		if (q == links.end() || p->first > q->first)
			q=p;
	}

	if (q != links.end())
	{
		linkRow=q->first;
		linkPos=q->second.end();
		--linkPos;
	}
}
