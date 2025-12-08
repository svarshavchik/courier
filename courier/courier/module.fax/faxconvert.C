/*
** Copyright 2025 S. Varshavchik.
** See COPYING for distribution information.
*/

#include "config.h"
#include "courier.h"
#include "faxconvert.h"
#include "comfax.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "rfc822/rfc822.h"
#include "rfc822/rfc822hdr.h"
#include "rfc822/rfc2047.h"
#include "rfc2045/rfc2045.h"
#include "rfc2045/rfc2045charset.h"
#include "numlib/numlib.h"
#include "gpglib/gpglib.h"
#include <courier-unicode.h>

#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#if HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif
#include <iostream>
#include <iterator>
#include <string>
#include <set>
#include <sstream>
#include <algorithm>

void rfc2045_error(const char *errmsg)
{
	fprintf(stderr, "faxconvert: %s\n", errmsg);
	exit(1);
}

struct faxconvert_info {
	rfc822::fdstreambuf &fd;
	rfc2045::entity &topp;
	unsigned partnum;
	int npages;
	int flags;

	const rfc2045::entity *cover_page_part;
	rfc822::fdstreambuf *cover_page_fd;

	std::set<std::string> unknown_list;
} ;

/*
** Remove a directory's contents.
*/

void rmdir_contents(const char *dir)
{
	DIR *p=opendir(dir);
	struct dirent *de;

	if (!p)
		return;

	std::string f;

	while ((de=readdir(p)) != 0)
	{
		const char *c=de->d_name;

		if (strcmp(c, ".") == 0 || strcmp(c, "..") == 0)
			continue;

		f.clear();
		f=dir;
		f += "/";
		f += c;
		unlink(f.c_str());
	}
	closedir(p);
}

static int faxconvert_recursive(const rfc2045::entity *,
				struct faxconvert_info *);
static int convert_cover_page(const rfc2045::entity *,
			      struct faxconvert_info *,
			      rfc822::fdstreambuf *, unsigned *);

int faxconvert(const char *filename, int flags, unsigned *ncoverpages)
{
	int fd=open(filename, O_RDONLY);
	rfc2045::entity entity;
	int rc;

	/* Open the MIME message, and parse it */

	if (fd < 0)
	{
		perror(filename);
		return (-1);
	}

	rfc822::fdstreambuf fdb{fd};

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)	/* I'm a lazy SOB */
	{
		perror(filename);
		close(fd);
		return (-1);
	}

	std::istream input_stream{&fdb};
	std::istreambuf_iterator<char> b{input_stream}, e;

	rfc2045::entity::line_iter<false>::iter parser{b, e};
	entity.parse(parser);

	/*
	** Ok, clean out SUBTMPDIR and FAXTMPDIR.  We run each filter and tell
	** it to spit its output into SUBTMPDIR, then, we scan SUBTMPDIR,
	** pick up whatever we find there, and move it to FAXTMPDIR.
	*/

	rmdir_contents( SUBTMPDIR );
	rmdir_contents( FAXTMPDIR);

	faxconvert_info info{fdb, entity};

	info.partnum=0;
	info.cover_page_part=NULL;
	info.cover_page_fd= nullptr;
	info.npages=0;
	info.flags=flags;
	rc=faxconvert_recursive(&entity, &info);

	/* Generate a cover page, now */

	if (rc == 0)
	{
		if (info.cover_page_part == NULL)
		{
			fprintf(stderr, "Error: no cover page (empty document)?\n");
			rc= -1;
		}
		else
		{
			rc= convert_cover_page(info.cover_page_part,
					       &info,
					       info.cover_page_fd,
					       ncoverpages);
		}
	}

	rmdir_contents(SUBTMPDIR);
	return (rc);
}

/*
** Recursively parse the message's MIME structure, and convert each MIME
** section to FAX format.
*/

static rfc822::fdstreambuf start_filter(const char *, pid_t *);
static int end_filter(const char *, pid_t);

static int do_filter(const rfc2045::entity *, const std::string &,
		     rfc822::fdstreambuf *,
		     struct faxconvert_info *, int);

/* Determine if we have a filter for the given content type */

static std::string get_filter_prog(const std::string &content_type)
{
	std::string filterprog;

	/*
	** The name of each filter is just its MIME type, with / and .
	** replaced by -, and with '.filter' appended.
	*/

	filterprog.reserve(content_type.size()+sizeof(FILTERBINDIR)+20);

	filterprog= FILTERBINDIR "/";

	for (char c:content_type)
	{
		if (c == '.' || c == '/')
			c='-';
		filterprog.push_back(c);
	}
	filterprog += ".filter";

	if (access(filterprog.c_str(), X_OK) < 0)
		filterprog.clear();

	return (filterprog);
}

static int faxconvert_recursive(const rfc2045::entity *m,
				struct faxconvert_info *info)
{
	std::string filterprog;
	int rc;

	if (m->content_type.value == "multipart/mixed")
	{
		/* Spit out all attachments, one after another */

		for (auto &sube:m->subentities)
		{

			rc=faxconvert_recursive(&sube, info);
			if (rc)
				return (rc);
		}
		return (0);
	}

	if (m->content_type.value == "multipart/alternative")
	{
		const rfc2045::entity *bestm=NULL;

		/* Search for MIME content we understand */

		for (auto &sube:m->subentities)
		{
			if (sube.multipart() ||
			    !get_filter_prog(sube.content_type.value).empty())
				bestm= &sube;
		}

		if (bestm)
			return ( faxconvert_recursive(bestm, info));
	}

	if (m->content_type.value == "multipart/related")
	{
		auto sid=m->content_type_start();

		/* Punt - find the main content, and hope for the best */

		for (auto &q:m->subentities)
		{
			if (q.content_id != sid)
			{
				auto qq=q.content_type_multipart_signed();

				if (!qq) continue;

				/* Don't give up just yet */

				if (qq->content_id != sid)
					continue;
				return faxconvert_recursive(qq, info);
			}

			return faxconvert_recursive(&q, info);
		}
	}

	auto ms=m->content_type_multipart_signed();

	if (ms)
	{
		return ( faxconvert_recursive(ms, info));
	}

	if (info->partnum == 0)	/* Cover page? */
	{
		if (m->content_type.value == "text/plain")
		{
			/* Yes - we have text/plain content */

			++info->partnum;
			info->cover_page_part=m;
			info->cover_page_fd=&info->fd;
			return (0);
		}

		/* No - too bad */

		info->cover_page_part=m;
		info->cover_page_fd=nullptr;
		++info->partnum;
	}
	else if (info->flags & FAX_COVERONLY)
		return (0);	/* Ignore the rest of the message */

	filterprog=get_filter_prog(m->content_type.value);

	if (filterprog.empty() && !(info->flags & FAX_OKUNKNOWN))
	{
		std::cerr <<
			"This message contains an attachment with the document type of\n"
			"\"" << m->content_type.value <<
			"\".  This document type cannot be faxed.\n"
			  << std::flush;
		return (-1);
	}

	if (filterprog.empty())
	{
		info->unknown_list.insert(m->content_type.value);
		rc=0;
	}
	else
	{
		rc=do_filter(m, filterprog, &info->fd, info, 0);
	}
	++info->partnum;
	return (rc);
}

/*
** Read all non-hidden entries in a directory.  Return the list in
** sorted order.
*/

std::vector<std::string> read_dir_sort_filenames(const char *dirname,
						 const char *fnpfix)
{
	std::vector<std::string> ret;
	unsigned cnt=0;
	DIR *dirp;

	if ((dirp=opendir(dirname)) != NULL)
	{
		struct dirent *de;

		while ((de=readdir(dirp)) != NULL)
		{
			if (de->d_name[0] == '.')
				continue;

			std::string filename=fnpfix;
			filename += de->d_name;

			ret.push_back(filename);
			++cnt;
		}
		closedir(dirp);
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

/*
** Convert a single MIME section into FAX format, by running the appropriate
** filter.
*/

static int do_filter(const rfc2045::entity *m,		/* The MIME section */
		     const std::string &filterprog,	/* The filter */
		     rfc822::fdstreambuf *fd,	/* The message */
		     struct faxconvert_info *info,	/* Housekeeping */
		     int is_cover_page)	/* We're generated text/plain cover */
{
	pid_t pidnum=0;
	int n, rc;

	char filenamebuf[sizeof(FAXTMPDIR)+NUMBUFSIZE*2+60];

	auto filter_fd=start_filter(filterprog.c_str(), &pidnum);
	if (filter_fd.error())
		return (-1);

	if (is_cover_page)
		for (auto content_type:info->unknown_list)
		{
			std::ostringstream o;

			o <<"Document type \"" << content_type
			  << "\" cannot be faxed\n"
				"----------------------------------------------------------------\n\n";


			std::string s=o.str();

			filter_fd.sputn(s.c_str(), s.size());
		}

	if (fd)	/* See convert_cover_page() */
	{
		/*
		** Decode the MIME section, push the decoded data to the
		** filtering script.
		*/

		rfc822::mime_decoder decoder{
			[&filter_fd]
			(const char *ptr, size_t n)
			{
				filter_fd.sputn(ptr, n);
			},
			*fd, unicode_default_chset()};

		decoder.decode_header=false;
		decoder.decode_subentities=false;

		decoder.decode<false>(*m);
	}

	filter_fd=rfc822::fdstreambuf{};
	n=end_filter(filterprog.c_str(), pidnum);
	if (n)
		return (n);

	/*
	** Read the filter's results, sort them, compile them.
	*/

	auto l=read_dir_sort_filenames(SUBTMPDIR, "");

	n=0;

	rc=0;

	for (auto &filename:l)
	{
		sprintf(filenamebuf, "%s/f%04d-%04d.g3", FAXTMPDIR,
			info->partnum, ++n);

		std::string p;
		p.reserve(sizeof(SUBTMPDIR) + 1 + filename.size());
		p=SUBTMPDIR "/";
		p += filename;

		if (rename(p.c_str(), filenamebuf))
		{
			perror(p.c_str());
			rc= -1;
		}

#ifdef TESTFAXCONVERT
		TESTFAXCONVERT(filenamebuf);
#endif
		++info->npages;
	}
	return (rc);
}

/*
** Start the filter process as a child process.
*/

static rfc822::fdstreambuf start_filter(const char *prog, pid_t *pidptr)
{
	pid_t p;
	int pipefd[2];

	/* Prepare an empty directory for the filter. */

	rmdir_contents(SUBTMPDIR);
	mkdir(SUBTMPDIR, 0700);

	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		return rfc822::fdstreambuf{};
	}

	p=fork();
	if (p < 0)
	{
		close(pipefd[0]);
		close(pipefd[1]);
		perror("fork");
		return rfc822::fdstreambuf{};
	}

	if (p == 0)
	{
		close(0);
		if (dup(pipefd[0]) < 0)
		{
			perror("dup");
			exit(1);
		}
		close(pipefd[0]);
		close(pipefd[1]);
		execl(prog, prog, SUBTMPDIR, (char *)NULL);
		perror(prog);
		exit(1);
	}

	*pidptr=p;
	close(pipefd[0]);
	return rfc822::fdstreambuf{pipefd[1]};
}

/*
** Wait for the filter process to finish, return its exit status.
*/

static int end_filter(const char *filterprog, pid_t p)
{
	int waitstat;
	pid_t p2;

	while ((p2=wait(&waitstat)) != p)
	{
		if (p2 >= 0)
			continue;
		if (errno != EINTR)
		{
			perror("wait");
			return (-1);
		}
	}

	if (WIFEXITED(waitstat))
	{
		waitstat=WEXITSTATUS(waitstat);

		if (waitstat == 0)
			return (0);
	}

	std::cerr << filterprog
		  << ": terminated with exit status "
		  << waitstat << "\n" << std::flush;
	return (-1);
}

/*
** Generate cover page.
*/

static std::string convnames(const std::string &);

static int convert_cover_page(const rfc2045::entity *rfcp,
			      struct faxconvert_info *info,
			      rfc822::fdstreambuf *, unsigned *ncoverpages)
     /*
     ** fd >= 0 if there's initial text/plain content that can go on the
     ** cover page.  fd < 0 if there is no text/plain content to go on the
     ** cover page.
     */
{
	std::string hfrom, hto, hdate, hsubject;
	size_t save_pages;
	int rc;

	char npages[NUMBUFSIZE];

	rfc2045::entity::line_iter<false>::headers headers{
		info->topp, info->fd
	};

	do
	{
		const auto &[name, content] = headers.name_content();

		if (name == "from")
			hfrom=content;

		if ((name == "to" || name == "cc") && !content.empty())
		{
			if (!hto.empty())
				hto += ",";
			hto += content;
		}

		if (name == "subject")
		{
			hsubject.clear();
			rfc822::display_header(name, content,
					       unicode_default_chset(),
					       std::back_inserter(hsubject));
		}

		if (name == "date")
			hdate=content;

	} while (headers.next());

	/*
	** The cover page scripts read the cover page information from the
	** environment.
	*/

	auto hfrom_str=convnames(hfrom);

	setenv("FROM", hfrom_str.c_str(), 1);

	auto hto_str=convnames(hto);
	setenv("TO", hto_str.c_str(), 1);

	setenv("SUBJECT", hsubject.c_str(), 1);

	setenv("DATE", hdate.c_str(), 1);

	libmail_str_size_t(info->npages, npages);

	save_pages = info->npages;

	setenv("PAGES", npages, 1);

	info->partnum=0;
	rc=do_filter(rfcp, FILTERBINDIR "/coverpage", &info->fd, info, 1);

	/* Ass-backwards way to figure out # of cover pages */
	*ncoverpages = info->npages - save_pages;
	return (rc);
}

static std::string convnames(const std::string &s)
{
	rfc822::tokens t{s};
	rfc822::addresses a{t};

	std::string all_addresses;

	for (auto &address:a)
	{
		if (!all_addresses.empty())
			all_addresses += ", ";

		if (address.address.empty())
			continue;

		if (address.name.empty())
		{
			address.address.display_address(
				unicode_default_chset(),
				std::back_inserter(all_addresses)
			);
		}
		else
		{
			address.name.display_name(
				unicode_default_chset(),
				std::back_inserter(all_addresses)
			);
		}
	}

	return all_addresses;
}

#if 0
int main(int argc, char **argv)
{
	if (argc < 2)
		exit(0);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGPIPE, SIG_IGN);
	faxconvert(argv[1]);
	exit(0);
	return (0);
}
#endif
