#ifndef	afx_h
#define	afx_h

/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"afx/config.h"

#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

#include	<cstdio>
#include	<iostream>
#include	<istream>
#include	<ostream>


/////////////////////////////////////////////////////////////////////
//
// Some wanker in ISO got rid of ifstream(int), ofstream(int), and
// fstream(int).  Twit.

class afxpipestreambuf : public std::streambuf {
	int fd;
	char buffer[8192];

 public:

	std::ios *iosp;

	afxpipestreambuf(int);
	~afxpipestreambuf();
	void close();
	void seekg(std::streampos);
	void seekp(std::streampos);
	int handle() { return (fd); }
	void handle(int f);
	int sync();
 protected:
	int overflow(int);
	int underflow();
} ;

class afxipipestream : public std::istream {
	afxpipestreambuf fd_;
 public:
	afxipipestream(int = -1);
	~afxipipestream();

	void close() { fd_.close(); }
	void seekg(std::streampos p) { fd_.seekg(p); }
	void seekp(std::streampos p) { fd_.seekp(p); }
	int fd() { return (fd_.handle()); }
	void fd(int f) { fd_.handle(f); }
	void sync() { fd_.sync(); }
} ;

class afxopipestream: public std::ostream {
	afxpipestreambuf fd_;
 public:
	afxopipestream(int = -1);
	~afxopipestream();
	void close() { fd_.close(); }
	void seekg(std::streampos p) { fd_.seekg(p); }
	void seekp(std::streampos p) { fd_.seekp(p); }
	int fd() { return (fd_.handle()); }
	void fd(int f) { fd_.handle(f); }
	void sync() { fd_.sync(); }
} ;

class afxiopipestream: public std::iostream {
	afxpipestreambuf fd_;
 public:
	afxiopipestream(int = -1);
	~afxiopipestream();
	void close() { fd_.close(); }
	void seekg(std::streampos p) { fd_.seekg(p); }
	void seekp(std::streampos p) { fd_.seekp(p); }
	int fd() { return (fd_.handle()); }
	void fd(int f) { fd_.handle(f); }
	void sync() { fd_.sync(); }
} ;

#endif
