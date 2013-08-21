/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"cmlm.h"
#include	"cmlmarchive.h"
#include	"numlib/numlib.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<string.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<sysexits.h>

#include	<sstream>


Archive::Archive(): ExclusiveLock(MSGLOCKFILE)
{
}

Archive::~Archive()
{
}


int Archive::get_seq_no(unsigned long &nextseqno)
{
int	seqnofd;
char	buf[100];

	nextseqno=0;

	if ( (seqnofd=open(SEQNO, O_RDWR)) < 0)
	{
		if (errno != ENOENT)
		{
			perror(SEQNO);
			return (EX_TEMPFAIL);
		}
	}
	else
	{
	int	n;

		n=read(seqnofd, buf, sizeof(buf)-1);
		if (n < 0)	n=0;
		buf[n]=0;
		close(seqnofd);
		nextseqno=atol(buf);
	}
	seq_no=++nextseqno;
	return (0);
}

int Archive::save_seq_no()
{
char	buf[NUMBUFSIZE+1];
const char *p=strcat(libmail_str_off_t(seq_no, buf), "\n");
unsigned n=strlen(p);
int	seqnofd;

	if ( (seqnofd=open(NEXTSEQNO, O_WRONLY|O_CREAT, 0644)) < 0)
	{
		perror(NEXTSEQNO);
		return (EX_TEMPFAIL);
	}

	while (n)
	{
	int	i=write(seqnofd, p, n);

		if (i <= 0)
		{
			close(seqnofd);
			perror(NEXTSEQNO);
			return (EX_TEMPFAIL);
		}
		p += i;
		n -= i;
	}
	close(seqnofd);
	return (0);
}

std::string Archive::filename(unsigned long n)
{
	std::ostringstream o;

	o << ARCHIVE "/" << n;

	return o.str();
}

ArchiveList::ArchiveList():SharedLock(MSGLOCKFILE),
		dirp(opendir(ARCHIVE))
{
}

ArchiveList::~ArchiveList()
{
	if (dirp)	closedir(dirp);
}

int	ArchiveList::Next(unsigned long &n)
{
struct	dirent	*de;

	while (dirp && (de=readdir(dirp)) != 0)
	{
		if (de->d_name[0] == '.')
			continue;
		n=atol(de->d_name);
		return (0);
	}
	return (-1);
}

std::string ArchiveList::Next()
{
	std::string s;
	unsigned long n;

	if ( Next(n) == 0)
		s= Archive::filename(n);
	return (s);
}
