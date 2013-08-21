/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	lcrwstaticlist_h
#define	lcrwstaticlist_h

struct rw_list;
struct rw_install_info;

/* List of statically linked modules */

struct rw_static_info {
	const char *name;
	struct rw_list *(*rw_install)(const struct rw_install_info *);
	const char *(*rw_init)();
} ;

/* Macros to build the rw_static[] array */

#define	rwappend2(a,b)	a ## b

#define	DECLARE_STATICFUNCS(module)	\
	struct rw_list * rwappend2(module, _rw_install) \
				(const struct rw_install_info *); \
	const char *rwappend2(module, _rw_init)();

#define	rwname(a)	# a

#define	LIST_STATICFUNCS(module)	\
	{ rwname(module), rwappend2(module, _rw_install), \
			rwappend2(module, _rw_init) }

#endif
