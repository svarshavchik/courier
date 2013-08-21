/*
** Copyright 2001-2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "config.h"
#include "afx.h"
#include <cstdio>

int main(int argc, char **argv)
{
	int fd0=dup(0);
	int fd1=dup(1);

	afxipipestream i(fd0);
	afxopipestream o(fd1);

	int c;

	i.seekg(0);
	while ((c=i.get()) != EOF)
		o << (char)c;
	return (0);
}
