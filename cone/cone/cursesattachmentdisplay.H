/*
** Copyright 2003-2005, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesattachmentdisplay_H
#define cursesattachmentdisplay_H

#include "config.h"
#include "curses/mycurses.H"
#include "curses/cursescontainer.H"
#include "curses/cursesbutton.H"
#include "curses/curseskeyhandler.H"
#include "mymessage.H"
#include "libmail/objectmonitor.H"

#include <list>
#include <string>
#include <vector>

class CursesMessage;

namespace mail {
	class mimestruct;
}

////////////////////////////////////////////////////////////////
//
// A list of message's attachments.  We take a mimestruct, and
// unwrap it.

class CursesAttachmentDisplay : public CursesContainer,
				public CursesKeyHandler {

	mail::ptr<CursesMessage> messageInfoPtr;

	class Attachment;

	std::list<Curses *> children; // Labels created in this container.

	std::list<Attachment *> attachment_list;

	// A non-multipart attachments.

	class Attachment : public CursesButton {

		CursesAttachmentDisplay *parent;
		std::string name;
	public:

		mail::mimestruct *mime;
		mail::envelope *env;
		bool deleted;

		Attachment(CursesAttachmentDisplay *parentArg,
			   mail::mimestruct *mimeArg,
			   mail::envelope *envArg,
			   std::string nameArg);
		~Attachment();

		void clicked();
		bool processKeyInFocus(const Curses::Key &key);

		void markDeleted(bool flag);
	};

	class KeyImportHelper;

public:
	CursesAttachmentDisplay(CursesContainer *parent,
			     CursesMessage *messageArg);
	~CursesAttachmentDisplay();

private:
	void createAttList(mail::mimestruct *mimePtr, int &rowNum,
			   int nestingLevel);

	void createMsgAtt(mail::mimestruct *mimePtr,
			  mail::envelope *env, int &rowNum, int nestingLevel);

public:
	// Inherited from Curses
	int getWidth() const;
	int getHeight() const;

	bool isFocusable();
	void requestFocus();

	// Inherited from CursesKeyHandler

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);

	void open(mail::mimestruct *mimePtr);
	// Open a message/rfc822 attachment

	void download(Attachment *a);
	// Download an attachment.

	static void downloadTo(CursesMessage *, mail::mimestruct *,
			       std::string);
};

#endif
