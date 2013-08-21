/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_logininfo_H
#define libmail_logininfo_H

#include	<string>
#include	<map>

#include	"namespace.H"

LIBMAIL_START

class callback;

class fdTLS;

/////////////////////////////////////////////////////////////
//
// Object for common account login information:
//

class loginCallback;

class loginInfo {
public:

	class callbackTarget;

	// Glue to the real callback target:

	class callbackTarget {

	protected:

		loginCallback *currentCallback;

	public:

		callbackTarget();
		virtual ~callbackTarget();

		virtual void loginInfoCallback(std::string)=0;
		virtual void loginInfoCallbackCancel()=0;
	};


	std::string method;		// "imap", "pop3Account", etc...

	std::string server, uid, pwd;	// Login id/password

	std::map<std::string, std::string> options;	// Other options


	// FOR INTERNAL USE:

	loginCallback *loginCallbackFunc;

	bool use_ssl;

	class mail::callback *callbackPtr;
	class fdTLS *tlsInfo;
	// Callback function when login goes through

	loginInfo();
	~loginInfo();
};

////////////////////////////////////////////////////////////////////////////
//
// Application supplies a pointer to the following object.

class loginCallback {

public:

	enum callbackType { // A hint.
		USERID,
		PASSWORD
	};

	loginCallback();
	virtual ~loginCallback();

	// Libmail sets the following pointer, before invoking loginPrompt():

	loginInfo::callbackTarget *target; // INTERNAL


	void getUserid();
	void getPassword();

	virtual void loginPrompt(callbackType cbType,
				 std::string prompt)=0;
	// Applications need to define this.


	// Applications should not screw arount with target directly, but
	// invoke one of the following stubs:

	void callback(std::string);
	void callbackCancel();
};



LIBMAIL_END


#endif

