/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_base64_h
#define libmail_base64_h

#include "libmail_config.h"
#include "decoder.H"

#include <string>

#include "rfc822/encode.h"

LIBMAIL_START

//
// MIME base64 decoder.
//

class decodebase64 : public decoder {

	std::string decodeBuffer;

public:
	decodebase64();
	virtual ~decodebase64();
	void decode(std::string text); // text - base64 text

private:
	virtual void decoded(std::string buffer)=0;
	// decoded contents
};

// MIME base64 encoder

class encodebase64 {

	struct libmail_encode_info encodeInfo;

	static int callback_func(const char *, size_t, void *);

public:
	encodebase64();
	virtual ~encodebase64();

	void encode(std::string text);	// text - binary data to encode
	void flush();			// Flush any buffered encoded data.
private:
	virtual void encoded(std::string buffer)=0;
};

//
// A helper object that collects the output of mail::decodebase64 into a
// single string
//

class decodebase64str : public decodebase64 {

public:
	std::string challengeStr;
	decodebase64str();
	decodebase64str(std::string);
	~decodebase64str();
	void decoded(std::string s);

	operator std::string() const { return (challengeStr); }
};

//
// A helper object that base64-encodes a single chunk of data, and returns it.
//

class encodebase64str : public encodebase64 {

public:
	std::string responseStr;

	encodebase64str();
	encodebase64str(std::string);
	~encodebase64str();

	void encoded(std::string s);

	operator std::string() const { return (responseStr); }
};

LIBMAIL_END

#endif
