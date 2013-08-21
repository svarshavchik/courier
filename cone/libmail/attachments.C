/*
** Copyright 2004-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "misc.H"
#include "headers.H"
#include "rfc822/encode.h"
#include "rfc2045/rfc2045charset.h"
#include <sys/time.h>
#include "attachments.H"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sstream>
#include <iomanip>
#include <cstring>

using namespace std;

void mail::Attachment::common_init()
{
	content_fp=NULL;
	message_rfc822=NULL;
	generating=false;
}

void mail::Attachment::common_fd_init(int fd)
{
	int n;
	FILE *fp;

	if (lseek(fd, 0L, SEEK_END) >= 0 &&
	    lseek(fd, 0L, SEEK_SET) >= 0)
	{
		int fd2=dup(fd);

		if (fd2 < 0)
			throw strerror(errno);

		if ((content_fp=fdopen(fd2, "r")) == NULL)
		{
			close(fd2);
			throw strerror(errno);
		}
		return;
	}

	fp=tmpfile();
	if (!fp)
	{
		throw strerror(errno);
	}

	while ((n=read(fd, encode_info.output_buffer,
		       // Convenient buffer
		       sizeof(encode_info.output_buffer))) > 0)
	{
		if (fwrite(encode_info.output_buffer, n, 1, fp) != 1)
		{
			fclose(fp);
			throw strerror(errno);
		}
	}

	if (fflush(fp) < 0 || ferror(fp))
	{
		fclose(fp);
		throw strerror(errno);
	}
	content_fp=fp;
}

void mail::Attachment::check_multipart_encoding()
{
	string::iterator cte_start, cte_end;

	multipart_type=find_header("CONTENT-TYPE:", cte_start, cte_end);
	mail::upper(multipart_type);

	const char *p=multipart_type.c_str();

	if (strncmp(p, "MULTIPART/", 10) == 0 ||
	    strcmp(p, "MESSAGE/RFC822") == 0)
		transfer_encoding="8bit";
}

mail::Attachment::Attachment(string h, int fd)
	: headers(h)
{
	common_init();
	common_fd_init(fd);

	try {
		check_multipart_encoding();

		if (transfer_encoding.size() == 0)
			transfer_encoding=
				libmail_encode_autodetect_fp(content_fp, 1,
							     NULL);
	} catch (...)
	{
		fclose(content_fp);
		content_fp=NULL;
	}

	add_content_encoding();
}

mail::Attachment::~Attachment()
{
	if (content_fp)
		fclose(content_fp);
	if (message_rfc822 == NULL &&
	    parts.size() == 0)
	{
		if (generating)
			libmail_encode_end(&encode_info);
	}
}

mail::Attachment::Attachment(std::string h, int fd,
			     std::string chs,
			     std::string encoding)

	: headers(h)
{
	common_init();
	common_fd_init(fd);

	try {
		transfer_encoding=encoding;

		if (transfer_encoding.size() == 0)
			check_multipart_encoding();

		if (transfer_encoding.size() == 0)
			transfer_encoding=
				libmail_encode_autodetect_fp(content_fp, 0,
							     NULL);
		add_content_encoding();

	} catch (...) {
		fclose(content_fp);
		throw;
	}
}

mail::Attachment::Attachment(string h, string content)
	: headers(h), content_string(content)
{
	common_init();
	check_multipart_encoding();
	if (transfer_encoding.size() == 0)
		transfer_encoding=
			libmail_encode_autodetect_buf(content.c_str(), 1);

	add_content_encoding();
}

mail::Attachment::Attachment(string h, string content,
			     string chs,
			     string encoding)
	: headers(h), content_string(content)
{
	common_init();
	transfer_encoding=encoding;

	if (transfer_encoding.size() == 0)
		check_multipart_encoding();

	if (transfer_encoding.size() == 0)
		transfer_encoding=
			libmail_encode_autodetect_buf(content.c_str(), 1);
	add_content_encoding();
}

void mail::Attachment::add_content_encoding()
{
	string::iterator cte_start, cte_end;

	multipart_type=find_header("CONTENT-TYPE:", cte_start, cte_end);
	mail::upper(multipart_type);

	string existing_transfer_encoding=
		find_header("CONTENT-TRANSFER-ENCODING:", cte_start,
			    cte_end);

	const char *p=multipart_type.c_str();

	if (cte_start != cte_end ||
	    // Already have the header. Must be already encoded.

	    strncmp(p, "MULTIPART/", 10) == 0 ||
	    strcmp(p, "MESSAGE/RFC822") == 0)
	{
		transfer_encoding="8bit";
		return;
	}

	headers += "Content-Transfer-Encoding: ";
	headers += transfer_encoding;
	headers += "\n";
}

mail::Attachment::Attachment(string h,
			     const vector<Attachment *> &partsArg)
	: headers(h)
{
	common_init();
	parts=partsArg;
	common_multipart_init();
}

mail::Attachment::Attachment(string h,
			     const vector<Attachment *> &partsArg,
			     string multipart_typeArg,
			     const mail::mimestruct::parameterList
			     &multipart_parametersArg)
	: headers(h)
{
	common_init();
	parts=partsArg;
	multipart_type=multipart_typeArg;
	multipart_parameters=multipart_parametersArg;
	common_multipart_init();
}

string mail::Attachment::find_header(const char *header_name,
				     string::iterator &hstart,
				     string::iterator &hend)
{
	return find_header(header_name, headers, hstart, hend);
}

std::string mail::Attachment::find_header(const char *header_name,
					  std::string &headers,
					  std::string::iterator &hstart,
					  std::string::iterator &hend)
{
	size_t l=strlen(header_name);

	string::iterator b=headers.begin(), e=headers.end();
	string::iterator s=b;

	while (b != e)
	{
		while (b != e)
		{
			if (*b == '\n')
			{
				++b;
				break;
			}
			++b;
		}

		string h=string(s, b);

		mail::upper(h);

		if (strncmp(h.c_str(), header_name, l) == 0)
			break;
		s=b;
	}

	while (b != e)
	{
		if (*b != ' ' && *b != '\t')
			break;

		while (b != e)
		{
			if (*b == '\n')
			{
				++b;
				break;
			}
			++b;
		}
	}

	hstart=s;
	hend=b;

	if (s != b)
		s += l;

	while (s != b && unicode_isspace((unsigned char)*s))
		++s;

	while (s != b && unicode_isspace((unsigned char)b[-1]))
		--b;

	return string(s,b);
}

void mail::Attachment::common_multipart_init()
{
	string::iterator s, b;

	string content_type=find_header("CONTENT-TYPE:", s, b);

	string header=string(s,b);

	if (s != b)
		headers.erase(s,b);

	mail::Header::mime mimeHeader= mail::Header::mime::fromString(header);

	if (multipart_type.size() == 0)
		multipart_type=mimeHeader.value;
	multipart_parameters=mimeHeader.parameters;

	if (parts.size() == 0) // Woot?
	{
		multipart_type="text/plain";
		transfer_encoding="8bit";
		return;
	}

	mail::upper(multipart_type);

	if (multipart_type == "MESSAGE/RFC822")
	{
		if (parts.size() > 1) // Woot?
		{
			multipart_type="MULTIPART/MIXED";
			return;
		}
		message_rfc822=parts[0];
	}
	else
	{
		if (strncmp(multipart_type.c_str(), "MESSAGE/", 8) == 0)
			multipart_type="MULTIPART/MIXED";
	}

}

void mail::Attachment::begin()
{
	begin_recursive();

	string::iterator s, b;

	string content_type=find_header("MIME-VERSION:", s, b);

	if (s == b)
		headers="Mime-Version: 1.0\n" + headers;

	string boundary;

	do
	{
		ostringstream fmt;

		struct timeval tv;

		gettimeofday(&tv, NULL);

		fmt << "=_" << mail::hostname() << "-"
		    << tv.tv_sec
		    << "-"
		    << tv.tv_usec
		    << "-"
		    << getpid();


		boundary=fmt.str();
	} while (!try_boundary(boundary, 0));
}

bool mail::Attachment::try_boundary(std::string templ, int level)
{
	string boundary;
	ostringstream fmt;

	// =_(mumble) can never occur in base64 or quoted-printable

	if (transfer_encoding == "base64")
		return true;
	if (transfer_encoding == "quoted-printable")
		return true;

	fmt << templ << "-" << setw(4) << setfill('0') << level;
	boundary=fmt.str();

	if (content_string.size() > 0)
	{
		const char *p=content_string.c_str();

		while (*p)
		{
			if (strncmp(p, "--", 2) == 0 &&
			    strncasecmp(p+2, boundary.c_str(),
					boundary.size()) == 0)
				return false;

			while (*p && *p != '\n')
				++p;
			if (*p)
				++p;
		}
	}
	if (content_fp)
	{
		if (fseek(content_fp, 0L, SEEK_SET) < 0)
			throw strerror(errno);

		while (fgets(encode_info.output_buffer,
			     // Convenient buffer

			     sizeof(encode_info.output_buffer), content_fp))
		{
			if (strncmp(encode_info.output_buffer, "--", 2) == 0 &&
			    strncasecmp(encode_info.output_buffer+2,
					boundary.c_str(),
					boundary.size()) == 0)
				return false;
		}
		if (ferror(content_fp))
			throw strerror(errno);
	}

	if (message_rfc822)
		return message_rfc822->try_boundary(templ, level);

	std::vector<mail::Attachment *>::iterator b, e;

	for (b=parts.begin(), e=parts.end(); b != e; ++b)
		if (!(*b)->try_boundary(templ, level+1))
			return false;

	if (parts.size() > 0)
	{
		multipart_parameters.erase("boundary");
		multipart_parameters.set_simple("boundary", boundary);
	}

	return true;
}

void mail::Attachment::begin_recursive()
{
	std::vector<mail::Attachment *>::iterator b, e;

	generating=false;

	for (b=parts.begin(), e=parts.end(); b != e; ++b)
		(*b)->begin_recursive();
}

string mail::Attachment::generate(bool &error)
{
	if (message_rfc822)
	{
		if (!generating)
		{
			generating=true;
			mail::Header::mime content_type("Content-Type",
							multipart_type);

			content_type.parameters=multipart_parameters;

			return headers + content_type.toString() + "\n\n";
		}
		return message_rfc822->generate(error);
	}

	if (parts.size() > 0)
	{
		if (!generating)
		{
			generating=true;
			multipart_iterator=parts.begin();

			mail::Header::mime content_type("Content-Type",
							multipart_type);

			content_type.parameters=multipart_parameters;

			return headers +
				content_type.toString() + "\n\n"
				RFC2045MIMEMSG "\n--"
				+ multipart_parameters.get("boundary")
				+ "\n";
		}

		if (multipart_iterator == parts.end())
			return "";

		string s=(*multipart_iterator)->generate(error);

		if (error)
			return s;

		if (s.size() > 0)
			return s;

		++multipart_iterator;

		return "\n--" + multipart_parameters.get("boundary")
			+ (multipart_iterator == parts.end()
			   ? "--\n":"\n");
	}

	if (!generating)
	{
		generating=true;
		if (content_fp)
		{
			if (fseek(content_fp, 0L, SEEK_SET) < 0)
			{
				error=true;
				return "";
			}
		}

		libmail_encode_start(&encode_info,
				     transfer_encoding.c_str(),
				     &mail::Attachment::callback_func,
				     this);

		encoded=headers + "\n";
		if (content_fp)
			return encoded;
		if (libmail_encode(&encode_info,
				   content_string.c_str(),
				   content_string.size()) < 0)
		{
			error=true;
			return "";
		}
		libmail_encode_end(&encode_info);
		return encoded;
	}

	encoded="";
	if (content_fp)
	{
		if (feof(content_fp))
		{
			libmail_encode_end(&encode_info);
			return encoded;
		}

		do
		{
			char buf[BUFSIZ];
			int n=fread(buf, 1, sizeof(buf), content_fp);

			if (n < 0)
			{
				error=true;
				return "";
			}

			if (n == 0)
			{
				libmail_encode_end(&encode_info);
				return encoded;
			}

			if (libmail_encode(&encode_info, buf, n) < 0)
			{
				error=true;
				return "";
			}
		} while (encoded.size() == 0);
		// libmail_encode() may not call the callback function,
		// so encoded will be empty, which will be interpreted by
		// the caller as EOF, so we simply try again.
	}
	return encoded;
}

int mail::Attachment::callback_func(const char *c, size_t n, void *va)
{
	((mail::Attachment *)va)->encoded += string(c, c+n);
	return 0;
}

mail::Attachment::Attachment(const mail::Attachment &a)
{
	common_init();

	(*this)=a;
}

#define CPY(n) n=o.n

mail::Attachment &mail::Attachment::operator=(const Attachment &o)
{
	CPY(headers);
	CPY(transfer_encoding);
	if (content_fp)
		fclose(content_fp);

	content_fp=NULL;

	if (o.content_fp)
	{
		int fd2=dup(fileno(o.content_fp));

		if (fd2 < 0)
			throw strerror(errno);

		if ((content_fp=fdopen(fd2, "r")) == NULL)
		{
			close(fd2);
			throw strerror(errno);
		}
	}
	CPY(content_string);
	CPY(parts);

	message_rfc822=NULL;
	if (o.message_rfc822)
		message_rfc822=parts[0];

	CPY(multipart_type);
	CPY(multipart_parameters);
	CPY(generating);
	CPY(multipart_iterator);
	CPY(encode_info);
	CPY(encoded);
	return *this;
}
