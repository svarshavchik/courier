/*
** Copyright 2016 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include <iostream>
#include <stdlib.h>

extern "C" {

#include "courierauthdebug.h"

	int start();
};

int main(int argc, char **argv)
{
	courier_authdebug_login_init();

	if (argc > 1)
	{
		std::cerr <<
			"Error: authdaemond no longer handles its own daemonizing."
			  << std::endl
			  << "Use new startup script." << std::endl;
		exit(1);
	}

	start();
	return (0);
}
