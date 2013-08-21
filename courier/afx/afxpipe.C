/*
** Copyright 2001-2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"afx.h"

#include <cstdio>


afxpipestreambuf::afxpipestreambuf(int f) : fd(f), iosp(0)
{
}

afxpipestreambuf::~afxpipestreambuf()
{
	close();
}

void afxpipestreambuf::close()
{
	if (fd >= 0)
	{
		sync();
		::close(fd);
		fd= -1;
	}
}

void afxpipestreambuf::handle(int f)
{
	close();
	fd=f;
}

int afxpipestreambuf::overflow(int c)
{
	if (sync())
		return (EOF);
	int rc=sputc(c);
	return (rc);
}

void afxpipestreambuf::seekg(std::streampos p)
{
	seekp(p);
}

void afxpipestreambuf::seekp(std::streampos p)
{
	if (sync())
		return;

	if (lseek(fd, p, SEEK_SET) < 0 && iosp)
		iosp->setstate(std::ios::badbit);
	else
		setg(buffer, buffer+sizeof(buffer), buffer+sizeof(buffer));
}

int afxpipestreambuf::sync()
{
	if (fd < 0)
		return (-1);

	char *p=pbase();
	char *e=pptr();

	while (p < e)
	{
		int n=::write(fd, p, e-p);

		if (n <= 0 && iosp)
			iosp->setstate(std::ios::badbit);

		if (n <= 0)
			return (-1);

		p += n;
	}

	setp( buffer, buffer+sizeof(buffer));
	return (0);
}

int afxpipestreambuf::underflow()
{
	int n;

	if (fd < 0)
	  return (-1);

	n=read(fd, buffer, sizeof(buffer));

	if (n < 0 && iosp)
		iosp->setstate(std::ios::badbit);
	if (n == 0 && iosp)
		iosp->setstate(std::ios::eofbit);
	if (n <= 0)
		return (EOF);

	setg(buffer, buffer, buffer+n);
	return (sgetc());
}

afxipipestream::afxipipestream(int f) : std::istream(&fd_), fd_(f)
{
	fd_.iosp=this;
}

afxipipestream::~afxipipestream()
{
}

afxopipestream::afxopipestream(int f) : std::ostream(&fd_), fd_(f)
{
	fd_.iosp=this;
}

afxopipestream::~afxopipestream()
{
}


afxiopipestream::afxiopipestream(int f) : std::iostream(&fd_), fd_(f)
{
	fd_.iosp=this;
}

afxiopipestream::~afxiopipestream()
{
}
