/*
** Copyright 1998 - 2009 S. Varshavchik.
** See COPYING for distribution information.
*/

#ifndef	rw_h
#define	rw_h

#include	"courier.h"
#include	"rfc822/rfc822.h"

int aliasexp(std::istream &is, std::ostream &os, struct rw_transport *module);

/* Install libraries */

int rw_install_start();
int rw_install( const char *name,
	struct rw_list *(*rw_install)(const struct rw_install_info *),
	const char *(*rw_init)() );
int rw_install_init();

#ifdef	__cplusplus

/* Search for a rewriting function */

/*struct rw_list *rw_search(const char *); */

/* Rewrite address */

	/* Common rw_info structure */
struct rw_info_rewrite {
	char	*buf;
	char	*errmsg;
	} ;

rfc822::tokens rw_rewrite_tokenize(const char *);
int rw_syntaxchk(const rfc822::tokens &);
#endif

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

#ifdef	__cplusplus
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
			struct rw_transport *,
			 const rfc822::tokens &,
			 const rfc822::tokens &));

struct rw_transport *rw_search_transport(const char *);

void rw_rewrite_module(struct rw_transport *, struct rw_info *,
	void (*)(struct rw_info *));	/* Call module from transport library */

/* Rewrite RFC822 header: */

char *rw_rewrite_header(struct rw_transport *,	/* Rewriting library */
			std::string_view,		/* header */
			int,			/* flags/mode */
			const rfc822::tokens &,	/* sender */
			const rfc822::tokens &,	/* host */
			char **);		/* ptr to error message */

#endif

void rw_local_defaulthost(struct rw_info *, void (*)(struct rw_info *));
	/* Common rewriting function that appends the local domain to
	** unqualified addresses. */

/* Call rw_rewrite_header for the following headers: */

/* If this is ever changed, don't forget to update submit.C */

#define	DO_REWRITE_HEADER(l)	\
	(strncasecmp((l),"to:", 3) == 0 || strncasecmp((l), "cc:", 3) == 0 || \
	 strncasecmp((l), "from:", 5) == 0 || \
	 strncasecmp((l), "reply-to:", 9) == 0)

#endif
