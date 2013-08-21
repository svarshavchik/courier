#include	"courier.h"
#include	"rw.h"
#include	"rwint.h"
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

int rw_rewrite_msg(int fp,
	int (*writefunc)(const char *p, unsigned l, void *),
	void (*rewritefunc)(struct rw_info *,
		void (*)(struct rw_info *), void *),
	void *arg
	)
{
struct rwmsginfo rwinfo;
char	buffer[BUFSIZ];
int	i, j;

	rw_rewrite_msg_init(&rwinfo, writefunc, rewritefunc, arg);

	while ((j=read(fp, buffer, sizeof(buffer))) > 0)
	{
		if (rw_rewrite_msgheaders(buffer, j, &rwinfo) < 0)
		{
			j= -1;
			break;
		}
	}

	if ((i=rw_rewrite_msg_finish(&rwinfo)) != 0)
		j=i;
	return (j);
}
