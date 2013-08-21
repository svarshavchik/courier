/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "myservertask.H"

myServer::Task::Task(myServer *serverArg) : server(serverArg)
{
}

void myServer::Task::add()
{
	bool wasEmpty=server->tasks.empty();

	server->tasks.push(this);

	if (wasEmpty) // We're the first task
		begin();
}

myServer::Task::~Task()
{
}

void myServer::Task::begin()
{
	if (server == NULL || server->server == NULL) // Don't bother
	{
		done();
	}
	else
		start();
}

void myServer::Task::done()
{
	completed();
	delete this;
}

void myServer::Task::completed()
{
	if (server)
	{
		server->tasks.pop();

		if (!server->tasks.empty())
			server->tasks.front()->begin();
	}
}

void myServer::disconnectTasks()
{
	while (!tasks.empty())
	{
		Task *t=tasks.front();
		t->server=NULL;
		tasks.pop();
		delete t;
	}
}
