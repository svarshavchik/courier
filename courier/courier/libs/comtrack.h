#ifndef	comtrack_h
#define	comtrack_h

/*
** Copyright 2005 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	<time.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define TRACK_NHOURS 2

void trackpurge();

int track_find_email(const char *address, time_t *timestamp);
#define TRACK_ADDRACCEPTED 'A'
#define TRACK_ADDRDEFERRED 'D'
#define TRACK_ADDRFAILED   'F'
#define TRACK_NOTFOUND 0

void track_save_email(const char *address, int status);

int track_read_email(int (*cb_func)(time_t timestamp, int status,
				    const char *address, void *voidarg),
		     void *voidarg);


#define TRACK_BROKEN_STARTTLS 'S'
int track_find_broken_starttls(const char *address, time_t *timestamp);
void track_save_broken_starttls(const char *address);

#define TRACK_VERIFY_SUCCESS	 'V'
#define TRACK_VERIFY_SOFTFAIL    'n'
#define TRACK_VERIFY_HARDFAIL    'N'
int track_find_verify(const char *address, time_t *timestamp);
void track_save_verify_success(const char *address);
void track_save_verify_softfail(const char *address);
void track_save_verify_hardfail(const char *address);
#ifdef	__cplusplus
}
#endif
#endif
