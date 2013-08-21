/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesmessage_H
#define cursesmessage_H

//////////////////////////////////////////////////////////////////////////
//
// A message that's shown on a curses screen
//

#include "config.h"
#include "mymessage.H"
#include "curses/mycurses.H"
#include "curses/timer.H"
#include "libmail/rfcaddr.H"
#include <unistd.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "libmail/objectmonitor.H"
#include "cache.H"
#include "previousscreen.H"

namespace mail {
	class account;
	class envelope;
	class mimestruct;
}

class CursesMessageDisplay;
class Curses;

#define GPGKEYMIMETYPEPART "application"
#define GPGKEYMIMESUBTYPEPART "pgp-keys"
#define GPGKEYMIMETYPE GPGKEYMIMETYPEPART "/" GPGKEYMIMESUBTYPEPART

//////////////////////////////////////////////////////////////////
//
// A message is displayed on the screen in two steps:
//
// 1) The shown[] array is created, listing the parts of the message suitable
// for display.  Each part is saved in a temporary file.
//
// 2) After all parts are downloaded, the parts are reformatted to display
// ( translated into the local character set, linewrapped, etc).
//
// The second step is performed in the background, and the display is
// updated as the message is being reformatted.  If the display is resized,
// the reformatting is restarted.
//
// The text is reformatted into another temporary file, according to the
// current display width.  Yet another temporary file serves as an index
// to each line being shown.

class textAttributes;

class CursesMessage : public myMessage, public mail::obj,
		      public PreviousScreen {

public:
	CursesMessage(mail::account *,
		      void (myMessage::*completedFuncArg)());
	~CursesMessage();

	// Initialize display. mimeid: "" to display the entire message,
	// else ID of an attached message

	bool init(std::string mimeId,
		  bool allowExternalOpenArg);

	bool isShownMimeId(std::string id);
	// True if this attachment is shown now.

	void beginReformat(size_t w); // Begin step 2.

	size_t nLines(); // Number of lines in the message

	void getLineImage(size_t lineNum,
			  std::vector<std::pair<textAttributes, std::string>
			  > &line);
	// Returns the "image" of line #lineNum, a list of
	// attribute/text pairs.

	void drawLine(size_t lineNum,
		      Curses *window,
		      size_t windowLineNum);

	std::string shownMimeId; // Non "" if showing an attached message.
private:
	bool hasMimeId; // True after initialization.
	bool allowExternalOpen; // Allow external filters.

	// The display layout consists of headers and content.

	class Shownpart {
	public:
		Shownpart();
		~Shownpart();

		mail::envelope *envelope;  // Show this envelope, or
		mail::mimestruct *structure; // Show this part

		bool filtered_content; // External filter

		std::string disposition; // Usually structure->content_disposition

		// The above point to myMessage fields

		std::string contents;
		// Filename of downloaded contents.

		std::string content_chset;
		// The charset in contents

		std::string contents2;
		// Sometimes we need a second filename.

		void getflowed(bool &flowed, bool &delsp) const;
	};

	class newmsgformatter;
public:
	// Helper class used for reading a message part.

	class SaveText : public myMessage::ReadText {
		std::ostream &o;
	public:
		SaveText(std::ostream &o);
		virtual ~SaveText();

		virtual void operator()(std::string text);
	};

	// Helper class for prompting for encryption info

	class EncryptionInfo {
	public:
		EncryptionInfo();
		~EncryptionInfo();

		std::string signKey;
		std::vector<std::string> encryptionKeys;

		std::string passphrase;
		std::vector<std::string> otherArgs;

		bool isUsing() const
		{
			return encryptionKeys.size() > 0 ||
				signKey.size() > 0;
		}
	};

private:

	std::vector<Shownpart> shown; // Shown message parts.

	void grok(mail::mimestruct &, std::string);
        bool (CursesMessage::* getHandler(mail::mimestruct &))(size_t);

	bool readTextPlain(size_t n); // Part #n is text/plain content.
	bool filterExternal(size_t n); // Sent part #n to external filter.

	std::string getFilename(size_t n, std::string suffix);

	// Second step

public:
	mail::envelope *getEnvelope()
	{
		return shown.size() == 0 ? NULL:
			shown[0].envelope;
	}

	//
	// The current message may or may not be displayed on the screen
	// When the current message is shown, currentDisplay is initialized
	// to non NULL (by CursesMessageDisplay::CurrentMessageDisplay(),
	// and cleared by its destructor)
	//

	static CursesMessageDisplay *currentDisplay;

	static bool getBounceTo(class mail::smtpInfo &smtpInfo);
	// Get list of addys to bounce msg to

	static bool getSendInfo(std::string promptStr,
				std::string promptStr2,
				class mail::smtpInfo &,
				CursesMessage::EncryptionInfo *);
	static bool getSendInfo2(std::string promptStr,
				std::string promptStr2,
				class mail::smtpInfo &,
				CursesMessage::EncryptionInfo *);
	// Get SMTP parameters (DSN, etc...)
	// EncryptionInfo: if not NULL prompt for, and fill in, encryption info


	// Bits and pieces of encryption support:

	static void initEncryptInfo(CursesMessage::EncryptionInfo *&encryptInfo,
				    std::vector<std::string> &saveEncryptionKeys);

	static void initEncryptSign(CursesMessage::EncryptionInfo *encryptInfo);
	static void initEncryptEncrypt(CursesMessage::EncryptionInfo *encryptInfo,
				       std::vector<std::string> &saveEncryptionKeys);
	static bool setEncryptionOptions(CursesMessage::EncryptionInfo *encryptInfo);

	static mail::folder *getSendFolder(mail::smtpInfo &, mail::account *&,
					   mail::folder **,
					   class mail::callback::disconnect &);

	class ReformatTimer : public Timer {
	public:
		CursesMessage *message;

		ReformatTimer();
		~ReformatTimer();
		void alarm();
	};
private:
	ReformatTimer reformatTimer;

	class LineIndex {
	public:
		LineIndex();
		~LineIndex();

		std::streampos whence;
		size_t nbytes;

		enum Flags {
			NORMAL=0,
			ATTRIBUTES=1
		} flags;

	};

	Cache<size_t, std::pair<LineIndex::Flags, std::string> > lineCache;

	size_t displayWidth;

	FILE *displayFile; // I'd love to fstream, but I must have access
	FILE *indexFile;   // to the file descriptor.  Bitch at g++.

	size_t nlines;

	class FlowedTextParser;
	class HtmlParser;

	class reformatter {
	public:
		reformatter();
		virtual ~reformatter();

		virtual void parse(std::string text)=0;
		virtual bool finish()=0;
		virtual bool conversionError()=0;
	};

	reformatter *textReformatter;
	bool conversionErrorFlag;

	size_t reformat_index;	// Current section being reformatted
	std::ifstream reformat_file; // Current file being reformatted.
	std::string reformat_buffer; // Misc buffer
	std::streampos reformat_whence;
	void (myMessage::*reformatCompletedFunc)();

	bool reformat();

public:
	friend class myRfc2646Parser;

	bool reformatting() { return reformat_index < shown.size(); }

	static bool fullEnvelopeHeaders;

	bool rot13;

	static void getDescriptionOf(mail::mimestruct *mime,
				     mail::envelope *env,
				     std::string &name,
				     std::string &filename,
				     bool showEncoding);
	void reply();
	void forward();

	bool decrypt(std::string passphrase, std::vector<std::string> &opts,
		     bool &decryptFailed);

private:
	bool reformatHeader(std::string hdr, bool rfc2047Encoded);
	bool reformatLine(std::string hdr);
	void reformatAddLines(std::vector<std::string> &lines);
	void reformatFindUrls(std::vector<std::string> &lines,
			      textAttributes normalAttrs);

	bool reformatEnvelopeAddresses(std::string hdrName,
				       std::vector<mail::address> &addresses);

	bool reformatAddLine(std::string l,
			     CursesMessage::LineIndex::Flags
			     flags=CursesMessage::LineIndex::NORMAL);
	bool addHeader(std::string name, std::string value,
		       bool rfc2047Encoded);

	bool toMyCharset(const std::string &content_chset,
			 const std::string &my_chset,
			 std::string &line,
			 bool &errflag);

	bool toMyCharset_noerror(const std::string &content_chset,
				 const std::string &my_chset,
				 std::string &line,
				 bool &errflag);

	bool toMyCharset_impl(const std::string &content_chset,
			      const std::string &my_chset,
			      std::string &line,
			      bool &errflag);

	mail::mimestruct *decrypt_find(mail::mimestruct &m, std::string );

	class DecryptSaveText;


	/* --- cached links --- */

	class link { // A link on a given row
	public:
		size_t column;
		std::string url;
	};

	std::map<size_t, std::list<link> > links;

	size_t linkRow;
	std::list<link>::iterator linkPos;

	// Map keyed by row number, value is the links on the row

public:
	void clearlinks()
	{
		linkRow=(size_t)-1;
		links.clear();
	}

	bool hasLink()
	{
		return linkRow != (size_t)-1;
	}

	bool getCurrentLink(size_t &row, size_t &col, std::string &url)
	{
		if (hasLink())
		{
			row=linkRow;
			col=linkPos->column;
			url=linkPos->url;
			return true;
		}
		return false;
	}

	bool nextLink();
	bool prevLink();
	void toLastLink();
};

#endif
