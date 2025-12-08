/*
** Copyright 2002, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "runlater.H"
#include        <time.h>
#if HAVE_SYS_TIME_H
#include        <sys/time.h>
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
