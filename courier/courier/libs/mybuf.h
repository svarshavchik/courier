/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	mybuf_h
#define	mybuf_h


/**************************************************************************/
/*            A quick buffered I/O reader	                          */
/**************************************************************************/

#ifndef	MYBUF_SIZE
#define	MYBUF_SIZE	512
#endif

struct mybuf {
	char buffer[MYBUF_SIZE];
	int fd;
	char *readptr;
	int readleft;
	} ;

#define	mybuf_init(p, f)	((p)->fd=(f), (p)->readleft=0)

#ifndef	mybuf_readfunc
#define	mybuf_readfunc	read
#endif

#define	mybuf_get(p)  (  (p)->readleft <= 0 && \
		((p)->readptr=(p)->buffer, \
			(p)->readleft=mybuf_readfunc((p)->fd, (p)->buffer, \
				sizeof((p)->buffer))) <= 0 ? -1: \
		(--(p)->readleft, (int)(unsigned char)*(p)->readptr++))

#define	mybuf_more(q)	(!!(q)->readleft)

/* The following must be lvalues */

#define	mybuf_ptr(p)		( (p)->readptr )
#define	mybuf_ptrleft(p)	( (p)->readleft )

#endif
