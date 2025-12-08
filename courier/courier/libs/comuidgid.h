/*
** Copyright 2022 S. Varshavchik.
** See COPYING for distribution information.
*/

#ifndef	comuidgid_h
#define	comuidgid_h

#include <unistd.h>

#ifdef	__cplusplus
extern "C" {
#endif
extern uid_t courier_mailuid();
extern gid_t courier_mailgid();

#ifdef	__cplusplus
}
#endif

#endif
