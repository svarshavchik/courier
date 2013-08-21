/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_objectmonitor_H
#define libmail_objectmonitor_H

///////////////////////////////////////////////////////////////////////////
//
// Helper class that detects when the object references by a ptr is destroyed.
//
// Subclass mail::obj.  Declare mail::ptr<T>, where T is mail::obj's
// subclass.  After mail::obj is destroyed, mail::ptr<T>::operator T *()
// will return NULL

#include <set>
#include <cstdio>

#include "namespace.H"

LIBMAIL_START

class ptrBase {
public:
	ptrBase();
	virtual ~ptrBase();
	virtual void ptrDestroyed()=0;
};

template<class T> class ptr : public ptrBase {

	T *r;

public:
	ptr(T *ptrArg) : r(NULL)
	{
		if (ptrArg)
			ptrArg->objectBaseSet.insert(this);
		r=ptrArg;
	}

	ptr(const ptr &o) : r(NULL)
	{
		(*this)=o;
	}

	ptr &operator=(const ptr &o)
	{
		if (o.r == NULL ||
		    o.r->objectBaseSet.count(this) == 0)
		{
			if (o.r)
				o.r->objectBaseSet.insert(this);

			if (r && r->objectBaseSet.count(this) > 0)
				r->objectBaseSet.erase(r->objectBaseSet
						       .find(this));
		}
		r=o.r;

		return *this;
	}

	~ptr()
	{
		if (r && r->objectBaseSet.count(this) > 0)
			r->objectBaseSet.erase(r->objectBaseSet.find(this));
	}

	operator T *() const
	{
		return r;
	}

	T * operator->() const
	{
		return r;
	}

	bool isDestroyed() const { return r == 0; }

	void ptrDestroyed() { r=NULL; }

};

// Some convenient macros

#define MONITOR(T)  mail::ptr<T> thisMonitor(this)

#define DESTROYED() ( thisMonitor.isDestroyed() )

class obj {

public:
	std::set<ptrBase *> objectBaseSet;

	obj();
	virtual ~obj();

	obj(const obj &);
	obj &operator=(const obj &);
};

LIBMAIL_END

#endif
