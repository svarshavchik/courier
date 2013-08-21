#ifndef	bofh_h
#define	bofh_h

/*
** Copyright 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#undef	PACKAGE
#undef	VERSION
#include	"config.h"
#endif

#ifdef	__cplusplus
extern "C" {
#endif


struct bofh_list {
	struct bofh_list *next;
	char *name;
	struct bofh_list *aliases;
} ;

void bofh_init();

int bofh_chkbadfrom(const char *);
int bofh_chkspamtrap(const char *);
struct bofh_list *bofh_chkfreemail(const char *);
int bofh_chkbadmx(const char *);
int bofh_checkspf(const char *envvarname,
		  const char *defaultValue, const char *keyword);
int bofh_checkspf_status(const char *envvarname,
			 const char *defaultValue, const char *keyword);

extern unsigned max_bofh;
extern int max_bofh_ishard;

#ifdef	__cplusplus
}
#endif
#endif
