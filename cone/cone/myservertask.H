/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef myservertask_H
#define myservertask_H

#include "config.h"
#include "myserver.H"

class myServer::Task { // Background server tasks

protected:
	myServer *server;

public:
	friend class myServer;

	Task(myServer *serverArg);

	void add();
	// After constructing, must call this method
	// do queue up this task.

	myServer *getServer() const { return server; }

private:
	void begin();

	virtual void start()=0;	// Subclassed.  I have the floor.

protected:
	virtual void done(); // completed(), then delete this

	virtual void completed(); // I'm done, on to the next task

	virtual ~Task();
};

#endif
