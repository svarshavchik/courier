/*
** Copyright 2003-2005, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"

#include "myserver.H"
#include "myserverpromptinfo.H"
#include "myservercallback.H"
#include "myfolder.H"
#include "globalkeys.H"
#include "cursesmessage.H"
#include "cursesattachmentdisplay.H"
#include "gettext.H"
#include "savedialog.H"
#include "gpg.H"
#include "gpglib/gpglib.h"
#include "curses/cursesmainscreen.H"
#include "curses/cursestitlebar.H"
#include "curses/cursesstatusbar.H"
#include "libmail/structure.H"
#include "libmail/objectmonitor.H"
#include "libmail/addmessage.H"

#include <errno.h>
#include <fstream>

extern Gettext::Key key_PRIVATEKEY;

extern Gettext::Key key_SAVE;
extern Gettext::Key key_FOLDERINDEX;
extern Gettext::Key key_LESSTHAN;
extern Gettext::Key key_DELETE;
extern Gettext::Key key_UNDELETE;
extern Gettext::Key key_EXPUNGE;

extern CursesMainScreen *mainScreen;
extern CursesTitleBar *titleBar;
extern CursesStatusBar *statusBar;

extern void folderIndexScreen(void *);
extern void hierarchyScreen(void *);
extern void showCurrentMessage(void *);

//
// A button representing an attachment.
//
// Does something intelligent when ENTER or S is pressed.
//

CursesAttachmentDisplay::Attachment::Attachment(CursesAttachmentDisplay *p,
						mail::mimestruct *mimeArg,
						mail::envelope *envArg,
						std::string nameArg)
	: CursesButton(p, nameArg),
	  parent(p),
	  name(nameArg),
	  mime(mimeArg),
	  env(envArg),
	  deleted(false)
{
}

CursesAttachmentDisplay::Attachment::~Attachment()
{
}

void CursesAttachmentDisplay::Attachment::markDeleted(bool flag)
{
	deleted=flag;
	setText(deleted ? _("--- Deleted ---"): name);
}

void CursesAttachmentDisplay::Attachment::clicked()
{
	if (mime != NULL || env)
		parent->open(mime);
}

bool CursesAttachmentDisplay::Attachment::processKeyInFocus(const Curses::Key
							    &key)
{
	if (key == key_SAVE)
	{
		parent->download(this);
		return true;
	}

	if (key == key_DELETE)
	{
		if (mime && !mime->messagerfc822())
			markDeleted(true);
		return true;
	}

	if (key == key_UNDELETE)
	{
		if (mime && !mime->messagerfc822())
			markDeleted(false);
		return true;
	}

	return CursesButton::processKeyInFocus(key);
}

CursesAttachmentDisplay::CursesAttachmentDisplay(CursesContainer *parent,
				    CursesMessage *messageArg)
	: CursesContainer(parent), CursesKeyHandler(PRI_SCREENHANDLER),
	  messageInfoPtr(messageArg)
{
	int rowNum=0;

	createAttList(NULL, rowNum, 1);
	titleBar->setTitles(_("ATTACHMENTS"), "");
}

// Recursively create a list of attachments.

void CursesAttachmentDisplay::createAttList(mail::mimestruct *mimePtr,
					    int &rowNum, int nestingLevel)
{
	if (mimePtr == NULL || mimePtr->messagerfc822())
	{
		// Top level msg, or a message/rfc822 attachment.

		createMsgAtt(mimePtr,
			     mimePtr ? &mimePtr->getEnvelope():
			     &messageInfoPtr->envelope, rowNum,
			     nestingLevel);

		if (mimePtr)
		{
			if (mimePtr->getNumChildren() == 0)
				return;

			mimePtr=mimePtr->getChild(0);
		}
		else
			mimePtr= &messageInfoPtr->structure;

		++nestingLevel;
	}

	if (mimePtr->getNumChildren() > 0)
	{
		// A multipart attachment.

		CursesLabel *l=new CursesLabel(this,
					       mimePtr->type + "/"
					       + mimePtr->subtype);

		if (!l)
			LIBMAIL_THROW("Out of memory");

		try {
			children.push_back(l);
		} catch (...) {
			delete l;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		l->setRow(rowNum++);
		l->setCol(nestingLevel * 2);
		++nestingLevel;

		size_t i;

		for (i=0; i<mimePtr->getNumChildren(); i++)
			createAttList(mimePtr->getChild(i), rowNum,
				      nestingLevel+1);

		return;
	}

	createMsgAtt(mimePtr, NULL, rowNum, nestingLevel);
}

void CursesAttachmentDisplay::createMsgAtt(mail::mimestruct *mimePtr,
					   mail::envelope *env,
					   int &rowNum, int nestingLevel)
{
	std::string name;
	std::string filename;

	CursesMessage::getDescriptionOf(mimePtr, env, name, filename,
					true);

	Attachment *a=new Attachment(this, mimePtr, env, name);

	if (!a)
		LIBMAIL_THROW("Out of memory.");

	try {
		children.push_back(a);
	} catch (...) {
		delete a;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	a->setRow(rowNum++);
	a->setCol(nestingLevel * 2);
	attachment_list.push_back(a);
}

CursesAttachmentDisplay::~CursesAttachmentDisplay()
{
	std::list<Curses *>::iterator b, e;

	while ( (b=children.begin()) != (e=children.end()))
	{
		Curses *p= *b;

		children.erase(b);
		delete p;
	}
}

int CursesAttachmentDisplay::getWidth() const
{
	return getScreenWidth();
}

int CursesAttachmentDisplay::getHeight() const
{
	return (getParent()->getHeight());
}

bool CursesAttachmentDisplay::isFocusable()
{
	return true;
}

void CursesAttachmentDisplay::requestFocus()
{
	std::list<Curses *>::iterator b, e;

	b=children.begin();
	e=children.end();

	if (b != e)
	{
		(*b)->requestFocus();
		return;
	}

	CursesContainer::requestFocus();
}


bool CursesAttachmentDisplay::processKey(const Curses::Key &key)
{
	CursesMessage *messageInfo=messageInfoPtr;

	if (!messageInfo)
		return false;

	if (GlobalKeys::processKey(key, GlobalKeys::ATTACHMENTSCREEN,
				   messageInfo->myfolder->getServer()))
		return true;

	if (key == key_FOLDERINDEX ||
	    key == key_LESSTHAN)
	{
		keepgoing=false;
		myServer::nextScreen=&folderIndexScreen;
		myServer::nextScreenArg=messageInfo->myfolder;
		return true;
	}

	if (key == key_EXPUNGE)
	{
		std::set<std::string> removeAttachments;
		std::list<Attachment *>::iterator b=attachment_list.begin(),
			e=attachment_list.end();

		while (b != e)
		{
			if ((*b)->deleted && (*b)->mime)
				removeAttachments.insert((*b)->mime->mime_id);
			++b;
		}

		if (removeAttachments.empty())
		{
			statusBar->clearstatus();
			statusBar->status(_("No attachments to expunge."));
			statusBar->beepError();
		}

		myServer::Callback callback;

		mail::addMessage *add=messageInfo->myfolder->getFolder()
			->addMessage(callback);

		if (!add)
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return true;
		}

		myServer::Callback copy_callback;

		size_t dummy;

		add->assembleRemoveAttachmentsFrom(dummy,
						   messageInfo->myfolder
						   ->getServer()->server,
						   messageInfo->myfolder
						   ->getIndex(messageInfo->messagesortednum).uid,
						   messageInfo->structure,
						   removeAttachments,
						   copy_callback);

		if (!myServer::eventloop(copy_callback))
		{
			add->fail(copy_callback.msg);
			myServer::eventloop(callback);
			return true;
		}

		if (!add->assemble())
		{
			add->fail(strerror(errno));
			myServer::eventloop(callback);
			return true;
		}
		myServer *s=messageInfo->myfolder->getServer();

		try
		{
			s->currentFolder->isExpungingDrafts=true;

			add->go();

			if (!myServer::eventloop(callback))
			{
				if (s->currentFolder)
					s->currentFolder->
						isExpungingDrafts=false;
				return true;
			}

			if (s->server)
				s->checkNewMail();
			// Make sure newly-added message is in the folder index.

			if (!messageInfoPtr.isDestroyed() && s->currentFolder)
			{
				// Delete the older message.

				size_t messageNum=messageInfoPtr->messagenum;

				if (s->currentFolder->mymessage)
					delete s->currentFolder->mymessage;

				std::vector<size_t> msgList;

				msgList.push_back(messageNum);

				myServer::Callback removeCallback;

				s->server->removeMessages(msgList,
							  removeCallback);

				myServer::eventloop(removeCallback);

			}
			if (s->currentFolder)
				s->currentFolder->isExpungingDrafts=false;
		} catch (...) {
			if (s->currentFolder)
				s->currentFolder->isExpungingDrafts=false;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		keepgoing=false;
		if (myServer::nextScreen == NULL)
		{
			if (s->currentFolder)
			{
				myServer::nextScreen=&folderIndexScreen;
				myServer::nextScreenArg=s->currentFolder;
			}
			else
			{
				myServer::nextScreen= &hierarchyScreen;
				myServer::nextScreenArg=NULL;
			}

		}
		return true;
	}

	return false;
}

bool CursesAttachmentDisplay::listKeys( std::vector< std::pair<std::string, std::string> > &list)
{
	GlobalKeys::listKeys(list, GlobalKeys::ATTACHMENTSCREEN);

	list.push_back(std::make_pair(Gettext::keyname(_("FOLDERINDEX_K:I")),
				      _("Index")));

	list.push_back(std::make_pair(Gettext::keyname(_("SAVE_K:S")),
				      _("Save Att")));

	list.push_back(std::make_pair(Gettext::keyname(_("ENTER_K:ENTER")),
				      _("View")));

        list.push_back( std::make_pair(Gettext::keyname(_("DELETE_K:D")),
				       _("Delete Att")));

        list.push_back( std::make_pair(Gettext::keyname(_("UNDELETE_K:U")),
				       _("Undelete Att")));

        list.push_back( std::make_pair(Gettext::keyname(_("EXPUNGE_K:X")),
				       _("eXpunge Att")));

	return false;
}

void CursesAttachmentDisplay::download(Attachment *a)
{
	std::string name;
	std::string filename;

	CursesMessage::getDescriptionOf(a->mime, a->env, name, filename,
					false);

	{
		SaveDialog save_dialog(filename);

		save_dialog.requestFocus();
		myServer::eventloop();

		filename=save_dialog;
		mainScreen->erase();
	}

	mainScreen->draw();
	a->requestFocus();

	if (messageInfoPtr.isDestroyed() || filename.size() == 0)
		return;

	downloadTo(messageInfoPtr, a->mime, filename);
}

void CursesAttachmentDisplay::downloadTo(CursesMessage *message,
					 mail::mimestruct *mimePart,
					 std::string filename)
{
	statusBar->clearstatus();
	statusBar->status(_("Downloading..."));

	std::ofstream saveFile(filename.c_str());

	if (saveFile.bad() || saveFile.fail())
	{
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return;
	}

	CursesMessage::SaveText saveText (saveFile);

	if (!(mimePart == NULL ? message->readMessage(saveText):
	      message->readMessageContent(*mimePart, saveText)))
	{
		saveFile.close();
		unlink(filename.c_str());
		return;
	}

	saveFile.flush();
	if (saveFile.bad())
	{
		statusBar->status(strerror(errno), statusBar->SYSERROR);
		statusBar->beepError();
		return;
	}
}

// Helper class for importing GnuPG keys

class CursesAttachmentDisplay::KeyImportHelper : public myMessage::ReadText {

	std::string msg;

	bool errflag;

public:
	KeyImportHelper();
	~KeyImportHelper();

	void operator()(std::string text); // Read decoded key.

	bool finish();

	operator std::string() { return msg; }

private:
	// Also handle callback from libmail_gpg_import()

	static int dump_func(const char *, size_t, void *);
	int dump_func(const char *, size_t);
};

CursesAttachmentDisplay::KeyImportHelper::KeyImportHelper()
	: errflag(false)
{
}

CursesAttachmentDisplay::KeyImportHelper::~KeyImportHelper()
{
}

void CursesAttachmentDisplay::KeyImportHelper::operator()(std::string text)
{
	if (!errflag &&
	    libmail_gpg_import_do(text.c_str(), text.size(),
				  &CursesAttachmentDisplay
				  ::KeyImportHelper::dump_func, this))
		errflag=true;
}

bool CursesAttachmentDisplay::KeyImportHelper::finish()
{
	if (!errflag &&
	    libmail_gpg_import_finish(&CursesAttachmentDisplay
				      ::KeyImportHelper::dump_func, this))
		errflag=true;

	return !errflag;
}

int CursesAttachmentDisplay::KeyImportHelper::dump_func(const char *p,
							size_t n, void *vp)
{
	return ((CursesAttachmentDisplay::KeyImportHelper *)vp)
		->dump_func(p, n);
}

int CursesAttachmentDisplay::KeyImportHelper::dump_func(const char *p,
							size_t n)
{
	msg += std::string(p, p+n);
	return 0;
}


void CursesAttachmentDisplay::open(mail::mimestruct *mime)
{
	if (messageInfoPtr.isDestroyed())
		return;

	if (mime && strcasecmp(mime->type.c_str(), GPGKEYMIMETYPEPART) == 0 &&
	    strcasecmp(mime->subtype.c_str(), GPGKEYMIMESUBTYPEPART) == 0 &&
	    GPG::gpg_installed())
	{
		myServer::promptInfo importPrompt=
			myServer::promptInfo(_("Import this key? (Y/N) "))
			.yesno()
			.option(key_PRIVATEKEY,
				Gettext::keyname(_("PRIVATEKEY_K:P")),
				_("Import private key"));

		importPrompt=myServer::prompt(importPrompt);

		if (messageInfoPtr.isDestroyed() ||
		    importPrompt.abortflag)
			return;

		bool privateKey=false;

		if ((std::string)importPrompt == "N")
			return;

		if (key_PRIVATEKEY == importPrompt.firstChar())
			privateKey=true;

		if (libmail_gpg_import_start("", privateKey ? 1:0))
		{
			statusBar->clearstatus();
			statusBar->status(strerror(errno));
			statusBar->beepError();
			return;
		}

		statusBar->clearstatus();
		statusBar->status(_("Importing key..."));

		KeyImportHelper helper;

		if (!messageInfoPtr->readMessageContent(*mime, helper))
			return;

		bool flag=helper.finish();

		GPG::gpg.init();

		statusBar->clearstatus();
		statusBar->status((flag ? _("Key import succeeded.\n\n")
				   : _("Key import FAILED.\n\n")) +
				  (std::string)helper);

		if (!flag)
			statusBar->beepError();
		return;
	}

	if (messageInfoPtr->init(mime ? mime->mime_id:"", true) &&
	    !messageInfoPtr.isDestroyed())
	{
		Curses::keepgoing=false;
		myServer::nextScreen= &showCurrentMessage;
		myServer::nextScreenArg=(CursesMessage *)messageInfoPtr;
	}

#if 0
	messageInfoPtr->myfolder->goMessage(messageInfoPtr->messagesortednum,
					    mime ? mime->mime_id:"");
#endif
}


