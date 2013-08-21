/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef mymessage_H
#define mymessage_H

/////////////////////////////////////////////////////////////////////////
//
// The currently-open message.  Only one of these objects exist at one
// time (enforced by the class), per server.
//
// This object only stores the basic message information: envelope,
// structure, and its index number.
//

#include "libmail/envelope.H"
#include "libmail/structure.H"

#include "libmail/mail.H"

#include <string>
#include <vector>
#include <iostream>

class myFolder;

#include "myserver.H"

class myMessage : public mail::callback::disconnect,
		  public mail::callback::folder {

	static myMessage *theMessage; // Only one of these objects can exist.

	mail::account *getMessageSource(myServer::Callback &cb,
					size_t &messageNum);

	mail::ptr<mail::account> serverPtr;

public:

	class FolderUpdateCallback {

	public:
		static FolderUpdateCallback *currentCallback;

		FolderUpdateCallback();
		virtual ~FolderUpdateCallback();

		virtual void folderUpdated()=0;
	};

	myMessage(mail::account *);
	virtual ~myMessage();

	myFolder *myfolder; // Which folder this message's in.

	// tmp: account, holding the decrypted/verified copy of an
	// encrypted/signed message.

	mail::account *tmpaccount;
	bool usingTmpaccount;

private:
	// Subclassed from mail::callback, used by tmpaccount
	// They are no-ops.

	void disconnected(const char *errmsg);
	void servererror(const char *errmsg);

	void newMessages();
	void messagesRemoved(std::vector< std::pair<size_t, size_t> > &);
	void messageChanged(size_t);

public:


	size_t messagenum;  // Message number on the server

	size_t messagesortednum;   // Message sorted position

	void folderResorted();

	virtual bool isShownMimeId(std::string id)=0;

	char orig_status_code; // Message status before it was shown.

	mail::envelope envelope;
	mail::mimestruct structure;
	bool haveAttributes;

	// We may create temporary files to save various contents of the
	// message's sections.  The temporary files are automatically
	// unlinked by the destructor

	std::vector<std::string> tmpfiles;


	bool readAttributes();

	class ReadText {
	public:
		ReadText();
		virtual ~ReadText();

		virtual void operator()(std::string text)=0;
	};

	bool readMessage(mail::mimestruct *s,
			 mail::readMode readType,
			 ReadText &text);

	bool readMessage(ReadText &text);

	bool readMessageContent(mail::mimestruct &s,
			 ReadText &text);

	void copyContentsTo(mail::folder *, myServer::Callback &cb);

	virtual void reply()=0;
	virtual void forward()=0;

	static void createAttFilename(std::string &tmpname, std::string &attname,
			       std::string uniq="");
	static void readAttFiles(std::vector<std::string> &filenameList);

	class ReadAttFilesSort;

	static void clearAttFiles();
	static bool checkInterrupted(bool ignoreDrafts=true);

	static void newMessage(const class mail::folder *folderPtr,
			       myServer *serverPtr,
			       std::string mailtoUrl);

	static void newMessageAddressHdr(std::string addresses,
					 std::string hdrname,
					 std::ostream &o);

	static bool getDefaultHeaders(const class mail::folder *folderPtr,
				      myServer *serverPtr,
				      std::string &from,
				      std::string &replyto,
				      std::string &sentfolder,
				      std::string &customheaders);


	bool isSigned();
	bool isEncrypted();

private:
	static bool isSignedEncrypted(mail::mimestruct &, bool);

};

#endif
