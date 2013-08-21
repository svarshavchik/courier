/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imaphandler_H
#define libmail_imaphandler_H

#include "libmail_config.h"
#include "imap.H"

LIBMAIL_START

//////////////////////////////////////////////////////////////////////////
//
// Something that tokenizes server response.
//

class imapHandlerStructured : public imapHandler {

	int errormode;
	int donemode;
	int stringmode;

	size_t largestringleft;
	std::string errbuf;
	std::string stringbuffer;

protected:
	size_t largestringsize;
public:
	imapHandlerStructured(int timeoutVal=60)
		: imapHandler(timeoutVal), errormode(0),
		  donemode(0), stringmode(0), largestringleft(0)
	{
	}

	enum TokenType {
		EOL=256,
		ATOM,
		STRING,
		LARGESTRING,  // Another LARGESTRING, or STRING will follow
		NIL
	};

	class Token {
	public:
		int token;
		std::string text;

		Token(enum TokenType t, std::string s) : token(t), text(s) {}
		Token(int t, std::string s) : token(t), text(s) {}
		Token(enum TokenType t) : token((int)t) {}
		Token(int t) : token(t) {}

		operator int() { return token; }

		bool operator== (int t) { return t == token; }
		bool operator== (enum TokenType t) { return (int)t == token; }
		bool operator!= (int t) { return t != token; }
		bool operator!= (enum TokenType t) { return (int)t != token; }

		operator std::string();
	};

	virtual void error(imap &);
	virtual void done();
protected:

	virtual int process(imap &imapAccount, std::string &buffer);
private:
	virtual void process(imap &imapAccount, Token t)=0;

	virtual bool wantLargeString();

	// Subclass should return true if it's willing to accept {nnn}\r\n
	// piecemeal, using LARGESTRING tokens.
	// Otherwise, any {nnn}\r\n is converted to a string (up to some
	// reasonable limit)


};

LIBMAIL_END

#endif
