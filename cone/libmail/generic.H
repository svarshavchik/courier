/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_generic_H
#define libmail_generic_H

#include "libmail_config.h"

///////////////////////////////////////////////////////////////////////
//
// A primitive set of message access APIs, used by dumb drivers
// to implement the mail::account/IMAP-like message access.
//
// A driver may not have a direct implementation for some of the advanced
// methods (such as the MIME section version of readMessageContent, or
// the MIMESTRUCTURE request of readMessageAttributes).
//
// The alternative, used by the dumb drivers, is to implement the
// mail::generic interface:
//
// genericMessageRead() - read the header, or the body, or both portions of
//   the message.
//
// genericMessageSize() - return just the size of the message.
//
// genericGetMessageFd(), genericGetMessageStruct(),
//   genericGetMessageFdStruct() - return a descriptor of a temporary file
//   with the message's contents, and/or the rfc2045 parse of the message.
//   The dumb driver is expected to cache this data.  The likelyhood of
//   multiple calls to these functions, for the same message, is rather high,
//   so the driver should act accordingly.
//   The dumb driver does not need to implement genericGetMessageFdStruct,
//   if not mail::generic will provide a default, PROVIDED THAT the file
//   descriptor is guaranteed to remain valid after genericGetMessageFd()
//   if genericGetMessageStruct() is immediately called for the same msg. TODO
//
// genericReadMessageContent()/genericReadMessageContentDecode() - a dumb
//   driver can optionally implement these method.  If not, mail::generic
//   will use the previous three functions to do the job.
//
// genericCachedUid(std::string uid) - return true if the generics subclass
//   has message UID cached (used by generics to optimize implementation).
//
// genericAttributes() - implemented entirely by mail::generic.
//
// --
//
// fixMessageNumber() - to handle any concurrency issues, the generic
//   functions also keep track of the message's UID.  fixMessageNumber()
//   updates the message number, based on its uid, in the event that the
//   folder's contents were changed in progress.

#include "mail.H"
#include "maildir/maildirkeywords.h"

struct rfc2045;

LIBMAIL_START

class generic {

	class Attributes;

public:
	generic();
	virtual ~generic();

	virtual void genericMessageRead(std::string uid,
					size_t messageNumber,
					bool peek,
					mail::readMode
					readType,
					mail::callback::message &callback)=0;

	virtual void genericMessageSize(std::string uid,
					size_t messageNumber,
					mail::callback::message &callback)=0;

	virtual void genericGetMessageFd(std::string uid,
					 size_t messageNumber,
					 bool peek,
					 int &fdRet,
					 mail::callback &callback)=0;
	// NOTE: fdRet gets initialized, then the callback function is invoked.
	// The file descriptor remains valid ONLY UNTIL the callback function
	// terminates, at which point the file descriptor may be closed.
	// The callback function must dup the descriptor, if it needs to.

	virtual void genericGetMessageStruct(std::string uid,
					     size_t messageNumber,
					     struct rfc2045 *&structRet,
					     mail::callback &callback)=0;
	// NOTE: structRet gets initialized, then the callback function is
	// invoked. structRet remains valid ONLY UNTIL the callback function
	// terminates, at which point the structure may get deleted.

	virtual void genericGetMessageFdStruct(std::string uid,
					       size_t messageNumber,
					       bool peek,
					       int &fdRet,
					       struct rfc2045 *&structret,

					       mail::callback &callback);

	virtual bool genericCachedUid(std::string uid)=0;

	virtual void genericMarkRead(size_t messageNumber)=0;
	// Mark this msg as read, invoking a callback, if needed.

	// See above.

	static void genericReadMessageContent(mail::account *account,
					      generic *generic,
					      const std::vector<size_t>
					      &messages,
					      bool peek,
					      enum mail::readMode readType,
					      mail::callback::message
					      &callback);

	static void genericReadMessageContent(mail::account *account,
					      generic *generic,
					      size_t messageNum,
					      bool peek,
					      const mimestruct &msginfo,
					      enum mail::readMode readType,
					      mail::callback::message
					      &callback);

	static void genericReadMessageContentDecoded(mail::account *account,
						     generic *generic,
						     size_t messageNum,
						     bool peek,
						     const mimestruct
						     &info,
						     mail::callback::message
						     &callback);


	static void genericMoveMessages(mail::account *account,
					const std::vector<size_t> &messages,
					mail::folder *copyTo,
					mail::callback &callback);

	//
	// Generic keyword update for drivers that implement temporary
	// keywords only (keywords are not saved, and are lost when the
	// mail account is closed).
	// 
	// genericUpdateKeywords() is intended to be invoked directly by
	// updateKeywords().  It will dutifully iterate over the supplied
	// message set, and update each message's keywords, one by one.
	//
	// The interface between the generic implementation, and the
	// implementing driver is a bit tricky.
	// The driver needs to implement the genericProcessKeyword() method.
	// In the method, the driver needs to find the mail::keywords::Message
	// object for message #n, and invoke the doUpdateKeyword() method,
	// passing the reference to the mail::keywords::Message object as
	// the sole argument.  if the driver cannot locate the
	// mail::keywords::Message object (can happen in the mbox driver, if
	// the message was removed by another process, but the application
	// hasn't noop-ed or expunged the folder).

	class updateKeywordHelper {
	public:
		const std::set<std::string> &keywords;
		bool setOrChange;
		bool changeTo;

		updateKeywordHelper(const std::set<std::string> &keywordsArg,
				    bool setOrChangeArg,
				    bool changeToArg);
		~updateKeywordHelper();

		bool doUpdateKeyword(mail::keywords::Message &,
				     mail::keywords::Hashtable &);
	};

	virtual bool genericProcessKeyword(size_t messageNumber,
					   updateKeywordHelper &helper);

	static void genericUpdateKeywords(const std::vector<size_t> &messages,
					  const std::set<std::string> &keywords,
					  bool setOrChange,
					  // false: set, true: see changeTo
					  bool changeTo,
					  mail::callback::folder
					  *folderCallback,
					  generic *generic,
					  callback &cb);



private:
	class Move;

	class GetMessageFdStruct;
	class ReadMultiple;
	class ReadMimePart;

	class CopyMimePart;

	static void genericMakeMimeStructure(mimestruct &s,
					     int fd, struct rfc2045 *rfcp,
					     std::string mimeId,
					     envelope *saveEnvelope);
	//
	// Create a mimestruct, (and a envelope, if necessary)
	// from a rfc2045 parse of a MIME section

public:
	static void genericBuildEnvelope(std::string header, std::string value,
					 envelope &envelope);

	class Decode;

	static void genericAttributes(mail::account *account,
				      generic *genericInterface,
				      const std::vector<size_t> &msgs,
				      mail::account::MessageAttributes attributes,
				      mail::callback::message &callback);

	static bool fixMessageNumber(mail::account *account,
				     std::string uid,
				     size_t &msgNum);

	static void genericRemoveMessages(mail::account *account,
					  const std::vector<size_t> &messages,
					  callback &cb);


private:
	class Remove;

};

LIBMAIL_END

#endif

