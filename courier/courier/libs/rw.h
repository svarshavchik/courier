/*
** Copyright 1998 - 2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	rw_h
#define	rw_h

#include	"courier.h"


#ifdef	__cplusplus
extern "C" {
#endif

/* Install libraries */

int rw_install_start();
int rw_install( const char *name,
	struct rw_list *(*rw_install)(const struct rw_install_info *),
	const char *(*rw_init)() );
int rw_install_init();

/* Search for a rewriting function */

/*struct rw_list *rw_search(const char *); */

/* Rewrite address */

	/* Common rw_info structure */
struct rw_info_rewrite {
	char	*buf;
	char	*errmsg;
	} ;

struct rfc822t *rw_rewrite_tokenize(const char *);
int rw_syntaxchk(struct rfc822token *);

void rw_err_func(int, const char *, struct rw_info *);
	/*
	** udata must point to rw_info_rewrite, where the error message
	** is written to.
	*/

void rw_rewrite_print(struct rw_info *);
	/* Convert tokens to text, save in buf. */
void rw_rewrite_chksyn_print(struct rw_info *);
	/* ... But first, check for syntax errors */
void rw_rewrite_chksyn_at_ok_print(struct rw_info *);
	/* ... and allow leading @ in the address */

/* Internal structure stores module list */

extern struct rw_transport {
	struct rw_transport *next;
	char *name;
	struct rw_list *rw_ptr;
	const char *(*init)();
	void *udata;		/* For use by courierd */
	} *rw_transport_first, *rw_transport_last;

void rw_searchdel(struct rw_info *,
		void (*)(struct rw_info *,
			struct rw_transport *, const struct rfc822token *,
			const struct rfc822token *));

struct rw_transport *rw_search_transport(const char *);

void rw_rewrite_module(struct rw_transport *, struct rw_info *,
	void (*)(struct rw_info *));	/* Call module from transport library */

/* Rewrite RFC822 header: */

char *rw_rewrite_header(struct rw_transport *,	/* Rewriting library */
	const char *,		/* header */
	int,			/* flags/mode */
	struct rfc822token *,	/* sender */
	char **);		/* ptr to error message */

char	*rw_rewrite_header_func(void (*rwfunc)(
			struct rw_info *, void (*)(struct rw_info *), void *),
						/* Rewriting function */
	const char *,		/* See above */
	int,			/* See above */
	struct rfc822token *,	/* See above */
	char **,		/* See above */
	void *);	/* Context ptr, passed as last arg to rwfunc */


void rw_local_defaulthost(struct rw_info *, void (*)(struct rw_info *));
	/* Common rewriting function that appends the local domain to
	** unqualified addresses. */


/* Call rw_rewrite_header for the following headers: */

/* If this is ever changed, don't forget to update submit.C */

#if	HAVE_STRNCASECMP
#define	DO_REWRITE_HEADER(l)	\
	(strncasecmp((l),"to:", 3) == 0 || strncasecmp((l), "cc:", 3) == 0 || \
	 strncasecmp((l), "from:", 5) == 0 || \
	 strncasecmp((l), "reply-to:", 9) == 0)
#else
#define	DO_REWRITE_HEADER(l)	\
	(strnicmp((l),"to:", 3) == 0 || strnicmp((l), "cc:", 3) == 0 || \
	 strnicmp((l), "from:", 5) == 0 || \
	 strnicmp((l), "reply-to:", 9) == 0)
#endif

/* Rewrite headers in an entire message */

int rw_rewrite_msg(int,		/* Freshly open file descriptor
				containing the message to rewrite */

	int (*)(const char *, unsigned, void *),
				/*
				** This function is called repeatedly with
				** the contents of the rewritten message.
				** The function should non-0 if there was an
				** error while saving the contents of the
				** rewritten message.
				*/
	void (*)(struct rw_info *,
		void (*)(struct rw_info *), void *),
				/* This function is called to rewrite a
				** single address.  It receives the standard
				** rwinfo structure, and must call the
				** supplied function pointer after doing
				** any rewriting.
				*/
	void *	/*
		** This pointer is forwarded as the last argument to the
		** above two functions.
		*/
	);

	/* rw_rewrite_msg returns 0, or the non-zero exit status if there
	** was an error reported by the write func */

struct rfc2045;

int rw_rewrite_msg_7bit(int,
	struct rfc2045 *,
	int (*)(const char *, unsigned, void *),
	void (*)(struct rw_info *,
		void (*)(struct rw_info *), void *),
	void *
	);
#ifdef	__cplusplus
}
#endif
#endif
