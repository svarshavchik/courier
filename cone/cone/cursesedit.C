/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "gettext.H"
#include "cursesedit.H"
#include "curseshierarchy.H"
#include "addressbook.H"
#include "myserver.H"
#include "myreadfolders.H"
#include "myservercallback.H"
#include "myserverpromptinfo.H"
#include "myfolder.H"
#include "nntpcommand.H"
#include "opensubfolders.H"
#include "specialfolder.H"
#include "previousscreen.H"
#include "opendialog.H"
#include "specialfolder.H"
#include "gpg.H"
#include "passwordlist.H"
#include "colors.H"
#include "disconnectcallbackstub.H"
#include "libmail/rfc2047decode.H"
#include "libmail/rfc2047encode.H"
#include "gpglib/gpglib.h"

#include <sstream>
#include <fstream>
#include <iomanip>
#include <errno.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "unicode/unicode.h"
#include "rfc822/rfc822.h"
#include "rfc822/encode.h"
#include "rfc2045/rfc2045.h"
#include "rfc2045/rfc2045charset.h"
#include "libmail/mail.H"
#include "libmail/misc.H"
#include "libmail/structure.H"
#include "libmail/mimetypes.H"
#include "libmail/addmessage.H"
#include "libmail/smtpinfo.H"
#include "libmail/logininfo.H"
#include "libmail/headers.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursesstatusbar.H"
#include "curses/cursestitlebar.H"

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

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

extern Gettext::Key key_ENCRYPT;
extern Gettext::Key key_PRIVATEKEY;
extern Gettext::Key key_SIGN;
extern Gettext::Key key_FULLHEADERS;
extern Gettext::Key key_ATTACHFILE;
extern Gettext::Key key_ATTACHKEY;
extern Gettext::Key key_POSTPONE;
extern Gettext::Key key_SEND;
extern Gettext::Key key_ABORT;


extern CursesMainScreen *mainScreen;
extern CursesStatusBar *statusBar;
extern CursesTitleBar *titleBar;
extern struct CustomColor color_wm_headerName;

extern void hierarchyScreen(void *);

#define AUTOSAVEINTERVAL (CursesEditMessage::autosaveInterval < 60 ? 60:\
	CursesEditMessage::autosaveInterval > 60 * 60 ? 60 * 60:\
	CursesEditMessage::autosaveInterval)

//////////////////////////////////////////////////////////////////////////////

CursesEdit::CursesEditMessageClass
::CursesEditMessageClass(CursesContainer *parent)
	: CursesEditMessage(parent),
	  myEdit(NULL)
{
}

CursesEdit::CursesEditMessageClass::~CursesEditMessageClass()
{
}

void CursesEdit::CursesEditMessageClass::modified()
{
	if (myEdit)
		myEdit->restartAutosaveTimer();
}

std::string CursesEdit::CursesEditMessageClass::getConfigDir()
{
	return myServer::getConfigDir();
}

void CursesEdit::CursesEditMessageClass::extedited()
{
	mail::account::resume();

	if (myEdit->from)
		myEdit->from->requestFocus();
	else if (myEdit->to)
		myEdit->to->requestFocus();
	myEdit->message.requestFocus();
}

void CursesEdit::CursesEditMessageClass::macroDefined()
{
	myServer::saveconfig();
}

//////////////////////////////////////////////////////////////////////////////

CursesEdit::dummyFolderCallback::dummyFolderCallback()
{
}

CursesEdit::dummyFolderCallback::~dummyFolderCallback()
{
}

void CursesEdit::dummyFolderCallback::newMessages()
{
}

void CursesEdit::dummyFolderCallback::messagesRemoved(std::vector<std::pair<size_t,
						      size_t> > &dummy)
{
}

void CursesEdit::dummyFolderCallback::messageChanged(size_t n)
{
}

//////////////////////////////////////////////////////////////////////////////
//
// Subclass CursesAddressList and implement address book lookup

CursesEdit::AddressList::AddressList(CursesEdit *parentArg)
	: CursesAddressList(parentArg),
	  parent(parentArg)
{
}

CursesEdit::AddressList::~AddressList()
{
}

void CursesEdit::AddressList::resized()
{
	CursesAddressList::resized();
	parent->addressResized();
}

bool CursesEdit::AddressList::addressLookup(std::vector<mail::emailAddress> &in,
					    std::vector<mail::emailAddress> &out)
{
	// Need to cancel autosaving when doing the address book lookup.

	parent->autosaveTimer.cancelTimer();
	bool rc=AddressBook::searchAll(in, out);
	parent->restartAutosaveTimer();

	return rc;
}


//////////////////////////////////////////////////////////////////////////////
//
// DEL on an attachment button will remove the attachment.

CursesEdit::AttachmentButton::AttachmentButton(CursesEdit *parentArg,
					       std::string filenameArg,
					       std::string name,
					       std::string encodingArg)
	: CursesButton(parentArg, name),
	  parent(parentArg),
	  filename(filenameArg),
	  encoding(encodingArg)
{
}

CursesEdit::AttachmentButton::~AttachmentButton()
{
}

bool CursesEdit::AttachmentButton::processKeyInFocus(const Curses::Key &key)
{
	if (key == key.DEL)
	{
		parent->delAttachment(filename);
		return true;
	}

	return CursesButton::processKeyInFocus(key);
}

//////////////////////////////////////////////////////////////////////////////

CursesEdit::CustomHeader::CustomHeader(std::string textArg)
	: label(NULL), field(NULL), isHidden(false), name(textArg)
{
}

CursesEdit::CustomHeader::~CustomHeader()
{
	if (label)
		delete label;
	if (field)
		delete field;
}

//////////////////////////////////////////////////////////////////////////////
//
// Here we go

CursesEdit::CursesEdit(CursesContainer *parent)
	: CursesContainer(parent), CursesKeyHandler(PRI_PRISCREENHANDLER),
	  goodexit(false),
	  to(NULL), cc(NULL), newsgroups(NULL), followupto(NULL),
	  from(NULL), bcc(NULL), replyto(NULL),
	  subject(this),
	  sentFolderLabel(NULL),
	  sentFolderChoice(NULL),

	  attLabel(this, _("Attachments: ")),
	  newAttachment(this),

	  toLabel(NULL), ccLabel(NULL), newsgroupsLabel(NULL),
	  followuptoLabel(NULL),
	  subjectLabel(this, _("Subject: ")),
	  fromLabel(NULL), bccLabel(NULL), replytoLabel(NULL),
	  message(this)
{
	newAttachment=this;
	newAttachment= &CursesEdit::addAttachment;

	autosaveTimer=this;
	autosaveTimer= &CursesEdit::autosave;
	message.myEdit=this;

	Curses::CursesAttr attr;

	attr.setFgColor(color_wm_headerName.fcolor);
	subjectLabel.setAttribute(attr);
	attLabel.setAttribute(attr);
}

CursesEdit::~CursesEdit()
{
	autosaveTimer.cancelTimer(); // Shut it off.

	if (!goodexit)
		autosave_int(); // Something happened

	std::vector<AttachmentButton *>::iterator b, e;

	b=attachmentList.begin();
	e=attachmentList.end();

	while (b != e)
	{
		AttachmentButton *a= *b;

		*b++=NULL;

		if (a)
			delete a;
	}


	std::vector<CustomHeader *>::iterator cb, ce;

	cb=customHeaders.begin();
	ce=customHeaders.end();

	while (cb != ce)
	{
		CustomHeader *ch= *cb;

		*cb++=NULL;

		if (ch)
			delete ch;
	}

	if (to)
		delete to;
	if (cc)
		delete cc;
	if (newsgroups)
		delete newsgroups;
	if (followupto)
		delete followupto;

	if (from)
		delete from;
	if (bcc)
		delete bcc;
	if (replyto)
		delete replyto;
	if (sentFolderChoice)
		delete sentFolderChoice;
	if (toLabel)
		delete toLabel;
	if (ccLabel)
		delete ccLabel;
	if (newsgroupsLabel)
		delete newsgroupsLabel;
	if (followuptoLabel)
		delete followuptoLabel;

	if (fromLabel)
		delete fromLabel;
	if (bccLabel)
		delete bccLabel;
	if (replytoLabel)
		delete replytoLabel;
	if (sentFolderLabel)
		delete sentFolderLabel;
}

int CursesEdit::getWidth() const
{
	return getScreenWidth();
}

void CursesEdit::requestFocus()
{
	if (to)
		to->requestFocus();
	else if (cc)
		cc->requestFocus();
	else if (newsgroups)
		newsgroups->requestFocus();
	else
		subject.requestFocus();
}

bool CursesEdit::isFocusable()
{
	if (to)
		return to->isFocusable();

	if (cc)
		return cc->isFocusable();

	if (newsgroups)
		return newsgroups->isFocusable();

	return subject.isFocusable();
}

//
// When this object is constructed, we expect to see the existing message
// in config/message.txt

void CursesEdit::init()
{
	newsgroupMode=false;

	std::string msg=myServer::getConfigDir() + "/message.txt";

	{
		std::vector<mail::emailAddress> emptyList;

		if (to)
			to->setAddresses(emptyList);
		else
			toV=emptyList;

		if (cc)
			cc->setAddresses(emptyList);
		else
			ccV=emptyList;

		if (newsgroups)
			newsgroups->setText("");
		else
			newsgroupsV="";

		if (followupto)
			followupto->setText("");
		else
			followuptoV="";
	}
	SpecialFolder &sentFolder=SpecialFolder::folders.find(SENT)->second;

	if (sentFolder.folderList.size() == 0)
	{
		// Initialize the default sent folder.

		myServer *sentServer;

		mail::folder *f=sentFolder.getFolder(sentServer);

		if (f)
		{
			delete f;
			myServer::saveconfig();
		}
		else
		{
			Curses::keepgoing=false;
			myServer::nextScreen= &hierarchyScreen;
			myServer::nextScreenArg=NULL;
			PreviousScreen::previousScreen();
		}

	}

	std::ifstream i(msg.c_str());

	std::string subjectV;

	fromV.clear();
	bccV.clear();
	replytoV.clear();
	toV.clear();
	ccV.clear();
	newsgroupsV="";
	followuptoV="";
	xserver="";
	xfolder="";
	xuid="";

	defaultSentChoice=0;

	// Initialize vanity headers.

	std::string h=myServer::customHeaders;

	while (h.size() > 0)
	{
		std::string hh;

		size_t n=h.find(',');

		if (n == std::string::npos)
		{
			hh=h;
			h="";
		}
		else
		{
			hh=h.substr(0, n);
			h=h.substr(n+1);
		}

		bool isHidden=false;

		if (strncmp(hh.c_str(), "*", 1) == 0)
		{
			isHidden=true;
			hh=hh.substr(1);
		}

		CustomHeader *ch=new CustomHeader(hh);

		if (!ch)
			outofmemory();

		try {
			customHeaders.push_back(ch);
		} catch (...) {
			delete ch;
			throw;
		}
		ch->isHidden=isHidden;

		if (!ch->isHidden)
		{
			ch->label=new CursesLabel(this, hh + " ");
			ch->field=new CursesField(this);

			if (!ch->label || !ch->field)
				outofmemory();

			ch->label->setAlignment(Curses::RIGHT);
		}
	}

	const std::string my_chset=unicode_default_chset();

	Curses::CursesAttr attr;

	attr.setFgColor(color_wm_headerName.fcolor);

	if (i.is_open())
	{
		std::string cur_hdr="";

		std::string line;

		// Read headers, and initialize fields.

		for (;;)
		{
			if (getline(i, line).fail() && !i.eof())
				break;

			if (line.size() > 0 &&
			    unicode_isspace((unsigned char)line[0]))
			{
				std::string::iterator p=line.begin();

				while (p != line.end() &&
				       unicode_isspace((unsigned char)*p))
					++p;

				cur_hdr += std::string(p-1, line.end());
				// Wrapped header.
				continue;
			}

			std::string hdr=cur_hdr;
			cur_hdr=line; // line is now the start of next hdr.

			size_t p=hdr.find(':');

			std::string hdrname="";

			if (p != std::string::npos)
			{
				hdrname=hdr.substr(0, p++);

				while (p < hdr.size()
				       && unicode_isspace((unsigned char)
						  hdr[p]))
					++p;

				hdr= p < hdr.size() ? hdr.substr(p):"";
			}

			if (strcasecmp(hdrname.c_str(), "from") == 0)
			{
				size_t dummy;

				mail::address::fromString(hdr, fromV, dummy);
				continue;
			}

			if (strcasecmp(hdrname.c_str(), "to") == 0)
			{
				size_t dummy;

				mail::address::fromString(hdr, toV, dummy);
				continue;
			}

			if (strcasecmp(hdrname.c_str(), "cc") == 0)
			{
				size_t dummy;

				mail::address::fromString(hdr, ccV, dummy);
				continue;
			}

			if (strcasecmp(hdrname.c_str(), "bcc") == 0)
			{
				size_t dummy;

				mail::address::fromString(hdr, bccV, dummy);
				continue;
			}

			if (strcasecmp(hdrname.c_str(), "reply-to") == 0)
			{
				size_t dummy;

				mail::address::fromString(hdr, replytoV,
							  dummy);
				continue;
			}

			if (strcasecmp(hdrname.c_str(), "subject") == 0)
			{
				subjectV=hdr;
				continue;
			}

			if (strcasecmp(hdrname.c_str(), "newsgroups") == 0)
			{
				newsgroupsV=hdr;
				continue;
			}

			if (strcasecmp(hdrname.c_str(), "followup-to") == 0)
			{
				followuptoV=hdr;
				continue;
			}

			if (strcasecmp(hdrname.c_str(), "x-server") == 0)
			{
				// Orignal message's server.

				xserver=mail::rfc2047::decoder
					::decodeSimple(hdr);

				// Is it a news server?
				myServer *s=myServer::getServerByUrl(xserver);

				if (s && CursesHierarchy::autologin(s) &&
				    s->server
				    ->getCapability(LIBMAIL_SERVERTYPE)
				    == "nntp")
					newsgroupMode=true;
			}

			if (strcasecmp(hdrname.c_str(), "x-fcc") == 0)
			{
				std::string str=mail::rfc2047::decoder
					::decodeSimple(hdr);
				size_t i;

				for (i=0; i<sentFolder.folderList.size(); i++)
					if (sentFolder.folderList[i]
						    .nameUTF8 == str)
					{
						defaultSentChoice=i+1;
					}

				// Default fcc folder.

			}

			if (strcasecmp(hdrname.c_str(), "x-folder") == 0)
			{
				xfolder=mail::rfc2047::decoder
					::decodeSimple(hdr);
			}

			if (strcasecmp(hdrname.c_str(), "x-uid") == 0)
			{
				xuid=mail::rfc2047::decoder
					::decodeSimple(hdr);
			}

			// Check if it's a custom header.

			std::vector<CustomHeader *>::iterator cb, ce;

			cb=customHeaders.begin();
			ce=customHeaders.end();

			hdrname += ":";

			while (cb != ce)
			{
				CustomHeader *ch= *cb++;

				if (strcasecmp(ch->name.c_str(),
					       hdrname.c_str()) == 0)
				{
					std::string str=
						mail::rfc2047::
						decoder()
						.decode(hdr,
							unicode_default_chset()
							);
					ch->hiddenValue=str;
					if (ch->field)
						ch->field->setText(str);
					break;
				}
			}

			if (line.size() == 0)
				break; // End of headers
		}

		mail::rfc2047::decoder decode;

		newsgroupsV=decode.decode(newsgroupsV, my_chset);
		followuptoV=decode.decode(followuptoV, my_chset);
		subjectV=decode.decode(subjectV, my_chset);

		// Newsgroups mode creates a Newsgroups: header.
		// Mail mode creates a To: and Cc: header.

		if (newsgroupMode)
		{
			newsgroups=new CursesField(this);
			followupto=new CursesField(this);

			if (!newsgroups || !followupto)
				outofmemory();

			newsgroups->setText(newsgroupsV);
			followupto->setText(followuptoV);
			newsgroupsLabel=new CursesLabel(this,
							_("Newsgroups: "));
			followuptoLabel=new CursesLabel(this,
							_("Followup-To: "));

			if (!newsgroupsLabel || !followuptoLabel)
				outofmemory();

			newsgroupsLabel->setAttribute(attr);
			followuptoLabel->setAttribute(attr);
		}
		else
		{
			if ((to=new AddressList(this)) == NULL)
				outofmemory();
			to->setAddresses(toV);

			if ((cc=new AddressList(this)) == NULL)
				outofmemory();
			cc->setAddresses(ccV);

			toLabel=new CursesLabel(this, _("To: "));
			ccLabel=new CursesLabel(this, _("Cc: "));

			toLabel->setAttribute(attr);
			ccLabel->setAttribute(attr);
		}

		message.load(i, true, true); // Load the rest of the message.
	}

	subject.setText(subjectV);

	// Initialize list of existing attachments.

	std::vector<std::string> filenameList;

	myMessage::readAttFiles(filenameList);

	std::vector<std::string>::iterator b=filenameList.begin(), e=filenameList.end();

	while (b != e)
	{
		std::string f= *b++;

		struct stat stat_buf;

		if (stat(f.c_str(), &stat_buf))
			continue;

		std::ifstream i(f.c_str());

		if (!i.is_open())
			continue;

		mail::mimestruct s;

		// Well, make this quick and dirty by leveraging rfc2045

		struct rfc2045 *rfcp=rfc2045_alloc();

		if (!rfcp)
			outofmemory();

		try {
			static const char mv[]="Mime-Version: 1.0\n";

			rfc2045_parse(rfcp, mv, sizeof(mv)-1); // rfc2045 food

			std::string line;

			while (!getline(i, line).eof())
			{
				line += "\n";
				rfc2045_parse(rfcp, line.c_str(),
					      line.size());

				if (!rfcp->workinheader)
					break;
			}

			rfc2045_parse(rfcp, "\n\n\n", 3);

			const char *content_type;
			const char *content_transfer_encoding;
			const char *charset;

			rfc2045_mimeinfo(rfcp, &content_type,
					 &content_transfer_encoding,
					 &charset);

			s.content_description=
				rfcp->content_description ?
				rfcp->content_description:"";

			s.content_transfer_encoding=
				content_transfer_encoding ?
				content_transfer_encoding:"8BIT";

			s.type=content_type;

			size_t sn=s.type.find('/');

			if (sn != std::string::npos)
			{
				s.subtype=s.type.substr(sn+1);

				s.type=s.type.substr(0, sn);
			}

			char *disposition_name=NULL;
			char *disposition_filename;

			if (rfc2231_udecodeDisposition(rfcp, "name", NULL,
						       &disposition_name) < 0
			    ||
			    rfc2231_udecodeDisposition(rfcp, "filename", NULL,
						       &disposition_filename)
			    < 0)
			{
				if (disposition_name)
					free(disposition_name);
				LIBMAIL_THROW(strerror(errno));
			}

			s.content_disposition=rfcp->content_disposition
				? rfcp->content_disposition
				: "ATTACHMENT";
			s.content_size=stat_buf.st_size;

			try {
				if (*disposition_filename)
					s.content_disposition_parameters
						.set("FILENAME",
						     disposition_filename,
						     unicode_default_chset(),
						     "");
				free(disposition_name);
				free(disposition_filename);
			} catch (...) {
				free(disposition_name);
				free(disposition_filename);
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}

			rfc2045_free(rfcp);
		} catch (...) {
			rfc2045_free(rfcp);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		std::string name;
		std::string filename;

		CursesMessage::getDescriptionOf(&s, NULL, name, filename,
						false);

		AttachmentButton *b=
			new AttachmentButton(this, f, name,
					     s.content_transfer_encoding);

		if (!b)
			outofmemory();

		try {
			attachmentList.push_back(b);
		} catch (...) {
			delete b;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}

	addressResized();
	requestFocus();
	autosaveTimer.setTimer(AUTOSAVEINTERVAL);
}

// When this window is resized, try to keep a sane layout.
void CursesEdit::addressResized()
{
	// Find the widest header label.

	int tw=toLabel ? toLabel->getWidth():0;
	int cw=ccLabel ? ccLabel->getWidth():0;
	int nw=newsgroupsLabel ? newsgroupsLabel->getWidth():0;
	int nfw=followuptoLabel ? followuptoLabel->getWidth():0;
	int sw=subjectLabel.getWidth();
	int aw=attLabel.getWidth();
	int fw=fromLabel ? fromLabel->getWidth():0;
	int bw=bccLabel ? bccLabel->getWidth():0;
	int rw=replytoLabel ? replytoLabel->getWidth():0;
	int ww;

	int chw=0;

	std::vector<CustomHeader *>::iterator cb=customHeaders.begin(),
		ce=customHeaders.end();

	while (cb != ce)
	{
		std::string n= (*cb++)->name + ": ";

		widecharbuf wbuf;

		wbuf.init_string(n);

		int w=wbuf.wcwidth(0);

		if (chw < w)
			chw=w;
	}

	ww=tw;

	if (cw > ww) ww=cw;
	if (sw > ww) ww=sw;
	if (nw > ww) ww=nw;
	if (nfw > ww) ww=nfw;
	if (aw > ww) ww=aw;
	if (chw > ww) ww=chw;

	ww += 4;

	int r=0;

	int sW=getScreenWidth();

	erase(); // Clean start

	// Lay out the headers.  Keep in mind that some headers may be hidden,
	// so check for null ptr first.

#define SET(label, labelWidth, field) \
	do { label.setRow(r); label.setCol(ww-labelWidth); \
	field.setRow(r); field.setCol(ww); \
	field.setWidth( ww + 10 < sW ? sW - ww:10); \
	r += field.getHeight(); } while (0)

	if (from && fromLabel)
	{
		SET( (*fromLabel), fw, (*from));
	}

	if (to && toLabel)
	{
		SET( (*toLabel), tw, (*to));
	}

	if (cc && ccLabel)
	{
		SET( (*ccLabel), cw, (*cc));
	}

	if ( newsgroups && newsgroupsLabel)
	{
		SET( (*newsgroupsLabel), nw, (*newsgroups));
	}

	if ( newsgroups && followuptoLabel)
	{
		SET( (*followuptoLabel), nfw, (*followupto));
	}

	if (bcc && bccLabel)
	{
		SET( (*bccLabel), bw, (*bcc));
	}

	if (replyto && replytoLabel)
	{
		SET( (*replytoLabel), rw, (*replyto));
	}

	if (sentFolderLabel && sentFolderChoice)
	{
		sentFolderLabel->setRow(r);
		sentFolderLabel->setCol(ww-sentFolderLabel->getWidth());
		sentFolderChoice->setRow(r);
		sentFolderChoice->setCol(ww);
		++r;
	}

	SET( subjectLabel, sw, subject );

	cb=customHeaders.begin();
	ce=customHeaders.end();

	while (cb != ce)
	{
		CustomHeader *ch= *cb++;

		if (ch->label && ch->field)
		{
			ch->label->setRow(r);
			ch->field->setRow(r);
			ch->label->setCol(ww);
			ch->field->setCol(ww);

			ch->field->setWidth( ww + 10 < sW ? sW - ww:10);

			r++;
		}
	}

	SET( attLabel, aw, newAttachment );

#undef SET

	std::vector<AttachmentButton *>::iterator b, e;

	b=attachmentList.begin();
	e=attachmentList.end();

	while (b != e)
	{
		AttachmentButton *a= *b++;

		if (a)
		{
			a->setRow(r++);
			a->setCol(ww);
		}
	}

	message.setRow(++r);
	draw();
}

// Text entered into the add attachment field

void CursesEdit::addAttachment()
{
	std::string filename=newAttachment.getText();

	if (filename.size() == 0)
	{
		newAttachment.transferNextFocus();
		return;
	}

	std::vector<std::string> flist;

	CursesFileReq::expand(filename, flist);
	addAttachment(flist, "");
}

// Add the following attachment.

void CursesEdit::addAttachment(std::vector<std::string> &fileList, std::string mimeContentType)
{
	if (fileList.size() == 0)
		return;

	myServer::promptInfo response=
		myServer::prompt( myServer::promptInfo(_("Description (optional): ")));

	if (response.abortflag)
		return;

	std::string description=response;

	response=
		myServer::prompt( myServer::promptInfo(_("Inline attachment? (Y/N) "))
				  .yesno());

	if (response.abortflag)
		return;

	std::string inlineFlag=description;

	std::vector<std::string>::iterator b=fileList.begin(), e=fileList.end();

	size_t attNum=0;
	size_t attTotal=fileList.size();

	std::string errmsg;

	newAttachment.setText(""); // Do it again
	while (b != e)
	{
		std::string f= *b++;

		struct stat stat_buf;

		if (stat(f.c_str(), &stat_buf))
		{
			if (errmsg.size() > 0)
				errmsg += "\n";

			errmsg += f + ": " + strerror(errno);
		}

		mail::mimestruct pps;

		// attach() does the heavy lifting, clean up after it.

		std::string attachDescription=description;

		if (attTotal > 1 && description.size() > 0)
			attachDescription=Gettext(_("%1% (%2% of %3%)"))
				<< description << ++attNum << attTotal;

		if ((f=attach(f, attachDescription, inlineFlag == "Y"
			      ? "inline":"attachment",
			      mimeContentType, pps,
			      attNum-1, attTotal)).size() == 0)
			return; // Error

	// After attach() is done, create a new button.

		std::string name;
		std::string filename;

		CursesMessage::getDescriptionOf(&pps, NULL, name,
						filename, false);

		AttachmentButton *b=
			new AttachmentButton(this, f, name,
					     pps.content_transfer_encoding);

		if (!b)
			outofmemory();

		try {
			attachmentList.push_back(b);
		} catch (...) {
			delete b;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		addressResized(); // Redo layout
	}

	if (errmsg.size() > 0)
	{
		statusBar->clearstatus();
		statusBar->status(errmsg, statusBar->SYSERROR);
		statusBar->beepError();
	}
}

// librfc2045 callback: write out encoded mime attachment.

static int encode_callback(const char *p, size_t cnt, void *vp)
{
	return fwrite(p, cnt, 1, *(FILE **)vp) == 1 ? 0:-1;
}

//
// Encode a mime attachment.  Pick its encoding, copy it to
// config/att*.txt, with the appropriate set of headers.

std::string CursesEdit::attach(std::string filename, std::string description,
			  std::string disposition,
			  std::string contentType,
			  mail::mimestruct &pps,
			  size_t attNum,
			  size_t attTotal)
{
	std::string tmpname, attname;

	myMessage::createAttFilename(tmpname, attname); // Create filenames.

	std::string name_cpy;

	std::string errmsg="";

	statusBar->clearstatus();
	statusBar->status(_("Creating MIME attachment..."));
	statusBar->flush();

	FILE *fp=fopen(tmpname.c_str(), "w");

	if (!fp)
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return "";
	}

	struct stat stat_buf;

	try {

		if (description.size() > 0)
		{
			std::string s=
				mail::rfc2047::encode(description,
						      unicode_default_chset());

			fprintf(fp, "Content-Description: %s\n",
				s.c_str());
		}

		name_cpy=filename;

		std::string::iterator b, e, p;

		b=name_cpy.begin(), e=name_cpy.end(), p=b;

		while (b != e)
		{
			if (*b == '"' || *b == '\\')
				*b='_';

			if (*b++ == '/')
				p=b;
		}

		name_cpy.erase(name_cpy.begin(), p);

		if (contentType == GPGKEYMIMETYPE)
		{
			fprintf(fp, "Content-Disposition: %s\n",
				disposition.c_str());
			name_cpy="";
		}
		else
		{
			mail::mimestruct::parameterList paramList;

			paramList.set("filename", name_cpy,
				      unicode_default_chset(),
				      "");

			std::string s=paramList.toString(disposition);

			fprintf(fp, "Content-Disposition: %s\n", s.c_str());
		}

		// Figure out the MIME type.

		if (contentType.size() == 0) // Figure it out
		{
			contentType="auto"; // If we don't find the mime type.

			const char *ext=strrchr(filename.c_str(), '/');

			if (ext)
				ext++;
			else
				ext=filename.c_str();

			if (ext && (ext=strrchr(ext, '.')) != 0)
			{
				mail::mimetypes mimeType(ext+1);

				if (mimeType.found())
					contentType=mimeType.type;
			}
		}

		FILE *ifp=fopen(filename.c_str(), "r");

		if (!ifp)
			errmsg=strerror(errno);
		else try {
				int binflag;
				const char *encoding=
					libmail_encode_autodetect_fp(ifp, 0,
								     &binflag);

				if (ferror(ifp) || fseek(ifp, 0L, SEEK_SET) < 0)
					errmsg=strerror(errno);

				if (contentType == "auto")
					// Didn't find mime type, guess from encoding.
					contentType= binflag
						? "application/octet-stream"
						: "text/plain";

				fprintf(fp, "Content-Type: %s; charset=\"%s\"\n"
					"Content-Transfer-Encoding: %s\n\n",
					contentType.c_str(),
					unicode_default_chset(),
					encoding);

				pps.content_transfer_encoding=encoding;
				pps.type=contentType;

				{
					size_t n=pps.type.find('/');

					if (n != std::string::npos)
					{
						pps.subtype=pps.type
							.substr(n+1);
						pps.type=pps.type.substr(0, n);
					}
				}

				struct libmail_encode_info encodeInfo;

				libmail_encode_start(&encodeInfo, encoding,
						     &encode_callback,
						     &fp);

				char encodeBuf[BUFSIZ];
				int n=0;

				size_t byteCount=0;

				while (errmsg.size() == 0 &&
				       (n=fread(encodeBuf, 1, sizeof(encodeBuf), ifp))
				       > 0)
				{
					if (libmail_encode(&encodeInfo, encodeBuf, n))
						errmsg=strerror(errno);

					byteCount += n;
					myServer::reportProgress(byteCount, 0,
								 attNum, attTotal);
				}

				if (errmsg.size() == 0)
				{
					myServer::reportProgress(byteCount, byteCount,
								 attNum+1, attTotal);
					if (n < 0 ||
					    libmail_encode_end(&encodeInfo) < 0 ||
					    fflush(fp) < 0 || ferror(fp) ||
					    fstat(fileno(fp), &stat_buf) < 0)
						errmsg=strerror(errno);
				}

				fclose(ifp);
			} catch (...) {
				fclose(ifp);
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}

		fclose(fp);
	} catch (...) {
		fclose(fp);
		unlink(tmpname.c_str());
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	if (errmsg.size() > 0)
	{
		statusBar->clearstatus();
		statusBar->status(errmsg, statusBar->SYSERROR);
		return "";
	}

	pps.content_description=description;
	pps.content_size=stat_buf.st_size;
	pps.content_disposition=disposition;

	if (name_cpy.size() > 0)
		pps.content_disposition_parameters
			.set("FILENAME", name_cpy,
			     unicode_default_chset(), "");

	if (rename(tmpname.c_str(), attname.c_str()) < 0)
	{
		unlink(tmpname.c_str());
		statusBar->clearstatus();
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return "";
	}

	return attname;
}

//
// Delete a particular attachment.

void CursesEdit::delAttachment(std::string filename) // att*.txt
{
	restartAutosaveTimer();
	std::vector<AttachmentButton *>::iterator b, e, p;

	b=attachmentList.begin();
	e=attachmentList.end();

	while (b != e)
	{
		p=b;

		AttachmentButton *a= *b++;

		if (a && a->getFilename() == filename)
		{
			a->erase();
			bool needFocus=false;

			if (b != e && *b)
				(*b)->requestFocus();
			else
				needFocus=true;
			delete a;
			unlink(filename.c_str());
			*p=NULL;
			attachmentList.erase(p);
			resized();
			if (needFocus)
			{
				if (attachmentList.size() > 0 &&
				    attachmentList[attachmentList.size()-1])
					attachmentList[attachmentList.size()-1]
						->requestFocus();
				else
					newAttachment.requestFocus();
			}
			break;
		}
	}		
}

////////////////////////////////////////////////////////////////////////
//
// save() saves the message to a SaveSink object.  Depending on the
// context, SaveSink is subclassed to save the message to a file, or to
// a folder, or to sendmail, etc...

class CursesEdit::SaveSink {
public:
	SaveSink();
	virtual SaveSink &operator<<(std::string)=0;
	virtual ~SaveSink();
};

CursesEdit::SaveSink::SaveSink()
{
}

CursesEdit::SaveSink::~SaveSink()
{
}

//
// SaveSink subclass for autosaving the message.

class CursesEdit::SaveSinkAutosave : public CursesEdit::SaveSink {
public:
	std::ofstream savefile;
	std::string savefilename;

	SaveSinkAutosave();
	SaveSink &operator<<(std::string);
	virtual ~SaveSinkAutosave();
};

CursesEdit::SaveSinkAutosave::SaveSinkAutosave()
{
}

CursesEdit::SaveSinkAutosave::~SaveSinkAutosave()
{
	savefile.close();

	if (savefilename.size() > 0)
		unlink(savefilename.c_str());
}

CursesEdit::SaveSink &CursesEdit::SaveSinkAutosave::operator<<(std::string s)
{
	savefile << s;

	return *this;
}

//
// Periodically autosave the message.

void CursesEdit::restartAutosaveTimer()
{
	if (autosaveTimer.expired())
		autosaveTimer.setTimer(AUTOSAVEINTERVAL);
}

//
// When we draw the screen, draw a line of dashes right before msg text.

void CursesEdit::draw()
{
	CursesContainer::draw();

	std::vector<unicode_char> dashes;

	dashes.insert(dashes.end(), getWidth(), '-');

	writeText(dashes, message.getRow()-1, 0, CursesAttr());
}

void CursesEdit::erase()
{
	CursesContainer::erase();

	std::vector<unicode_char> dashes;

	dashes.insert(dashes.end(), getWidth(), ' ');

	writeText(dashes, message.getRow()-1, 0, CursesAttr());
}

//
// Special keys for this screen.
//

bool CursesEdit::processKey(const Curses::Key &key)
{
	if (statusBar->prompting())
		return false;

	if (key == key_FULLHEADERS)
	{
		if (!checkheaders())
			return true; // Make sure all headers are well formed.

		Curses::CursesAttr attr;

		attr.setFgColor(color_wm_headerName.fcolor);

#define TOGGLE(lab,labname,field,fieldsave) \
\
	if (lab) {delete lab; lab=NULL; } \
	else if ((lab=new CursesLabel(this, labname)) == NULL) \
			{ outofmemory(); } \
	else lab->setAttribute(attr); \
\
	if (field) { fieldsave.clear(); field->getAddresses(fieldsave); \
			delete field; field=NULL; } \
	else if ((field=new AddressList(this)) == NULL) \
			{ outofmemory(); } \
	else { field->setAddresses(fieldsave); }

#define TOGGLET(lab,labname,field,fieldsave) \
\
	if (lab) {delete lab; lab=NULL; } \
	else if ((lab=new CursesLabel(this, labname)) == NULL) \
			{ outofmemory(); } \
	else lab->setAttribute(attr); \
\
	if (field) { fieldsave=field->getText(); \
			delete field; field=NULL; } \
	else if ((field=new CursesField(this)) == NULL) \
			{ outofmemory(); } \
	else { field->setText(fieldsave); }

		erase();

		std::vector<CustomHeader *>::iterator cb, ce;

		cb=customHeaders.begin();
		ce=customHeaders.end();

		while (cb != ce)
		{
			CustomHeader *ch= *cb++;

			if (ch->isHidden)
			{
				TOGGLET(ch->label,
					(ch->name + " "),
					ch->field,
					ch->hiddenValue);

				if (ch->label)
					ch->label->setAlignment(Curses::RIGHT);

			}
		}

		if (newsgroupMode)
		{
			TOGGLE(toLabel, _("To: "), to, toV);
			TOGGLE(ccLabel, _("Cc: "), cc, ccV);
		}
		else
		{
			
			TOGGLET(newsgroupsLabel, _("Newsgroups: "),
				newsgroups, newsgroupsV);
			TOGGLET(followuptoLabel, _("Followup-To: "),
				followupto, followuptoV);
		}

		TOGGLE(fromLabel, _("From: "), from, fromV);

		TOGGLE(bccLabel, _("Bcc: "), bcc, bccV);

		TOGGLE(replytoLabel, _("Reply-To: "), replyto, replytoV);

		if (sentFolderLabel)
		{
			delete sentFolderLabel;
			sentFolderLabel=NULL;
		}
		else
		{
			sentFolderLabel=new CursesLabel(this, _("Fcc: "));

			if (!sentFolderLabel)
				outofmemory();
			sentFolderLabel->setAttribute(attr);
		}

		if (sentFolderChoice)
		{
			delete sentFolderChoice;
			sentFolderChoice=NULL;
		}
		else
		{
			sentFolderChoice=new CursesChoiceButton(this);

			std::vector<std::string> sentFolders;

			sentFolders.push_back(_("(none)"));

			SpecialFolder &sentFolder=
				SpecialFolder::folders.find(SENT)->second;

			size_t i;

			for (i=0; i<sentFolder.folderList.size(); i++)
				sentFolders
					.push_back(Gettext
						   ::fromutf8(sentFolder
							      .folderList[i]
							      .nameUTF8));

			sentFolderChoice->setOptions(sentFolders,
						     defaultSentChoice);
		}
		requestFocus();
		addressResized();

		return true;
	}

	if (key == key_ATTACHFILE)
	{
		std::vector<std::string> filenames;

		// Popup an attachment dialog.

		{
			OpenDialog open_dialog;

			open_dialog.requestFocus();
			myServer::eventloop();

			filenames=open_dialog.getFilenameList();
			mainScreen->erase();
		}
		mainScreen->draw();
		statusBar->draw();
		requestFocus();

		if (myServer::nextScreen)
			return true; // Unexpected event

		if (filenames.size())
			addAttachment(filenames, "");
		restartAutosaveTimer();
		return true;
	}

	if (key == key_ATTACHKEY)
	{
		myServer::promptInfo prompt=
			myServer::promptInfo(_("Attach public key? (Y/N) "))
			.yesno()
			.option(key_PRIVATEKEY,
				Gettext::keyname(_("PRIVATEKEY_K:P")),
				_("Attach private key"));

		prompt=myServer::prompt(prompt);

		if (prompt.abortflag || myServer::nextScreen)
			return true;

		bool privateKey=false;

		if ((std::string)prompt == "N")
			return true;

		if (key_PRIVATEKEY == prompt.firstChar())
			privateKey=true;

		std::string fingerprint= (GPG::gpg.*(privateKey ?
						&GPG::select_private_key:
						&GPG::select_public_key))
			("", _("ATTACH KEY"),
			 _("Select key to export and attach, then press ENTER"),
			 _("Attach key"), _("Cancel, do not attach.")
			 );
		mainScreen->draw();
		statusBar->draw();
		requestFocus();
		if (myServer::nextScreen || fingerprint.size() == 0)
			return true; // Unexpected event, or no selection.

		std::string tmpKey= myServer::getConfigDir() + "/pgpkey.tmp";

		std::ofstream o(tmpKey.c_str());

		if (o.bad() || o.fail())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return true;
		}

		std::string errmsg=GPG::exportKey(fingerprint, privateKey, o);

		if (errmsg.size() == 0 && (o.flush().fail()
					   || (o.close(), o.fail())
					   || o.bad()))
		{
			errmsg=strerror(errno);
		}

		if (errmsg.size() > 0)
		{
			statusBar->clearstatus();
			statusBar->status(errmsg);
			statusBar->beepError();
			return true;
		}

		std::vector<std::string> flist;

		flist.push_back(tmpKey);
		addAttachment(flist, GPGKEYMIMETYPE);
		return true;

	}

	if (key == key_ABORT && myServer::cmdcount > 0)
		return false; // Something's going on.

	if (key == key_POSTPONE || key == key_ABORT)
	{
		if (!checkheaders())
			return true;

		if (key == key_ABORT)
		{
			myServer::promptInfo prompt=myServer::
				prompt(myServer::
				       promptInfo(_("Discard this message? (Y/N) " ))
				       .yesno());

			if (prompt.abortflag)
				return true;

			if (!((std::string)prompt == "Y"))
				return true;
			myMessage::clearAttFiles();
		}

		myServer *draftServer;
		mail::folder *f=SpecialFolder::folders.find(DRAFTS)
			->second.getFolder(draftServer);

		if (!f)
			return true;

		try {
			if (key != key_ABORT)
			{
				if (!postpone(NULL, f, NULL, NULL))
					return true;
			}

			Curses::keepgoing=false;
			myServer::nextScreen= &hierarchyScreen;
			myServer::nextScreenArg=NULL;

			PreviousScreen::previousScreen();
			// Return to the previous screen.

			goodexit=true;
			delete f;
		} catch (...) {
			delete f;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		return true;
	}

#if 0
	if (key == (char)1)
	{
		myServer *sentServer;
		mail::folder *sentFolder;

		if (sentFolderChoice)
			defaultSentChoice=
				sentFolderChoice->getSelectedOption();

		SpecialFolder::Info *sentFolderInfo=NULL;

		if (defaultSentChoice == 0)
		{
			sentFolder=NULL;
			sentServer=NULL;
		}
		else
		{
			SpecialFolder &specialSentFolder=
				SpecialFolder::folders.find(SENT)->second;

			size_t n= defaultSentChoice-1;

			sentFolder=NULL;

			if (n < specialSentFolder.folderList.size())
			{
				sentFolderInfo=&specialSentFolder
					.folderList[n];

				sentFolder=sentFolderInfo
					->getFolder(sentServer);
			}
		}

		if (sentFolderInfo)
		{
			sentFolderInfo->lastArchivedMonth="000000";

			statusBar->clearstatus();
			statusBar->status(archiveSentFolder(sentFolderInfo,
							    sentFolder) ?
					  "archived":"not archived");
		}
		return true;
	}
#endif

	if (key == key_SEND)
	{
		if (!checkheaders())
			return true; // Headers must be well formed.

		myServer *sentServer;
		mail::folder *sentFolder;

		// Figure out the Fcc folder:

		if (sentFolderChoice)
			defaultSentChoice=
				sentFolderChoice->getSelectedOption();

		SpecialFolder::Info *sentFolderInfo=NULL;

		if (defaultSentChoice == 0) // No Fcc folder.
		{
			sentFolder=NULL;
			sentServer=NULL;
		}
		else
		{
			SpecialFolder &specialSentFolder=
				SpecialFolder::folders.find(SENT)->second;

			size_t n= defaultSentChoice-1;

			sentFolder=NULL;

			if (n < specialSentFolder.folderList.size())
			{
				sentFolderInfo=&specialSentFolder
					.folderList[n];

				sentFolder=sentFolderInfo
					->getFolder(sentServer);
			}
		}

		mail::smtpInfo smtpInfo;

		mail::folder *postFolder=NULL;

		myServer::nextScreen=NULL;
		try {
			mail::smtpInfo *smtpInfoPtr= &smtpInfo;
			CursesMessage::EncryptionInfo encryptInfo;

			myServer *s=myServer::getServerByUrl(xserver);

			if (s)
				encryptInfo.signKey=
					s->getFolderConfiguration(xfolder,
								  "SIGNKEY");

			if (s && encryptInfo.signKey.size() == 0)
				encryptInfo.signKey=
					s->getServerConfiguration("SIGNKEY");

			if (encryptInfo.signKey == "[]")
				encryptInfo.signKey="";

			if (getPostFolder(postFolder, encryptInfo) && // Posting to nntp?
			    checkSendFolder(postFolder, smtpInfoPtr,
					    sentFolderInfo, sentFolder,
					    encryptInfo) &&
			    // Check for sent/fcc folder

			    myServer::nextScreen == NULL &&
			    // Nothing unexpected happened

			    postpone(smtpInfoPtr, sentFolder, postFolder,
				     &encryptInfo))
			{

				std::string fromhdr, replytohdr, customhdr;

				saveheaders(fromhdr, replytohdr, customhdr,
					    NULL);

				goodexit=true;

				saved(fromhdr, replytohdr, getFcc(NULL),
				      customhdr, encryptInfo.signKey);

				myServer::nextScreen= &hierarchyScreen;
				myServer::nextScreenArg=NULL;
				PreviousScreen::previousScreen();
			}

			requestFocus();

			if (myServer::nextScreen)
				Curses::keepgoing=false;
			// Either above, or by an unexpected event
			// (server disconnected)

			if (postFolder)
			{
				delete postFolder;
				postFolder=NULL;
			}
			if (sentFolder)
				delete sentFolder;

		} catch (...) {
			if (postFolder)
				delete postFolder;

			if (sentFolder)
				delete sentFolder;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		return true;
	}

	return false;
}

//
// If the current message needs to be posted to a newsgroup, initialize a
// folder object, for posting.
//

bool CursesEdit::getPostFolder(mail::folder *&postFolder,
			       CursesMessage::EncryptionInfo &encryptInfo)
{
	postFolder=NULL;  // Not posting just yet.

	if (newsgroups)
		newsgroupsV=newsgroups->getText();

	if (followupto)
		followuptoV=followupto->getText();

	std::string::iterator b=newsgroupsV.begin(), e;

	while (b != newsgroupsV.end() && unicode_isspace((unsigned char)*b))
		b++;

	newsgroupsV.erase(newsgroupsV.begin(), b);

	for (b=e=newsgroupsV.begin(); b != newsgroupsV.end(); )
	{
		if (!unicode_isspace((unsigned char)*b++))
			e=b;
	}

	newsgroupsV.erase(b, newsgroupsV.end());

	if (newsgroupsV.size() == 0)
		return true;

	std::string errmsg;

	if (nntpCommandFolder::nntpCommand.size() > 0)
	{
		// Special command for posting to a news server.
		postFolder=new nntpCommandFolder();

		if (!postFolder)
			errmsg=strerror(errno);
	}
	else
	{
		// We need to find a server to post to.

		myServer *s=myServer::getServerByUrl(xserver);

		if (s && CursesHierarchy::autologin(s) &&
		    s->server->getCapability(LIBMAIL_SERVERTYPE) == "nntp")
		{
			; // Original msg from a nntp server, post there.
		}
		else
		{
			// Find ANY nntp server.

			s=NULL;

			std::vector<myServer *>::iterator b=
				myServer::server_list.begin(),
				e=myServer::server_list.end();

			for ( ; b != e; b++)
			{
				if ((*b)->server &&
				    (*b)->server
				    ->getCapability(LIBMAIL_SERVERTYPE)
				    == "nntp")
				{
					s= *b;
					break;
				}
			}
		}

		if (!s)
		{
			statusBar->clearstatus();
			statusBar->status(_("NetNews server not found. "
					    "Please log on to a server first.")
					  );
			statusBar->beepError();
			return false;
		}

		// Get a posting folder from libmail.

		mail::smtpInfo nntpInfo;

		nntpInfo.options.insert(make_pair(std::string("POST"),
						  std::string("1")));

		postFolder=s->server->getSendFolder(nntpInfo, NULL, errmsg);
	}

	if (!postFolder)
	{
		statusBar->clearstatus();
		statusBar->status(errmsg);
		statusBar->beepError();
		return false;
	}

	std::string fromhdr, replytohdr, customhdr;

	saveheaders(fromhdr, replytohdr, customhdr, NULL);

	// If there are no listed recipients, and GPG is installed, use a different prompt.

	if (recipients.size() == 0)
	{
		std::vector<std::string> saveEncryptionKeys;

		CursesMessage::EncryptionInfo *encryptInfoPtr= &encryptInfo;

		CursesMessage::initEncryptInfo(encryptInfoPtr, saveEncryptionKeys);

		while (encryptInfoPtr) // This is an IF statement, really
		{
			std::string prompt=Gettext(_("Post message%1%? (Y/N) "))
				<< (encryptInfoPtr->signKey.size() > 0 ?
				    encryptInfoPtr->encryptionKeys.size() > 0 ?
				    _(" (sign and encrypt)"):_(" (sign)"):
				    encryptInfoPtr->encryptionKeys.size() > 0 ?
				    _(" (encrypt)"):"");


			myServer::promptInfo option=
				myServer::prompt(myServer::promptInfo(prompt)
						 .yesno()
						 .option(key_SIGN,
							 Gettext::keyname(_("SIGN_K:S")),
							 _("Sign"))
						 .option(key_ENCRYPT,
							 Gettext::keyname(_("ENCRYPT_K:E")),
							 _("Encrypt")));

			if (option.abortflag || !Curses::keepgoing ||
			    myServer::nextScreen)
				return false;

			if ((std::string)option == "N")
			{
				return false;
			}



			unicode_char promptKey=option.firstChar();

			if (key_SIGN == promptKey)
			{
				CursesMessage::initEncryptSign(encryptInfoPtr);
				if (!Curses::keepgoing)
					return false;
				continue;
			}

			if (key_ENCRYPT == promptKey)
			{
				CursesMessage::initEncryptEncrypt(encryptInfoPtr,
								  saveEncryptionKeys);
				if (!Curses::keepgoing)
					return false;
				continue;
			}
			return CursesMessage::setEncryptionOptions(encryptInfoPtr);
		}
	}

	myServer::promptInfo yesno=
		myServer::prompt(myServer
				 ::promptInfo(_("Post message? (Y/N) "))
				 .yesno());

	if (yesno.abortflag)
	{
		delete postFolder;
		postFolder=NULL;
		return false;
	}

	if ((std::string)yesno != "Y")
	{
		delete postFolder;
		postFolder=NULL;
		return false;
	}
	return true;
}

//
// After checking for a posting folder, check for the sent/fcc folder.
//
// Arguments:
//
// postFolder - not NULL if we know that we're posting to a newsgroup.
// smtpInfo - ptr to an smtpinfo structure that describes sending parameters
// sentFolderInfo - definition of the sent mail folder (when it was last
//                  archived)
// sentFolder - the sent folder
//
// If postFolder is not NULL (posting), then if there are no listed recipients,
// smtpInfoPtr is set to NULL, and we exit.
// Otherwise, if it turns out there are no listed recipients, that will be
// caught in postpone()
//

bool CursesEdit::checkSendFolder(mail::folder *postFolder,
				 mail::smtpInfo * &smtpInfoPtr,
				 SpecialFolder::Info *sentFolderInfo,
				 mail::folder *sentFolder,
				 CursesMessage::EncryptionInfo &encryptInfo)
{
	std::string prompt=_("Send message? (Y/N) ");
	std::string prompt2=_("send message? (Y/N) ");

	std::string fromhdr, replytohdr, customhdr;

	saveheaders(fromhdr, replytohdr, customhdr, NULL);

	if (postFolder)
	{
		if (recipients.size() == 0)
		{
			smtpInfoPtr=NULL;
			return archiveSentFolder(sentFolderInfo, sentFolder);
		}

		prompt=_("Mail copies to listed recipients? (Y/N) ");
		prompt2=_("mail copies to listed recipients? (Y/N) ");
	}

	std::vector<mail::address> addresses;

	std::vector<std::string>::iterator ab=recipients.begin(),
		ae=recipients.end();

	while (ab != ae)
		addresses.push_back( mail::address("", *ab++));

	// Also select the sender's public key, by default.

	{
		std::vector<mail::address> a;
		size_t dummy;

		mail::address::fromString(fromhdr, a, dummy);

		addresses.insert(addresses.end(), a.begin(), a.end());
	}

	std::vector<GPG::Key_iterator> defaultKeys;

	GPG::gpg.find_public_keys(addresses, defaultKeys);

	std::vector<GPG::Key_iterator>::iterator kb=defaultKeys.begin(),
		ke=defaultKeys.end();

	while (kb != ke)
	{
		encryptInfo.encryptionKeys.push_back( (*kb++)->fingerprint );
	}

	bool rc=CursesMessage::getSendInfo(prompt, prompt2, *smtpInfoPtr,
					   &encryptInfo);


	return rc && archiveSentFolder(sentFolderInfo, sentFolder);
}

//
// Once a month, archive the sent folder.
//
// Returns false if an error occurs.

bool CursesEdit::archiveSentFolder(SpecialFolder::Info *sentFolderInfo,
				   mail::folder *sentFolder)
{
	time_t now=time(NULL);
	struct tm *nowtm=localtime(&now);
	struct tm saveTm= *nowtm;
	std::string thisMonth;

	{
		std::stringstream o;

		o << saveTm.tm_year + 1900 << "-"
		  << std::setw(2) << std::setfill('0')
		  << saveTm.tm_mon + 1;

		thisMonth=o.str();
	}

	if (sentFolderInfo == NULL ||
	    sentFolderInfo->lastArchivedMonth == thisMonth)
		return true; // No sent folder, or already archived.

	if (sentFolderInfo->lastArchivedMonth == "")
		// First time for this folder, so mark it as archived this mon
	{
		sentFolderInfo->lastArchivedMonth=thisMonth;
		myServer::saveconfig();
		return true;
	}


	myServer::promptInfo answer=
		myServer::prompt(myServer
				 ::promptInfo(_("Rename last month's selected sent mail folder? (Y/N) "))
				 .yesno());

	if (answer.abortflag)
		return false;

	if ( (std::string)answer != "Y")
	{
		sentFolderInfo->lastArchivedMonth=thisMonth;
		// Don't ask again.

		myServer::saveconfig();
		return true;
	}

	myServer::CreateFolderCallback yearFolderDirectory;


	// If this sent folder is named foo, create directory foo-yyyy
	std::stringstream o;
	std::string year;

	o << sentFolder->getName() << "-" <<
		(saveTm.tm_year + 1900 + (saveTm.tm_mon == 0 ? -1:0));

	year=o.str();

	// Get sent folder's parent folder, we want to create a new subdir.

	myServer::CreateFolderCallback parentFolder;
	myServer::Callback callback;
	sentFolder->getParentFolder(parentFolder, callback);
	if (!myServer::eventloop(callback))
		return false;


	std::string errmsg=createArchiveDir(parentFolder.folderFound, year,
				       yearFolderDirectory);

	if (!yearFolderDirectory.folderFound) // createArchiveDir failed.
	{
		statusBar->clearstatus();
		statusBar->status(errmsg, statusBar->SERVERERROR);
		statusBar->beepError();
		return false;
	}

	renameArchiveFolder(sentFolder, yearFolderDirectory.folderFound,
			    saveTm);  // Move this sent dir there.

	{
		// Create a new sent folder

		myServer::Callback callback;

		callback.noreport=true;

		sentFolder->create(false, callback);
		myServer::eventloop(callback);
	}

	sentFolderInfo->lastArchivedMonth=thisMonth;
	myServer::saveconfig();
	return true;
}

//
// Create sentfolder-YYYY directory to archive sent folder
//

std::string CursesEdit::createArchiveDir(mail::folder *parentFolder,
				    std::string subdir,
				    myServer::CreateFolderCallback &archiveDir)
{
	myServer::Callback callback;

	callback.noreport=true;
	parentFolder->createSubFolder(subdir, true, archiveDir, callback);

	// If create failed, perhaps this subdir already exists

	if (!myServer::eventloop(callback) || !archiveDir.folderFound)
	{
		OpenSubFoldersCallback openSubfolders;
		myServer::Callback callback2;

		callback2.noreport=true;
		parentFolder->readSubFolders(openSubfolders, callback2);
		myServer::eventloop(callback2);

		std::vector<mail::folder *>::iterator b, e;

		b=openSubfolders.folders.begin();
		e=openSubfolders.folders.end();

		while (b != e)
		{
			mail::folder *f= *b++;

			if (f->getName() == subdir)
			{
				archiveDir.folderFound=f->clone();

				if (!archiveDir.folderFound)
					LIBMAIL_THROW((strerror(errno)));
				break;
			}
		}
	}

	return callback.msg;
}

// Finally rename the old sent folder to its archived place.

void CursesEdit::renameArchiveFolder(mail::folder *sentFolder,
				     mail::folder *newParent,
				     struct tm lastMonthTm)
{
	OpenSubFoldersCallback renameSubFolder;
	myServer::Callback callback;

	callback.noreport=true;

	char folderName[100];

	if (lastMonthTm.tm_mon == 0)
	{
		lastMonthTm.tm_mon=12;
		--lastMonthTm.tm_year;
	}
	--lastMonthTm.tm_mon;

	strftime(folderName, sizeof(folderName),"%m-%b", &lastMonthTm);

	sentFolder->renameFolder(newParent, folderName, renameSubFolder,
				 callback);
	myServer::eventloop(callback);
}

///////////////////////////////////////////////////////////////////////////
//
// When sending a message, the message may be sent to up to three
// destinations:
//
// 1) An SMTP server
// 2) An NNTP server (post and/or mail)
// 3) A folder
//
// This SaveSink subclass pushes the message to any defined destination.
//

class CursesEdit::SaveSinkFile : public CursesEdit::SaveSink {

	CursesEdit &edit;

public:

	// We may need to create a server+folder libmail objects for sending.
	// This class will clean them up at exit.

	mail::account *smtpServer;
	mail::folder *smtpFolder;

	mail::addMessage *sendMessage;
	mail::addMessage *saveMessage;
	mail::addMessage *postMessage;

	SaveSinkFile(CursesEdit &editArg);
	SaveSink &operator<<(std::string);
	virtual ~SaveSinkFile();

	void abort(std::string errmsg);
};

CursesEdit::SaveSinkFile::SaveSinkFile(CursesEdit &editArg)
	: edit(editArg),
	  smtpServer(NULL),
	  smtpFolder(NULL),
	  sendMessage(NULL),
	  saveMessage(NULL),
	  postMessage(NULL)
{
	// Cancel the autosave timer while we exists.

	edit.autosaveTimer.cancelTimer();
}

void CursesEdit::SaveSinkFile::abort(std::string errmsg)
{
	// Fail whatever objects we have.

	mail::addMessage *p=sendMessage;

	sendMessage=NULL;

	if (p)
		p->fail(errmsg);

	p=saveMessage;
	saveMessage=NULL;
	if (p)
		p->fail(errmsg);

	p=postMessage;
	postMessage=NULL;

	if (p)
		p->fail(errmsg);
}

CursesEdit::SaveSinkFile::~SaveSinkFile()
{
	if (postMessage)
		postMessage->fail(_("Message post aborted"));
	if (sendMessage)
		sendMessage->fail(_("Message send aborted"));
	if (saveMessage)
		saveMessage->fail(_("Message save aborted"));
	if (smtpFolder)
		delete smtpFolder;
	if (smtpServer)
		delete smtpServer;
}

CursesEdit::SaveSink &CursesEdit::SaveSinkFile::operator<<(std::string s)
{
	if (sendMessage)
		sendMessage->saveMessageContents(s);
	if (saveMessage)
		saveMessage->saveMessageContents(s);
	if (postMessage)
		postMessage->saveMessageContents(s);
	return *this;
}

///////////////////////////////////////////////////////////////////////////
//
// When encryption is used, postpone() receives an EncryptSinkFile instead
// of SaveSinkFile.  The original SaveSinkFile is changed into by EncryptSinkFile,
// which encrypts or signs the message, and then feeds the result to the original
// SaveSinkFile.

class CursesEdit::EncryptSinkFile : public CursesEdit::SaveSink {

	SaveSink &origSink;
	CursesMessage::EncryptionInfo &encryptionInfo;
	FILE *tFile;
	FILE *passFd;

	static int input_func(char *, size_t, void *);
	static void output_func(const char *, size_t, void *);

	int input_func(char *, size_t);
	void output_func(const char *, size_t);

	std::string errmsg;

	static void errhandler_func(const char *errmsg, void *errmsg_arg);

public:
	EncryptSinkFile(SaveSink &origSinkArg,
			CursesMessage::EncryptionInfo &encryptionInfoArg);
	~EncryptSinkFile();

	SaveSink &operator<<(std::string);

	bool init(); // Start the deal going.
	std::string done(); // Done feeding the message, encrypt it.
};

CursesEdit::EncryptSinkFile
::EncryptSinkFile(SaveSink &origSinkArg,
		  CursesMessage::EncryptionInfo &encryptionInfoArg)
	: origSink(origSinkArg), encryptionInfo(encryptionInfoArg),
	  tFile(NULL), passFd(NULL)
{
}

CursesEdit::EncryptSinkFile::~EncryptSinkFile()
{
	if (tFile)
		fclose(tFile);

	if (passFd)
		fclose(passFd);
}

bool CursesEdit::EncryptSinkFile::init()
{
	if ((tFile=tmpfile()) == NULL)
		return false;

	return true;
}

CursesEdit::SaveSink &CursesEdit::EncryptSinkFile::operator<<(std::string s)
{
	if (fwrite(&s[0], s.size(), 1, tFile) != 1)
		; // Ignore gcc warning
	return *this;
}

std::string CursesEdit::EncryptSinkFile::done()
{
	struct libmail_gpg_info gi;
	std::string passphrase_fd;

	memset(&gi, 0, sizeof(gi));

	if (ferror(tFile) || fflush(tFile) < 0 ||
	    fseek(tFile, 0L, SEEK_SET) < 0)
		return strerror(errno);

	if (encryptionInfo.passphrase.size() > 0)
	{
		if ((passFd=tmpfile()) == NULL ||
		    fprintf(passFd, "%s", encryptionInfo.passphrase.c_str()) < 0 ||
		    fflush(passFd) < 0 || fseek(passFd, 0L, SEEK_SET) < 0)
			return strerror(errno);

		std::ostringstream o;

		o << fileno(passFd);

		passphrase_fd=o.str();

		gi.passphrase_fd= passphrase_fd.c_str();
	}

	gi.input_func= &CursesEdit::EncryptSinkFile::input_func;
	gi.input_func_arg= this;

	gi.output_func= &CursesEdit::EncryptSinkFile::output_func;
	gi.output_func_arg= this;

	gi.errhandler_func= &CursesEdit::EncryptSinkFile::errhandler_func;
	gi.errhandler_arg=this;

	int dosign;
	int doencode;

	std::vector< std::vector<char> > argv_ptr;
	std::vector<char *> argv_cp;

	{
		std::vector<std::string> argv;

		dosign=encryptionInfo.signKey.size() > 0;

		if (dosign)
		{
			argv.push_back(std::string("--default-key"));
			argv.push_back(encryptionInfo.signKey);
		}

		doencode=encryptionInfo.encryptionKeys.size() > 0
			? LIBMAIL_GPG_ENCAPSULATE:0;

		std::vector<std::string>::iterator ekb=encryptionInfo.encryptionKeys.begin(),
			eke=encryptionInfo.encryptionKeys.end();

		while (ekb != eke)
		{
			argv.push_back(std::string("-r"));
			argv.push_back( *ekb++ );
		}

		argv.push_back(std::string("--no-tty"));
		argv.insert(argv.end(), encryptionInfo.otherArgs.begin(),
			    encryptionInfo.otherArgs.end());


		GPG::create_argv(argv, argv_ptr, argv_cp);
	}

	gi.argv=&argv_cp[0];
	gi.argc=argv_cp.size();

	if (libmail_gpg_signencode(dosign, doencode, &gi))
	{
		// If signing failed, un-memorize the passphrase.

		if (encryptionInfo.signKey.size() > 0)
			PasswordList::passwordList.remove("gpg:" +
							  encryptionInfo
							  .signKey);
			
		if (errmsg.size() == 0)
			errmsg=strerror(errno);
		return errmsg;
	}

	return "";
}

int CursesEdit::EncryptSinkFile::input_func(char *p, size_t n, void *vp)
{
	return ((CursesEdit::EncryptSinkFile *)vp)->input_func(p, n);
}

void CursesEdit::EncryptSinkFile::output_func(const char *p, size_t n, void *vp)
{
	return ((CursesEdit::EncryptSinkFile *)vp)->output_func(p, n);
}


int CursesEdit::EncryptSinkFile::input_func(char *p, size_t n)
{
	int rc=libmail_gpg_inputfunc_readfp(p, n, tFile);

	return rc;

}

void CursesEdit::EncryptSinkFile::output_func(const char *p, size_t n)
{
	origSink << std::string(p, p+n);
}

void CursesEdit::EncryptSinkFile::errhandler_func(const char *errmsg,
						  void *errmsg_arg)
{
	((CursesEdit::EncryptSinkFile *)errmsg_arg)->errmsg=errmsg;
}

bool CursesEdit::postpone(mail::smtpInfo *sendInfo, // Not NULL - send msg
			  mail::folder *folder,	// Not NULL - save msg to fold
			  mail::folder *postingFolder, // Not NULL - posting
			  CursesMessage::EncryptionInfo *encryptionInfo
			  // Not NULL - encrypt or sign message
			  )
{
	disconnectCallbackStub disconnectStub;
	myServer::Callback sendCallback;
	myServer::Callback saveCallback;
	myServer::Callback postCallback;

	SaveSinkFile saveAndSend(*this);

	bool sent=false;
	bool saved=false;

	if (sendInfo)
	{
		std::string dummy;

		saveheaders(dummy, dummy, dummy, NULL);

		if (recipients.size() == 0)
		{
			statusBar->clearstatus();
			statusBar->status(_("No recipients specified."));
			statusBar->beepError();
			return false;
		}

		if (sender.size() == 0)
		{
			statusBar->clearstatus();
			statusBar->status(_("From: header cannot be empty."));
			statusBar->beepError();
			return false;
		}

		sendInfo->sender=sender;
		sendInfo->recipients=recipients;
		saveAndSend.smtpFolder=
			CursesMessage::getSendFolder(*sendInfo,
						     saveAndSend.smtpServer,
						     &folder,
						     disconnectStub);
		if (!saveAndSend.smtpFolder)
			return false;

		sendCallback.noreport=true;

		saveAndSend.sendMessage=
			saveAndSend.smtpFolder->addMessage(sendCallback);

		if (!saveAndSend.sendMessage)
		{
			statusBar->clearstatus();
			statusBar->status(sendCallback.msg,
					  statusBar->SERVERERROR);
			statusBar->beepError();
			return false;
		}
	}

	CursesMessage::EncryptionInfo dummyEncryptionStructure;

	EncryptSinkFile encrypt(saveAndSend, *(encryptionInfo ? encryptionInfo
					       : &dummyEncryptionStructure));

	saveCallback.noreport=true;

	saveAndSend.saveMessage=NULL;

	if (folder) // Fcc-ing msg to a folder.
	{
		saveAndSend.saveMessage=folder->addMessage(saveCallback);

		if (!saveAndSend.saveMessage)
		{
			statusBar->clearstatus();
			statusBar->status(saveCallback.msg,
					  statusBar->SERVERERROR);
			statusBar->beepError();
			return false;
		}

		if (sendInfo == NULL && postingFolder == NULL)
			// If not posting/sending, we're saving a draft
			saveAndSend.saveMessage->messageInfo.draft=true;
	}

	if (postingFolder) // Posting
	{
		saveAndSend.postMessage=
			postingFolder->addMessage(postCallback);

		if (!saveAndSend.postMessage)
		{
			statusBar->clearstatus();
			statusBar->status(postCallback.msg,
					  statusBar->SERVERERROR);
			statusBar->beepError();
			return false;
		}
	}

	// Here we go.

	statusBar->clearstatus();
	statusBar->status(_("Assembling MIME message..."));

	SaveSink &sink= encryptionInfo && encryptionInfo->isUsing()
		? (SaveSink &)encrypt:(SaveSink &)saveAndSend;

	if (encryptionInfo && encryptionInfo->isUsing())
	{
		if (!encrypt.init())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return false;
		}
	}

	if (!save(sink, false, true,
		  sendInfo == NULL && postingFolder == NULL))
		return false;

	// All right.  Let's carefully wrap things up.

	if (encryptionInfo && encryptionInfo->isUsing())
	{
		statusBar->clearstatus();
		statusBar->status(_("Encrypting/signing..."));
		statusBar->flush();

		std::string errmsg=encrypt.done();

		if (errmsg.size() > 0)
		{
			saveAndSend.abort(errmsg);

			if (saveAndSend.smtpServer) // Should be there.
			{
				myServer::Callback logoutCallback;
				logoutCallback.noreport=true;

				saveAndSend.smtpServer->logout(logoutCallback);

				myServer::eventloop(logoutCallback);
				delete saveAndSend.smtpServer;
				saveAndSend.smtpServer=NULL;
			}


			statusBar->clearstatus();
			statusBar->status(errmsg);
			statusBar->beepError();
			return false;
		}
	}

	if (folder) // Fcc comes first.
	{
		statusBar->clearstatus();
		statusBar->status(Gettext(_("Saving message in %1%..."))
				  << folder->getName());

		saveAndSend.saveMessage->go();
		saveAndSend.saveMessage=NULL;

		if (!myServer::eventloop(saveCallback))
		{
			statusBar->clearstatus();
			statusBar->status(saveCallback.msg,
					  statusBar->SERVERERROR);
			statusBar->beepError();
			return false;
		}

		statusBar->clearstatus();
		statusBar->status(saveCallback.msg);
		saved=true;
	}

	if (saveAndSend.sendMessage) // Sending.
	{
		statusBar->clearstatus();
		statusBar->status(_("Sending message..."));

		saveAndSend.sendMessage->go();
		saveAndSend.sendMessage=NULL;

		if (!myServer::eventloop(sendCallback))
		{
			std::string errmsg=sendCallback.msg;

			if (sent)  // We already filed an fcc.
			{
				errmsg=Gettext(_("The mail server rejected the message with the following error:\n\n    %1%\n"

						 "F.Y.I: A copy of the unsent message was also saved in"
						 " the sent-mail folder."))
							 << errmsg;
			}

			statusBar->clearstatus();
			statusBar->status(errmsg,
					  statusBar->SERVERERROR);
			statusBar->beepError();
			return false;
		}

		if (saveAndSend.smtpServer) // Should be there.
		{
			myServer::Callback logoutCallback;
			logoutCallback.noreport=true;

			saveAndSend.smtpServer->logout(logoutCallback);

			myServer::eventloop(logoutCallback);
			delete saveAndSend.smtpServer;
			saveAndSend.smtpServer=NULL;
		}

		markreplied();

		statusBar->clearstatus();
		statusBar->status(sendCallback.msg);
		sent=true;
	}

	if (saveAndSend.postMessage) // Posting.
	{
		statusBar->clearstatus();
		statusBar->status(_("Posting message..."));

		saveAndSend.postMessage->go();
		saveAndSend.postMessage=NULL;

		if (!myServer::eventloop(postCallback))
		{
			std::string errmsg=postCallback.msg;

			if (saved || sent)
			{
				errmsg=Gettext(_("The NetNews server rejected the message with the following error:\n\n    %1%%2%%3%")) << errmsg <<
					(sent ?
					 _("\nHowever, a copy of the message was already mailed to its listed recipients.\n"
					   "Remove the recipient list from the message before attempting to repost it,"
					   " in order to prevent sending a duplicate copy to the original recipients."):"") <<
					(saved ?
					 "\nF.Y.I: A copy of the unposted message was also saved in"
					 " the sent-mail folder.":"");
			}

			statusBar->clearstatus();
			statusBar->status(errmsg,
					  statusBar->SERVERERROR);
			statusBar->beepError();
			return false;
		}
		statusBar->clearstatus();
		statusBar->status(sendCallback.msg);
	}

	myMessage::clearAttFiles();

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
// After sending the message, try to mark something as replied.

void CursesEdit::markreplied()
{
	std::string errmsg="";
	myServer *s;

	// Not really a while loop, more like an if statement.

	while (xserver.size() > 0 && xfolder.size() > 0
	       && xuid.size() > 0
	       && (s=myServer::getServerByUrl(xserver)) != NULL)
	{
		std::string xduid=mail::rfc2047::decoder::decodeSimple(xuid);

		// Wanna mark something as replied

		// See if we're lucky, and the folder is currently open.

		if (s->currentFolder && !s->currentFolder->mustReopen)
		{
			const mail::folder *f=s->currentFolder->getFolder();

			if (f->getPath() == xfolder)
			{
				mail::account *server=s->server;
				size_t n=server->getFolderIndexSize();
				size_t i;

				for (i=0; i<n; i++)
				{
					mail::messageInfo info=
						server->getFolderIndexInfo(i);

					if (info.uid != xduid)
						continue;

					info.replied=true;
					myServer::Callback callback;

					callback.noreport=true;

					server->saveFolderIndexInfo(i, info,
								    callback);

					myServer::eventloop(callback);
					break;
				}

				if (i < n)
					break;
			}
		}

		// We'll have to open another login.  We can't just open
		// the folder because we won't be able to go back to the
		// previous screen, which may be tied to the currently-
		// opened folder.
		// If this adventure fails, we'll still return true.

		// Make sure we're logged in.

		if (!CursesHierarchy::autologin(s))
			return;

		mail::account *anotherLogin=NULL;
		disconnectCallbackStub disconnectStub;
		dummyFolderCallback folderStub;

		// Step 1: log in

		try {
			{
				mail::account::openInfo loginInfo;
				myServer::Callback callback;

				callback.noreport=true;
				loginInfo.url=s->url;
				loginInfo.pwd=s->password;
				s->find_cert_by_id(loginInfo.certificates);
				loginInfo.extraString=
					myServer::getConfigDir()
					+ "/" + s->newsrc;

				if (s->newsrc.size() == 0)
					loginInfo.extraString="";

				if ((anotherLogin=
				     mail::account::open(loginInfo,
							 callback,
							 disconnectStub)
				     ) == NULL)
				{
					errmsg=callback.msg;
					break;
				}

				if (!myServer::eventloop(callback))
				{
					delete anotherLogin;
					errmsg=callback.msg;
					break;
				}
			}
		} catch (...) {
			if (anotherLogin)
				delete anotherLogin;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		mail::folder *loginFolder=NULL;

		try {
			myReadFolders folders;

			// Step 2: find the folder

			std::string folderString="";

			{
				myServer::Callback findCallback;
				findCallback.noreport=true;

				anotherLogin->findFolder(xfolder,
							 folders,
							 findCallback);

				if (myServer::eventloop(findCallback) &&
				    folders.size() == 1)
					folderString=folders[0];
			}

			loginFolder=folderString.size() > 0 ?
				anotherLogin->folderFromString(folderString):
				NULL;

			// Step 3: open the folder

			if (loginFolder)
			{
				myServer::Callback selectCallback;

				selectCallback.noreport=true;
				loginFolder->open(selectCallback,
						  NULL,
						  folderStub);
				// OPENFOLDER

				if (!myServer::eventloop(selectCallback))
				{
					delete loginFolder;
					loginFolder=NULL;
				}
			}

			// Step 4: find the message, mark it as replied.

			size_t n=loginFolder ?
				anotherLogin->getFolderIndexSize():0;
			size_t i;

			for (i=0; i<n; i++)
			{
				mail::messageInfo info=
					anotherLogin->getFolderIndexInfo(i);

				if (info.uid != xduid)
					continue;

				info.replied=true;
				myServer::Callback callback;

				callback.noreport=true;

				anotherLogin->saveFolderIndexInfo(i, info,
								  callback);

				myServer::eventloop(callback);
				break;
			}

			// Step 5: log out nicely.

			myServer::Callback logoutCallback;

			logoutCallback.noreport=true;

			anotherLogin->logout(logoutCallback);
			myServer::eventloop(logoutCallback);

			if (loginFolder)
				delete loginFolder;
			loginFolder=NULL;
			delete anotherLogin;

		} catch (...) {
			if (loginFolder)
				delete loginFolder;

			delete anotherLogin;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		break;
	}

	if (errmsg.size() > 0)
	{
		statusBar->clearstatus();
		statusBar->status(errmsg);
		statusBar->beepError();
	}
}

bool CursesEdit::listKeys( std::vector< std::pair<std::string, std::string> > &list)
{
	list.push_back(std::make_pair(Gettext::keyname(_("FULLHDRS_K:^F")),
				 _("Full Hdrs")));
	list.push_back(std::make_pair(Gettext::keyname(_("SEND_K:^X")),
				 _("Send")));
	list.push_back(std::make_pair(Gettext::keyname(_("POSTPONE_K:^P")),
				 _("Postpone")));
	list.push_back(std::make_pair(Gettext::keyname(_("ATTACHMENT_K:^T")),
				 _("Attach")));

	if (GPG::gpg_installed())
		list.push_back(std::make_pair(Gettext::keyname(_("ATTACHKEY_K:^E")),
					 _("Attach Key")));

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
//                  Assemble and save the message


// Compute a unique multipart boundary.  Verify that the unique string is
// really unique.

static bool search_boundary(FILE *fp, std::string boundary, size_t &cnt,
			    size_t nparts)
{
	try {
		const char *p=boundary.c_str();
		size_t l=strlen(p);

		char buffer[1000];

		while (fgets(buffer, sizeof(buffer), fp))
		{
			cnt += strlen(buffer);

			myServer::reportProgress(cnt, 0, 1, nparts);
			if (buffer[0] == '-' && buffer[1] == '-' &&
			    strncasecmp(buffer + 2, p, l) == 0)
				return true;
		}
	} catch (...) {
	}

	return false;
}

//
// Concatenate all att*.txt files together into a multipart msg
//

static void copy_multipart(FILE *f, // Opened att*.txt file

			   CursesEdit::SaveSink &sink, // sinking there

			   std::string boundary, // multipart delimiter

			   size_t partNum,	// Feedback reporting
			   size_t nparts)	// Feedback reporting.
{
	int c;
	size_t byteCount=0;
	char buffer[BUFSIZ];
	size_t i=0;

	sink << "\n--" << boundary << "\n";

	for (;;)
	{
		c=getc(f);
		if (i >= sizeof(buffer) || (i > 0 && c == EOF))
		{
			sink << std::string(buffer, buffer+i);

			byteCount += i;
			i=0;

			myServer::reportProgress(byteCount, 0,
						 partNum, nparts);
		}

		if (c == EOF)
			break;

		buffer[i++]=c;

	}

	myServer::reportProgress(byteCount, byteCount, partNum, nparts);
}

void CursesEdit::autosave()
{
	statusBar->clearstatus();
	statusBar->status(_("Autosaving..."));
	autosave_int();
}

// Autosave message.txt

void CursesEdit::autosave_int()
{
	SaveSinkAutosave autoSaveFile;

	autoSaveFile.savefilename=myServer::getConfigDir() + "/message.tmp";

	unlink(autoSaveFile.savefilename.c_str());
	autoSaveFile.savefile.open(autoSaveFile.savefilename.c_str());

	bool good=false;

	if (autoSaveFile.savefile.is_open())
	{
		if (save(autoSaveFile, true, false, true))
		{
			autoSaveFile.savefile.flush();
			autoSaveFile.savefile.close();

			std::string txtname=myServer::getConfigDir()
				+ "/message.txt";

			if (autoSaveFile.savefile.good() &&
			    rename(autoSaveFile.savefilename.c_str(),
				   txtname.c_str()) == 0)
			{
				statusBar->status("");
				statusBar->progress("");
				good=true;
			}
			else
			{
				statusBar->clearstatus();
				statusBar->status(strerror(errno),
						  statusBar->SYSERROR);
			}
		}
	}
	else
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno), statusBar->SERVERERROR);
	}
	if (!good)
		statusBar->beepError();
}

///////////////////////////////////////////////////////////////////////////

static int encode_callbackSink(const char *p, size_t cnt, void *vp)
{
	(*((CursesEdit::SaveSink *)vp))	<< std::string(p, p+cnt);
	return 0;
}

bool CursesEdit::save(SaveSink &sink,
		      bool saveAllHeaders,
		      // True: autosave, else false
		      // If true, all X- headers are preserved

		      bool saveAllAttachments,
		      // False: autosave, else true
		      // If true, all attachments are assembled, and included.

		      bool saveFcc
		      // If true, save the X-Server/X-Folder/X-UID headers.
		      // It's true when postponing to drafts, or autosaving.
		      )

{
	std::string oldfile=myServer::getConfigDir() + "/message.txt";


	// Grab some existing headers

	{
		std::ifstream i(oldfile.c_str());

		if (!i.is_open())
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno),
					  statusBar->SYSERROR);
			return false;
		}

		bool keepHeader=false;

		while (!i.eof())
		{
			std::string line="";

			getline(i, line);

			if (line.size() == 0)
				break;

			if (!unicode_isspace((unsigned char)line[0]))
			{
				size_t p=line.find(':');
				keepHeader=false;

				// Begin a new header.  Assume don't keep it,
				// until we change our mind.


				if (p != std::string::npos)
				{
					std::string h=line.substr(0, p);

					if (strcasecmp(h.c_str(), "References")
					    == 0 ||
					    (saveAllHeaders &&
					     strncasecmp(h.c_str(), "X-", 2)
					     == 0))
						keepHeader=true;

					if (strcasecmp(h.c_str(), "X-Fcc")
					    == 0)
						keepHeader=false; // Changed?


					if (strcasecmp(h.c_str(), "X-Server")
					    == 0 ||
					    strcasecmp(h.c_str(), "X-Folder")
					    == 0 ||
					    strcasecmp(h.c_str(), "X-UID")
					    == 0)
					{
						if (saveFcc)
							keepHeader=true;
					}
				}
			}

			if (keepHeader)
				sink << line << "\n";
		}

		// Manufacture a suitable Message-ID: header.

		struct timeval tv;

		gettimeofday(&tv, NULL);

		std::string h;

		{
			std::ostringstream o;

			o << "cone." << tv.tv_sec << '.' << tv.tv_usec << '.'
			  << getpid() << '.' << getuid() << "@"
			  << mail::hostname();
			h=o.str();
		}

		sink << mail::Header::encoded("Message-ID",
					      "<" + h + ">",
					      unicode_default_chset())
			.toString()
		     << "\nX-Mailer: http://www.courier-mta.org/cone/\n";
	}

	std::string xfcc=getFcc(saveFcc ? &sink:NULL);

	// Save envelope headers.

	std::string fromhdr;
	std::string replytohdr;
	std::string customhdr;
	std::string charset= unicode_default_chset();
	size_t nparts;

	saveheaders(fromhdr, replytohdr, customhdr, &sink);

	char datebuf[200];

	rfc822_mkdate_buf(time(NULL), datebuf);

	if (newsgroups)
		newsgroupsV=newsgroups->getText();

	if (newsgroupsV.size() > 0)
		sink << mail::Header::encoded("Newsgroups", newsgroupsV,
					      charset).toString()
		     << "\n";

	if (followupto)
		followuptoV=followupto->getText();

	if (followuptoV.size() > 0)
		sink << mail::Header::encoded("Followup-To", followuptoV,
					      charset).toString()
		     << "\n";

	sink << mail::Header::encoded("Subject", subject.getText(),
				      charset).toString()
	     << "\nDate: " << datebuf << "\nMime-Version: 1.0\n";


	FILE *fp2=::tmpfile();

	//
	// Progress reporting.
	//
	// 1 pass to convert and save edited text to a temp file.
	//
	// 1 pass to compute multipart boundary separator
	//   ... if saveAllAttachments and there are attachments
	// 1 pass for each attachment to mime-ify it

	nparts=1;
	if (attachmentList.size() > 0 && saveAllAttachments)
	{
		nparts += attachmentList.size()+1;
	}

	if (!fp2)
	{
		statusBar->clearstatus();
		statusBar->status(strerror(errno),
				  statusBar->SYSERROR);
		return false;
	}

	// First, save the message's contents to a temp file.

	size_t saveByteCount=0;
	for (size_t lineNum=0, nLines=message.numLines();
	     lineNum < nLines; lineNum++)
	{
		std::string s=mail::iconvert::convert
			(message.getUTF8Text(lineNum, true),
			 "utf-8",
			 unicode_default_chset());

		const char *p=s.c_str();

		saveByteCount += strlen(p)+1;

		fprintf(fp2, "%s\n", p);
		myServer::reportProgress(saveByteCount, 0, 0, nparts);
	}

	myServer::reportProgress(saveByteCount, saveByteCount, 0, nparts);

	if (fflush(fp2) || ferror(fp2) || fseek(fp2, 0L, SEEK_SET) < 0)
	{
		fclose(fp2);
		statusBar->clearstatus();
		statusBar->status(strerror(errno),
				  statusBar->SYSERROR);
		return false;
	}

	// Compute a multipart boundary

	std::string mp_boundary;
	size_t mp_cnt=0;

	saveByteCount=0;

	for (;;)
	{
		mp_boundary="=_";
		mp_boundary += mail::hostname();
		mp_boundary += "-";

		std::string mpbuf;

		{
			std::ostringstream fmt;

			fmt << time(NULL) << "-"
			    << std::setw(4) << std::setfill('0')
			    << mp_cnt++;
			mpbuf=fmt.str();
		}
		mp_boundary += mpbuf;

		if (fseek(fp2, 0L, SEEK_SET) < 0)
		{
			fclose(fp2);
			statusBar->clearstatus();
			statusBar->status(strerror(errno),
					  statusBar->SYSERROR);
			return false;
		}

		if (search_boundary(fp2, mp_boundary, saveByteCount, nparts))
			continue;

		size_t c;

		for (c=0; c<attachmentList.size(); c++)
		{
			if (attachmentList[c]->getEncoding() == "BASE64")
				continue;

			FILE *z=fopen(attachmentList[c]->getFilename().c_str(),
				      "r");

			if (!z)
			{
				fclose(fp2);
				statusBar->clearstatus();
				statusBar->status(strerror(errno),
						  statusBar->SYSERROR);
				return false;
			}

			if (search_boundary(z, mp_boundary, saveByteCount,
					    nparts))
			{
				fclose(z);
				break;
			}

			fclose(z);
		}

		if (c == attachmentList.size())
			break;
	}

	myServer::reportProgress(saveByteCount, saveByteCount, 2, nparts);

	// Compute a suitable encoding method for the message's text.

	const char *encoding;

	if (fseek(fp2, 0L, SEEK_SET) < 0 ||
	    ((encoding=libmail_encode_autodetect_fp(fp2, 0, NULL)),
	     ferror(fp2)) || fseek(fp2, 0L, SEEK_SET) < 0)
	{
		fclose(fp2);
		statusBar->clearstatus();
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		return false;
	}

	if (attachmentList.size() > 0 && saveAllAttachments)
	{
		// We'll be assembling the attachments shortly.

		sink << "Content-Type: multipart/mixed; boundary=\""
		     << mp_boundary << "\"\n\n" RFC2045MIMEMSG
		     << "\n--" << mp_boundary << "\n";
	}

	// Encode message text.

	std::string errmsg="";

	try {
		struct libmail_encode_info encodeInfo;

		sink << "Content-Type: text/plain; format=flowed; delsp=yes; charset=\""
		     << unicode_default_chset()
		     << "\"\n"
			"Content-Disposition: inline\n"
			"Content-Transfer-Encoding: " << encoding << "\n\n";

		libmail_encode_start(&encodeInfo, encoding,
				     &encode_callbackSink, &sink);

		char encodeBuf[BUFSIZ];
		int n=0;

		size_t byteCount=0;

		while (errmsg.size() == 0 &&
		       (n=fread(encodeBuf, 1, sizeof(encodeBuf), fp2)) > 0)
		{
			if (libmail_encode(&encodeInfo, encodeBuf, n))
				errmsg=strerror(errno);

			byteCount += n;
			myServer::reportProgress(byteCount, byteCount,
						 2, nparts);
		}

		if (errmsg.size() == 0)
		{
			if (n < 0 ||
			    libmail_encode_end(&encodeInfo) < 0)
				errmsg=strerror(errno);
		}

		fclose(fp2);
	} catch (...) {
		fclose(fp2);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	if (errmsg.size() > 0)
	{
		statusBar->clearstatus();
		statusBar->status(errmsg, statusBar->SYSERROR);
		return false;
	}

	if (attachmentList.size() == 0 || !saveAllAttachments)
	{
		// We don't want to save attachments.

		return true;
	}

	// Append attachments.

	size_t c;

	for (c=0; c<attachmentList.size(); c++)
	{
		FILE *z=fopen(attachmentList[c]->getFilename().c_str(), "r");

		if (!z)
		{
			fclose(fp2);
			statusBar->clearstatus();
			statusBar->status(errmsg, statusBar->SYSERROR);
			return false;
		}

		copy_multipart(z, sink, mp_boundary, 3 + c, nparts);
	}

	sink << "\n--" << mp_boundary << "--\n";
	return true;
}

// Generate an X-Fcc: header.

std::string CursesEdit::getFcc(SaveSink *sink)
{
	std::string xfcc="";

	if (sentFolderChoice)
		defaultSentChoice=sentFolderChoice->getSelectedOption();

	if (defaultSentChoice > 0)
	{
		SpecialFolder &sentFolder=
			SpecialFolder::folders.find(SENT)->second;

		size_t n= defaultSentChoice-1;

		if (n < sentFolder.folderList.size())
			xfcc=sentFolder.folderList[n].nameUTF8;

		if (xfcc.size() > 0 && sink)
		{
			(*sink) << "X-Fcc: "
				<< (std::string)mail::rfc2047::encode(xfcc, "UTF-8")
				<< "\n";
		}
	}

	return xfcc;
}

// Make sure that all RFC 822 headers are syntactically valid.

bool CursesEdit::checkheaders()
{
	AddressList *screenHdrs[]={from, to, cc, bcc, replyto};
	size_t i;

	for (i=0; i<sizeof(screenHdrs)/sizeof(screenHdrs[0]); i++)
	{
		if (screenHdrs[i] && !screenHdrs[i]->validateAll())
			return false;
	}

	return true;
}

// Save sender/recipient fields.
//
// As a side effect, initialize the recipients and sender member objects,
// which we want to know in advance before sending a message.

void CursesEdit::saveheaders(std::string &fromhdr, std::string &replytohdr,
			     std::string &customhdr,
			     SaveSink *sink)
{
	AddressList *screenHdrs[]={from, to, cc, bcc, replyto};
	std::vector<mail::emailAddress> *saveBufs[]={&fromV, &toV, &ccV, &bccV,
					   &replytoV};
	std::string headerNames[]={"From", "To", "Cc", "Bcc",
			      "Reply-To"};

	recipients.clear();
	sender="";
	customhdr="";

	size_t i;

	// Take each header's actual contents, if it's shown.  If it's
	// hidden, go to its buffer.

	std::string charset= unicode_default_chset();

	for (i=0; i<sizeof(headerNames)/sizeof(headerNames[0]); i++)
	{
		std::vector<mail::emailAddress> addressArray;

		if (screenHdrs[i])
		{
			screenHdrs[i]->getAddresses(addressArray);
		}
		else
			addressArray= *saveBufs[i];

		if (addressArray.size() == 0)
			continue;

		size_t j;

		for (j=0; j<addressArray.size(); j++)
		{
			if (i == 0 && sender.size() == 0)
				sender=addressArray[j].getAddr();

			if (i == 1 || i == 2 || i == 3) // To, Cc, Bcc
			{
				if (addressArray[j].getAddr().size() > 0)
					recipients.push_back(addressArray[j]
							     .getAddr());
			}
		}

		if (i == 0)
			fromhdr=mail::address::toString("", addressArray);

		if (i == 4)
			replytohdr=mail::address::toString("", addressArray);

		if (sink)
			(*sink) << mail::Header::addresslist(headerNames[i],
							     addressArray)
				.toString()
				<< "\n";
	}

	// Also write out custom headers, there.

	std::vector<CustomHeader *>::iterator cb, ce;

	cb=customHeaders.begin();
	ce=customHeaders.end();

	while (cb != ce)
	{
		CustomHeader *ch= *cb++;

		std::string v=ch->hiddenValue;

		if (ch->field)
			v=ch->field->getText();

		if (v.size() > 0)
		{
			std::string h=ch->name + " "
				+ (std::string)mail::rfc2047::encode(v, charset);

			if (sink)
				(*sink) << h << "\n";

			if (customhdr.size() > 0)
				customhdr += "\n";
			customhdr += h;
		}
	}

}

//
// After saving a message, automatically memorize the from and reply-to
// headers so that they be the default next time.

void CursesEdit::saved(std::string fromhdr,
		       std::string replytohdr,
		       std::string xfcc, std::string customhdr,
		       std::string signKey)
{
	if (xserver.size() == 0)
		return;

	if (signKey.size() == 0)
		signKey="[]";

	std::string::iterator b, e;

	for (b=fromhdr.begin(), e=fromhdr.end(); b != e; b++)
		if (*b == '\n')
			*b=' ';

	for (b=replytohdr.begin(), e=replytohdr.end(); b != e; b++)
		if (*b == '\n')
			*b=' ';

	myServer *s=myServer::getServerByUrl(xserver);

	if (!s)
		return;

	xfcc=mail::rfc2047::encode(xfcc, "UTF-8");

	if (xfcc == "")
		xfcc="=?UTF-8?" "?=";  // Heh -- trigraph warning :-)


	bool doUpdate=false;

	const char * const fldnames[]=
		{"FROM", "REPLY-TO", "FCC", "CUSTOM", NULL};
	const std::string fldvals[]=
		{fromhdr, replytohdr, xfcc, customhdr};

	size_t i;

	myServer::promptInfo memorizePrompt(_("Memorize new headers in this message? (Y/N) "));

	memorizePrompt.yesno();

	if (xfolder.size() > 0)
	{
		for (i=0; fldnames[i]; i++)
		{
			if (s->getFolderConfiguration(xfolder, fldnames[i])
			    != fldvals[i])
			{
				memorizePrompt=
					myServer::prompt(memorizePrompt);

				if (memorizePrompt.abortflag)
					return;

				if ((std::string)memorizePrompt != "Y")
					return;

				for (i=0; fldnames[i]; i++)
				{
					s->updateFolderConfiguration
						(xfolder, fldnames[i],
						 fldvals[i]);
				}
				doUpdate=true;
				break;
			}
		}

		if (s->getFolderConfiguration(xfolder, "SIGNKEY") != signKey)
		{
			if (!doUpdate) // Else already prompted, above
				memorizePrompt=
					myServer::prompt(memorizePrompt);

			if (memorizePrompt.abortflag)
				return;

			if ((std::string)memorizePrompt != "Y")
				return;

			s->updateFolderConfiguration(xfolder, "SIGNKEY",
						     signKey);
			doUpdate=true;
		}
	}

	memorizePrompt=
		myServer::promptInfo(_("Memorize headers in this message "
				       "as account defaults? (Y/N) ")).yesno();

	if (doUpdate)
		myServer::saveconfig();
	doUpdate=false;

	for (i=0; fldnames[i]; i++)
	{
		if (s->getServerConfiguration(fldnames[i]) != fldvals[i])
		{
			memorizePrompt=myServer::prompt(memorizePrompt);

			if (memorizePrompt.abortflag)
				return;

			if ((std::string)memorizePrompt != "Y")
				return;

			for (i=0; fldnames[i]; i++)
			{
				s->updateServerConfiguration(fldnames[i],
							     fldvals[i]);
			}
			doUpdate=true;
			break;
		}
	}

	if (s->getServerConfiguration("SIGNKEY") != signKey)
	{
		if (!doUpdate) // Else already prompted, above
			memorizePrompt=
				myServer::prompt(memorizePrompt);

		if (memorizePrompt.abortflag)
			return;

		if ((std::string)memorizePrompt != "Y")
			return;

		s->updateServerConfiguration("SIGNKEY", signKey);
		doUpdate=true;
	}

	if (doUpdate)
		myServer::saveconfig();
}

void editScreen(void *dummy)
{
	CursesEdit edit_screen(mainScreen);

	titleBar->setTitles(_("EDIT MESSAGE"), "");

	edit_screen.init();
	myServer::eventloop();
}
