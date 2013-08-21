/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "runlater.H"
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

std::queue<mail::ptr<mail::runLater> > mail::runLater::runningLater;

mail::runLater::runLater()
{
}

mail::runLater::~runLater()
{
}

void mail::runLater::RunLater()
{
	runningLater.push(this);
}

void mail::runLater::checkRunLater(int &timeout)
{
	mail::runLater *ptr;

	while (!runningLater.empty())
	{
		ptr=runningLater.front();
		runningLater.pop();

		if (ptr)
		{
			timeout=0;
			ptr->RunningLater();
		}
	}
}
