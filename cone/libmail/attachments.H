/*
** Copyright 2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_attachments_H
#define libmail_attachments_H

///////////////////////////////////////////////////////////////////////////
//
// mail::Attachment is a versatile object that's used to build arbitrary
// MIME objects.
//
// A mail::Attachment object can represent a:
//
// 1) A content-containing MIME entity.  The mail::Attachment object
//    can optionally select the most appropriate encoding method.
//
// 2) A multipart MIME entity.
//
// 3) A message/rfc822 entity.


#include <string>
#include <vector>
#include <stdio.h>
#include "namespace.H"
#include "structure.H"
#include "rfc822/encode.h"

LIBMAIL_START

class Attachment {

	std::string headers; // All entities: the headers

	std::string transfer_encoding;
	// If not empty - this is a content entity

	FILE *content_fp; // File descriptor with the content, or
	std::string content_string; // The content in memory

	mail::Attachment *message_rfc822; // This is a message/rfc822 object

	std::vector<mail::Attachment *> parts; // This is a multipart object

	std::string multipart_type;
	mail::mimestruct::parameterList multipart_parameters;

	bool try_boundary(std::string, int level);

	void common_init();
	void common_fd_init(int content_fd);
public:

	//
	// The headers parameter in the following constructions should ideally
	// be created using mail::Header::list.  In all cases the string MUST
	// have a trailing newline.
	//

	Attachment(std::string headers, int content_fd);
	// Content attachment, autodetect encoding

	Attachment(std::string headers, int content_fd,
		   std::string charset,
		   std::string transfer_encoding="");
	// Content attachment uses the given encoding.  transfer_encoding may
	// be an empty string, in which case the encoding is picked based on
	// charset.

	// The following two constructors receive the literal content,
	// instead of a file descriptor.
	Attachment(std::string headers, std::string content_string);
	Attachment(std::string headers, std::string content_string,
		   std::string charset,
		   std::string transfer_encoding="");

	//
	// Create a multipart attachment.  The Content-Type: header is
	// extracted from 'headers' and parsed.
	//
	// If Content-Type: is message/rfc822, the parts vector's length must
	// be exactly 1.

	Attachment(std::string headers,
		   const std::vector<Attachment *> &parts);
	Attachment(std::string headers,
		   const std::vector<Attachment *> &parts,
		   std::string multipart_type,
		   const mail::mimestruct::parameterList
		   &multipart_parameters);
private:
	void check_multipart_encoding();

	void common_multipart_init();
public:

	~Attachment();


	// Generating the MIME object.

	void begin();
	std::string generate(bool &error);

	// Return the next chunk of the generated object.
	// Returns an empty string when done, or if an error occured.
	// (If error=false, we're done)

private:
	void begin_recursive();

	bool generating;
	std::vector<mail::Attachment *>::iterator multipart_iterator;

	struct libmail_encode_info encode_info;

	static int callback_func(const char *, size_t, void *);

	std::string encoded;

public:

	// NOTE: the copy constructor and assignment operator does NOT
	// duplicate the subtree.

	Attachment(const Attachment &);
	Attachment &operator=(const Attachment &);


private:
	std::string find_header(const char *,
				std::string::iterator &,
				std::string::iterator &);
	void add_content_encoding();
public:
	static std::string find_header(const char *,
				       std::string &,
				       std::string::iterator &,
				       std::string::iterator &);
};


LIBMAIL_END

#endif
