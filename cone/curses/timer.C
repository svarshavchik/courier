/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "timer.H"

std::set<Timer *> Timer::timer_list;

Timer::Timer()
{
	timeout.tv_sec=0;
	timeout.tv_usec=0;

	timer_list.insert(this);
}

Timer::~Timer()
{
	timer_list.erase(this);
}

void Timer::setTimer(int nSeconds)
{
	gettimeofday(&timeout, NULL);
	timeout.tv_sec += nSeconds;
}

void Timer::setTimer(struct timeval tv)
{
	gettimeofday(&timeout, NULL);
	timeout.tv_sec += tv.tv_sec;

	timeout.tv_usec += tv.tv_usec;

	if (timeout.tv_usec >= 1000000)
	{
		++timeout.tv_sec;
		timeout.tv_usec %= 1000000;
	}
}

struct timeval Timer::getTimer() const
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return getTimer(tv);
}

struct timeval Timer::getTimer(const struct timeval &tv) const
{
	struct timeval t;

	t.tv_sec=0;
	t.tv_usec=0;

	if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
		return t;

	if (tv.tv_sec > timeout.tv_sec)
		return t;

	if (tv.tv_sec == timeout.tv_sec &&
	    tv.tv_usec >= timeout.tv_usec)
		return t;

	t=timeout;

	t.tv_sec -= tv.tv_sec;
	t.tv_usec -= tv.tv_usec;

	if (t.tv_usec < 0)
	{
		t.tv_usec += 1000000;
		t.tv_sec--;
	}

	return t;
}

#define DEBUG 1
#undef DEBUG

struct timeval Timer::getNextTimeout(bool &alarmCalledFlag)
{
	struct timeval now;

	gettimeofday(&now, NULL);

	bool alarmed;

	bool wasAlarmed=false;

	struct timeval s;

	alarmCalledFlag=false;

	do
	{
#if DEBUG
	cerr << "In getNextTimeout:" << endl;
#endif

		alarmed=false;

		std::set<Timer *>::iterator b=timer_list.begin(),
			e=timer_list.end();

		s.tv_sec=0;
		s.tv_usec=0;

		while (b != e)
		{
			Timer *t= *b++;

			if (t->timeout.tv_sec == 0 &&
			    t->timeout.tv_usec == 0)
				continue;

			struct timeval v=t->getTimer(now);

#if DEBUG
			cerr << "Timer " << t << ": " << v.tv_sec
			     << "." << v.tv_usec << endl;
#endif

			if (v.tv_sec == 0 && v.tv_usec == 0)
			{
#if DEBUG
				cerr << "ALARM: " <<
					(wasAlarmed ? "ignored":"handled")
				     << endl;
#endif
				if (wasAlarmed)
				{
					// We can get here if an alarm
					// went off, and the alarm handler
					// reset the alarm to 0 seconds again.
					// We'll get it on the next go-round

					s=v;
					break; // Kick the alarm next time
				}
				t->timeout=v;
				t->alarm();
				alarmCalledFlag=true;
				alarmed=true;
			}
			else if ((s.tv_sec == 0 && s.tv_usec == 0) ||
				 v.tv_sec < s.tv_sec ||
				 (v.tv_sec == s.tv_sec
				  && v.tv_usec < s.tv_usec))
				s=v;
		}

		wasAlarmed=alarmed;

	} while (alarmed);

#if DEBUG
	cerr << "getNextTimeout: " << s.tv_sec << "." << s.tv_usec << endl;
#endif
	return s;
}

void Timer::alarm()
{
}
