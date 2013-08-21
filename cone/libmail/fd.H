/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_fd_H
#define libmail_fd_H

#include "mail.H"

#include <string>
#include <queue>

LIBMAIL_START

///////////////////////////////////////////////////////////////////////////
//
// A network connection-based (optionally encrypted) mail::account handler.
//
// Implements the process() method.
//
// The subclass should invoke socketWrite() to write to the network
// connection.  socketWrite() can receive either a string, or a WriteBuffer.
// A WriteBuffer is subclassed to implement writing large amounts of data.
// The subclass must implement WriteBuffer::fillWriteBuffer() to save
// the next portion of data that needs to be written in the writebuffer
// field.  WriteBuffer::fillWriteBuffer() gets called repeatedly each time
// the contents of writebuffer are succesfully written out.
// WriteBuffer::fillWriteBuffer() must return false when there's no more
// data to be written out, in which case the objects gets destroyed by
// mail::fd.
//
// The subclass should implement socketRead() to process read information.
// socketRead() receives everything read from the network connection, but
// not processed, and should return the number of characters that were
// processed (0 - need more input, < 0, error)
//
// The subclass should implement disconnect() which gets invoked when the
// network connection is closed.
//
// The remaining public interface is obvious.

class fdTLS;

class fd : public mail::account {

	int socketfd;

	bool ioDebugFlag;

	std::vector<std::string> certificates;

protected:
	bool writtenFlag;

public:
	int getfd() { return socketfd; }

	class WriteBuffer {

	public:
		WriteBuffer();
		virtual ~WriteBuffer();

		std::string writebuffer;

		virtual bool fillWriteBuffer();
	};

	static std::string rootCertRequiredErrMsg;
	// Error message returned when SSL certificate verification is
	// requested, but trusted SSL certificate authority list is not
	// installed.

	fd(mail::callback::disconnect &disconnect_callback,
	   std::vector<std::string> &certificatesArg);

	virtual ~fd();

	std::string socketDisconnect();
	//
	// Disconnect the socket.  If socketDisconnect() is invoked after
	// a fatal network error, a non-empty error message is returned.

	std::string socketConnect(class mail::loginInfo &loginInfo,
				  const char *plainservice,
				  const char *sslservice);
	//
	// Create a new server connection, should be called shortly after
	// the constructor.
	//
	// loginInfo - login parameters.
	// plainService, sslService - default ports to connect to, for SSL
	//      and non-SSL connections.
	//
	// Returns an empty string for success, or an error message.

	std::string socketAttach(int fd);
	//
	// Attach to an existing socket
	//

	bool socketConnected();
	//
	// Returns non-zero if a network connection exists (may not necessarily
	// be kigged on).
	//

	virtual void disconnect(const char *reason)=0;
	// The subclass must implement the disconnect method.  Reason is the
	// error message responsible for disconnecting, or a null or an empty
	// string for an orderly shut down.

	virtual int socketRead(const std::string &readbuffer)=0;
	//
	// The subclass must implement socketRead() to process read data in
	// readbuffer.  socketRead should return the number of bytes consumed
	// (which may be 0, if socketRead needs to see more data before it
	// can be consumed).

	void socketWrite(std::string s);
	void socketWrite(WriteBuffer *p);

	// When TLS is enabled, socketBeginEncryption initiates SSL
	// negotiation.  The login parameters may already specify that the
	// server connection should be encrypted right from the start, in
	// which case fd begin SSL negotiation automatically as soon
	// as the connection is established.  socketEncrypted() may be used
	// to determine whether the connection is already encrypted.  If not,
	// passing a copy of the original login parameters to
	// socketBeginEncryption() will begin SSL negotiation.

	bool socketBeginEncryption(mail::loginInfo &);

	bool socketEncrypted()
	{
		return tls != NULL;
	}

	bool socketEndEncryption();
	//
	// socketEndEncryption() terminates SSL negotiation, which normally
	// happens prior to disconnecting from the server.
	// socketEndEncryption() returns true if an SSL connection is
	// currently active, and SSL disconnection was initiated.
	// The disconnect() method will be called automatically after SSL
	// terminates.  Typical usage:
	//
	// if (!socketEndEncryption()) socketDisconnect();



protected:
	int process(std::vector<pollfd> &fds, int &timeout);

private:
	std::string starttls(class mail::loginInfo &loginInfo,
			     bool starttlsFlag);

public:
	static time_t getTimeoutSetting(mail::loginInfo &loginInfo,
					const char *name, time_t defaultValue,
					time_t minValue, time_t maxValue);
private:
	bool establishtls();

	int connecting;		// Connection in progress
	fdTLS *tls;		// SSL/TLS stuff

	std::queue<WriteBuffer *> writequeue;
			// Stuff waiting to be written out

	std::string *getWriteBuffer(); // Get next stuff to write

	void writtenBuffer(size_t n)	// Stubb was written
	{
		std::string *s= &writequeue.front()->writebuffer;

		*s= s->substr(n);
	}

	std::string readbuffer;	// Stuff read, but not processed.

	fd(const fd &); // UNDEFINED

	fd &operator=(const fd &); // UNDEFINED
};

LIBMAIL_END

#endif
