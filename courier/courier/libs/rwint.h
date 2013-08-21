#ifndef	rwint_h
#define	rwint_h

#if	HAVE_CONFIG_H
#include	"courier_lib_config.h"
#endif

#include	"courier.h"

#ifdef	__cplusplus
extern "C" {
#endif

struct rwmsginfo {

	int	inheaders;
	int	lastnl;
	char	*headerbuf;
	size_t	headerbufsize;
	size_t	headerbuflen;

	int (*writefunc)(const char *p, unsigned l, void *);
	void (*rewritefunc)(struct rw_info *, void (*)(struct rw_info *),
		void *);

	void	*arg;
	} ;


void rw_rewrite_msg_init(struct rwmsginfo *,
	int (*)(const char *p, unsigned l, void *),
	void (*)(struct rw_info *,
		void (*)(struct rw_info *), void *),
	void *);

int rw_rewrite_msg_finish(struct rwmsginfo *);

int rw_rewrite_msgheaders(const char *, unsigned, struct rwmsginfo *);

#ifdef	__cplusplus
}
#endif
#endif
