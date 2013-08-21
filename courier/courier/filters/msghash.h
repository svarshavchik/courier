/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	msghash_h
#define	msghash_h

#define	MD5_INTERNAL
#include	<md5/md5.h>

/* Hash parameters: */

#define	MSGHASH_MARGIN	3		/* First/last N lines of message which
					** are ignored for partial hashes
					*/
#define	MSGHASH_LINESIZE 256		/* Fixed size of the line buffer */

#define	MSGHASH_HIMARGIN 5
	/* Ignore first/last MSGHASH_MARGIN lines for
	** messages larger than MSGHASH_HIMSGHASH_MARGIN */


struct msghashinfo {
	int	inheader;	/* Flag - in the header */

	unsigned numlines;	/* Num of stripped lines in the message */

	/* Three MD5 hashes are calculated: */

	struct MD5_CONTEXT	c_entire;	/* Entire message */
	unsigned long c_entire_cnt;

	struct MD5_CONTEXT	c_top;		/* All but last three lines */
	unsigned long c_top_cnt;

	struct MD5_CONTEXT	c_bot;		/* All but top three lines */
	unsigned long c_bot_cnt;

	char	linebuf[MSGHASH_MARGIN][MSGHASH_LINESIZE];
	char	headerlinebuf[MSGHASH_LINESIZE];
	char	cur_header[16];
	unsigned linebuf_head, linebuf_tail;
	unsigned headerlinebuf_size;
	MD5_DIGEST	md1, md2;
	} ;

void msghash_init(struct msghashinfo *);
void msghash_line(struct msghashinfo *, const char *);
void msghash_finish(struct msghashinfo *);

#endif
