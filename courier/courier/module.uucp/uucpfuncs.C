#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"rw.h"
#include	"dbobj.h"
#include	"rfc822/rfc822.h"
#include	"sysconfdir.h"
#include	"uucpfuncs.h"

#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

const char *uucpme()
{
static const char *buf=0;

	if (buf == 0)
	{
	char	*f=config_localfilename("uucpme");

		buf=config_read1l(f);
		free(f);
		if (buf == 0)
		{
			const char *p=config_me();

			buf=strcpy((char *)courier_malloc(strlen(p)+1), p);
			if ((f=(char *)strchr(buf, '.')) != 0)	*f=0;
		}
	}
	return (buf);
}

static struct dbobj uucpneighbors;
static int uucpneighbors_isopen=0;

void uucpneighbors_init()
{
	if (uucpneighbors_isopen)	return;

	dbobj_init(&uucpneighbors);
	uucpneighbors_isopen=1;
	if (dbobj_open(&uucpneighbors, SYSCONFDIR "/uucpneighbors.dat", "R"))
		uucpneighbors_isopen= -1;
}

char	*uucpneighbors_fetch(const char *key, size_t keyl,
			     size_t *retlen, const char *opts)
{
	if (!uucpneighbors_isopen)
		return nullptr;

	return dbobj_fetch(&uucpneighbors, key, keyl, retlen, opts);
}

int uucprewriteheaders()
{
	int val;

	char	*f=config_localfilename("uucprewriteheaders");

	val= access(f, 0) == 0 ? 1:0;
	free(f);
	return val;
}
