/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/


#include	"courier.h"
#include	"comctlfile.h"
#include	<string.h>
#include	<stdlib.h>

/* Determine if this recipient should be reported on */

const char *dsn_sender(struct ctlfile *ctf, unsigned i, int isdelayeddsn)
{
char	c=ctf->delstatus[i][0];
const char *p;
const char *sender=ctf->sender;

/* This msg just came back from the dsn module */

	if (c != COMCTLFILE_DELSUCCESS && !isdelayeddsn)
		c=COMCTLFILE_DELFAIL;
		/* Deferrals are treated as failures for final dsn */


	{
		static int bouncetoauth = -1;

		if (bouncetoauth < 0)
		{
			p=getenv("DSNTOAUTHADDR");

			bouncetoauth=p && *p == '0' ? 0:1;
		}

		if (bouncetoauth)
		{
			int i;

			i=ctlfile_searchfirst(ctf, COMCTLFILE_AUTHNAME);

			if (i >= 0 &&
			    strchr(ctf->lines[i]+1, '@'))
			{
				sender=ctf->lines[i]+1;
			}
		}
	}

	/*
	** An empty envelope sender always bounces failures to postmaster,
	** doublebounce.
	*/

	if (*sender == 0)
		return (!isdelayeddsn &&
			c == COMCTLFILE_DELFAIL &&
			(ctf->dsnreceipients[i] == 0 ||
			ctf->dsnreceipients[i][0] == 0 ||
			strchr(ctf->dsnreceipients[i], 'F'))
				? "postmaster":0);

	if (strcmp(sender, TRIPLEBOUNCE) == 0)	return (0);

	switch (c)	{
	case COMCTLFILE_DELSUCCESS:
		if (isdelayeddsn)
			return (0);	/* This is a delayed DSN only */

		/*
		** Only succesfull local deliveries or relays are ever
		** reported.
		*/

		for (p=ctf->delstatus[i]; *p; p++)
			if (*p == 'l' || *p == 'r')	break;
		if (!*p)	return (0);

		/*
		** To report a success DSN, NOTIFY=SUCCESS must always be
		** selected.
		*/

		if (ctf->dsnreceipients[i] &&
			ctf->dsnreceipients[i][0] &&
			strchr(ctf->dsnreceipients[i], 'S'))
			break;
		return (0);

	case COMCTLFILE_DELFAIL:
		if (isdelayeddsn)
			return (0);	/* This is a delayed DSN only */

		/*
		** Report failures unless NOTIFY was used, and FAILURE was
		** NOT selected.
		*/

		if (ctf->dsnreceipients[i] &&
			ctf->dsnreceipients[i][0] &&
			strchr(ctf->dsnreceipients[i], 'F') == 0)
			return (0);
		break;
	default:

		/* Delayed DSN */

		if (!isdelayeddsn)	return (0);

		if ( ctf->dsnreceipients[i] && ctf->dsnreceipients[i][0])
		{
		const char *p=ctf->dsnreceipients[i];

			for ( ; *p; p++)
			/*
			** When NOTIFY was used, and DELAY was selected,
			** always report.
			*/
				if (*p == 'D')	break;

			if (!*p)	return (0);
		}
		else
		{
		static int defaultdelay = -1;

			if (defaultdelay < 0)
			{
				p=getenv(DSNDEFAULTNOTIFYDELAY);
				defaultdelay=p && *p == '0' ? 0:1;
			}
			if (defaultdelay)
				break;
			return (0);
		}
		break;
	}
	return (sender);
}
