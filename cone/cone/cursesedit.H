/*
** Copyright 2003-2006, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesedit_H
#define cursesedit_H

///////////////////////////////////////////////////////////////////////////////
//
// The "Write Message" screen.  We've got all sorts of fun stuff here:
//
// * A bunch of fields for the mail headers.
//
// * A CursesEdit field -- the main message body.
//
// This object does everything needed to write a message, except for the
// actual text editing stuff: reading/writing the message, spell checking,
// sending the message, autosaving, attachment handling...
 

#include "config.h"
#include "libmail/mail.H"
#include "libmail/structure.H"
#include "cursesaddresslist.H"
#include "curses/curseslabel.H"
#include "curses/cursesbutton.H"
#include "curses/curseschoicebutton.H"
#include "curses/cursesfield.H"
#include "curses/curseskeyhandler.H"
#include "curses/timer.H"

#include "cursesmessage.H"
#include "curseseditmessage.H"
#include "specialfolder.H"
#include "myserver.H"

#include <vector>
#include <string>
#include <iostream>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

namespace mail {
	class mimestruct;
}

class CursesEdit : public CursesContainer, public CursesKeyHandler {

	// Subclass CursesEditMessage to trap text modifications
	// (used to trigger autosave)

	class CursesEditMessageClass : public CursesEditMessage {
	public:
		CursesEdit *myEdit;

		CursesEditMessageClass(CursesContainer *);
		~CursesEditMessageClass();

		void modified();
		std::string getConfigDir();
		void extedited();
		void macroDefined();
	};

	bool goodexit; // Graceful cleanup, no need to autosave

	// Subclass CursesAddressList, implement address book lookups

	class AddressList : public CursesAddressList {
		CursesEdit *parent;
	public:
		AddressList(CursesEdit *parentArg);
		~AddressList();

		void resized();
		bool addressLookup(std::vector<mail::emailAddress> &in,
				   std::vector<mail::emailAddress> &out);
	};

	// Existing attachments:

	class AttachmentButton : public CursesButton {
		CursesEdit *parent;
		std::string filename;
		std::string encoding;
	public:
		AttachmentButton(CursesEdit *parentArg,
				 std::string filenameArg,
				 std::string name,
				 std::string encodingArg);
		~AttachmentButton();

		bool processKeyInFocus(const Curses::Key &key);
		std::string getFilename() { return filename; }
		std::string getEncoding() { return encoding; }
	};

	// Various fields

	AddressList *to, *cc;
	CursesField *newsgroups;
	CursesField *followupto;
	AddressList *from, *bcc, *replyto;
	CursesField subject;

	CursesLabel *sentFolderLabel;
	CursesChoiceButton *sentFolderChoice;
	size_t defaultSentChoice; // Fcc folder by default.

	// New attachment:

	CursesLabel attLabel;
	CursesFieldRedirect<CursesEdit> newAttachment;

	// Existing attachments:

	std::vector<AttachmentButton *> attachmentList;

	// More stuff

	CursesLabel *toLabel, *ccLabel, *newsgroupsLabel, *followuptoLabel,
		subjectLabel, *fromLabel, *bccLabel, *replytoLabel;

	// Vanity headers:

	class CustomHeader {
	public:
		CustomHeader(std::string textArg);
		~CustomHeader();

		CursesLabel *label;
		CursesField *field;
		bool isHidden;
		std::string name;
		std::string hiddenValue;
	};

	std::vector<CustomHeader *> customHeaders;

	CursesEditMessageClass message; // The message text itself.

	bool newsgroupMode; // True: newsgroup-style headers.

	// Places to save contents of headers that are not displayed:

	std::vector<mail::emailAddress> fromV, toV, ccV, bccV, replytoV;
	std::string newsgroupsV;
	std::string followuptoV;

	void addressResized();

	void addAttachment();
	void addAttachment(std::vector<std::string> &filename, std::string mimeContentType);
	void delAttachment(std::string filename);

	TimerRedirect<CursesEdit> autosaveTimer;
	// autosave, at regular intervals

	void autosave();
	void autosave_int();

	// autoarchive sent folder:
	bool archiveSentFolder(SpecialFolder::Info *,
			       mail::folder *);
	std::string createArchiveDir(mail::folder *parentFolder,
				     std::string subdir,
				     myServer::CreateFolderCallback &);
	void renameArchiveFolder(mail::folder *sentFolder,
				 mail::folder *newParent,
				 struct tm lastMonthTm);

	bool postpone(class mail::smtpInfo *, class mail::folder *,
		      class mail::folder *,
		      CursesMessage::EncryptionInfo *);
	void markreplied();

	void restartAutosaveTimer();

public:

	static std::string attach(std::string filename,
				  std::string description,
				  std::string disposition,
				  std::string contentType,
				  mail::mimestruct &fakestructure,
				  size_t attNum,
				  size_t attTotal);

	CursesEdit(CursesContainer *parent);
	~CursesEdit();

	int getWidth() const;

	void init();

	void requestFocus();
	bool isFocusable();

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);
	void draw();
	void erase();

	class SaveSink;
	class SaveSinkAutosave;
	class SaveSinkFile;
	class EncryptSinkFile;

	friend class SaveSinkFile;

	bool save(SaveSink &sink,
		  bool saveAllHeaders, bool saveAllAttachments,
		  bool saveFcc);

private:
	std::string getFcc(SaveSink *sink);

	std::string sender;		   // Initialized by save()
	std::vector<std::string> recipients; // Initialized by save()

	std::string xserver, xfolder, xuid; // Initialized by save()

	bool getPostFolder(mail::folder *&,
			   CursesMessage::EncryptionInfo &);
	bool checkSendFolder(mail::folder *,
			     mail::smtpInfo *&,
			     SpecialFolder::Info *,
			     mail::folder *,
			     CursesMessage::EncryptionInfo &);

	void saved(std::string fromhdr, std::string tohdr,
		   std::string xfcc, std::string customhdr,
		   std::string key);

	bool checkheaders();

	void saveheaders(std::string &fromhdr, std::string &replytohdr,
			 std::string &customhdr, SaveSink *saveSink);

	// When marking a message as replied to, need a dummy folder callback
	// object if need to open a separate connection for this.

	class dummyFolderCallback : public mail::callback::folder {

	public:
		dummyFolderCallback();
		~dummyFolderCallback();

		void newMessages();
		void messagesRemoved(std::vector< std::pair<size_t,
				     size_t> > &list);
		void messageChanged(size_t n);
	};
};

#endif
