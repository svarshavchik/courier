/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "driver.H"

LIBMAIL_START

extern mail::driver inbox_driver, mbox_driver, maildir_driver,
	imap_driver, pop3_driver, pop3maildrop_driver,
	nntp_driver, smtp_driver, tmp_driver;

LIBMAIL_END

static mail::driver *drivers[]={
	&mail::inbox_driver,
	&mail::mbox_driver,
	&mail::maildir_driver,
	&mail::imap_driver,
	&mail::pop3_driver,
	&mail::pop3maildrop_driver,
	&mail::nntp_driver,
	&mail::smtp_driver,
	&mail::tmp_driver,
	NULL };

mail::driver **mail::driver::get_driver_list()
{
	return drivers;
}
