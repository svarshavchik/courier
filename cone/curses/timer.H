/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef timer_H
#define timer_H

#include "../curses/curses_config.h"
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

#include <set>

////////////////////////////////////////////////////////////////////////////
//
// The Timer object is used primarily by CursesStatusBar to automatically
// clear messages after a short delay, but it can be used as a general
// purpose timer.
//
// Timer processing depends on the application repeatedly calling the
// getNextTimeout() method, which checks for any timers that need to go
// off.  getNextTimeout() returns a timeval when the getNextTimeout() should
// be called again, to check for any more timers.
//
// The Timer object may be instantiated as is, or subclassed.  The expired()
// method may be invoked, after getNextTimeout(), to check if the timer has
// gone off.  Alternativelly, the subclass can implement the alarm() method.

class Timer {
	struct timeval timeout;

	static std::set<Timer *> timer_list;

public:
	Timer();
	virtual ~Timer();

	// Set a timeout for the given number of seconds.

	void setTimer(int);

	// Set a timeout for the given number of seconds/milliseconds.

	void setTimer(struct timeval tv);

	// Cancel this timer.

	void cancelTimer() { timeout.tv_sec=0; timeout.tv_usec=0; }
	bool expired() { return timeout.tv_sec == 0 && timeout.tv_usec == 0; }

	// Compute how long before this timer goes off.

	struct timeval getTimer() const;

	// Compute how long before this timer goes off, if the current time is
	// 'now'.

	struct timeval getTimer(const struct timeval &now) const;

	// The timer has gone off.

	virtual void alarm();

	// Trigger any pending timers.  alarmCalledFlag gets set to true if
	// any alarms were kicked off

	static struct timeval getNextTimeout(bool &alarmCalledFlag);
};

//////////////////////////////////////////////////////////////////////////
//
//  A helpful template that embeds Timer as a member of another class.
//  Rather than subclassing from Timer, multiple Timer objects may be
//  members of a class.  Typical usage:
//
//  class X {
//     ...
//
//  TimerRedirect<X> timera, timerb;
//
//       void timeraFunc();
//       void timerbFunc();
//  };
//
//  X::X()
//  {
//       timera=this;
//       timerb=this;
//
//       timera= &X::timerAFunc;
//       timerb= &X::timerBFunc;
//  }
//
//  ...
//
//       timera.setTimer(10);
//

template<class T> class TimerRedirect : public Timer {

	T *myClass;
	void (T::*myMethod)();

public:
	TimerRedirect() : myClass(0), myMethod(0) {}
	~TimerRedirect() {}

	void operator=(T *classPtr) { myClass=classPtr; }
	void operator=( void (T::*methodPtr)() ) { myMethod=methodPtr; }

	void alarm() { if (myClass && myMethod) (myClass->*myMethod)(); }
};

#endif
