#if	HAVE_CONFIG_H
#include	"courier_lib_config.h"
#endif
#include	"courier.h"
#include	"rw.h"
#include	"rwint.h"
#include	"rfc2045/rfc2045.h"
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

static int call_rw_rewrite_msgheaders(const char *p, int l,
	void *msginfo)
{
	if (rw_rewrite_msgheaders(p, l, (struct rwmsginfo *)msginfo))
		return (0);
	return (l);
}

int rw_rewrite_msg_7bit(int fp,
	struct rfc2045 *rfcp,
	int (*writefunc)(const char *p, unsigned l, void *),
	void (*rewritefunc)(struct rw_info *,
		void (*)(struct rw_info *), void *),
	void *arg
	)
{
	struct rwmsginfo rwinfo;
	int	i, j;
	struct rfc2045src *src;

	rw_rewrite_msg_init(&rwinfo, writefunc, rewritefunc, arg);

	src=rfc2045src_init_fd(fp);

	if (src)
	{
		j=rfc2045_rewrite_func(rfcp, src,
				       &call_rw_rewrite_msgheaders, &rwinfo,
				       "courier " COURIER_VERSION);
		rfc2045src_deinit(src);
	}
	else
		j= -1;

	if ((i=rw_rewrite_msg_finish(&rwinfo)) != 0)
		j=i;
	return (j);
}
