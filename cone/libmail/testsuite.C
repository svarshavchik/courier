/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "nntpnewsrc.H"

#include <iostream>

using namespace std;

static void test1()
{
	mail::nntp::newsrc n;

	n.newsgroupname="misc.test";

	n.read(2);
	n.read(4);
	n.read(8);
	n.read(9);

	n.read(12);
	n.read(11);

	cout << (string)n << endl;

	n.read(1);
	n.read(13);

	cout << (string)n << endl;

	n.read(3);

	cout << (string)n << endl;

	n.read(1);
	n.read(4);
	n.read(11);
	n.read(13);
	n.read(12);

	cout << (string)n << endl;
}

static void test2()
{
	mail::nntp::newsrc n("control! 1-5,10-15,20-25");

	cout << (string)n << endl;

	n.unread(1);

	cout << (string)n << endl;

	n.unread(5);

	cout << (string)n << endl;

	n.unread(12);

	cout << (string)n << endl;

	n.unread(10);

	cout << (string)n << endl;

	n.unread(11);

	cout << (string)n << endl;
	n.unread(15);
	n.unread(14);
	n.unread(13);
	cout << (string)n << endl;

	n.unread(1);
	n.unread(5);
	n.unread(10);
	n.unread(19);
	n.unread(26);
	cout << (string)n << endl;
}

int main()
{
	test1();
	test2();
	exit(0);
}
