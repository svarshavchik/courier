/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_autodecoder_H
#define libmail_autodecoder_H

#include "decoder.H"
#include "base64.H"
#include "qp.H"
#include "namespace.H"

//
// Generic MIME decoder.  A mail::decoder subclass that uses either base64
// or qp, where appropriate.

LIBMAIL_START

class autodecoder : public decoder {

	// Declare both base64 and qp decoding members.

	class base64 : public decodebase64 {

		autodecoder &me;

	public:
		base64(autodecoder &meArg);
		~base64();
	private:
		void decoded(std::string);
	};

	class qp : public decodeqp {
		autodecoder &me;

	public:
		qp(autodecoder &meArg);
		~qp();
	private:
		void decoded(std::string);
	};

	base64 base64Decoder;
	qp qpDecoder;

	decoder *decoder;	// Points to one of these two, or NULL

public:
	autodecoder(std::string contentTransferEncoding);
	~autodecoder();

	void decode(std::string s);

	virtual void decoded(std::string)=0;

};

LIBMAIL_END

#endif
