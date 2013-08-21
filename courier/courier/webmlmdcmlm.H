/*
**
** Copyright 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef webmlmdcmlm_H
#define webmlmdcmlm_H

#include <stdio.h>
#include <sys/types.h>

#include <string>
#include <vector>

namespace webmlmd {

	// Run couriermlm as a child process

	class cmlm {

		pid_t pid;		// couriermlm's pid

		class filep {

			filep(const filep &); // UNDEFINED
			filep &operator=(const filep &); // UNDEFINED

		public:
			FILE *fp;

			filep();
			~filep();
			void close();
		};

		filep stdin_filep,	// couriermlm child process's stdin
			stdout_filep;		// and stdout

		cmlm(const cmlm &); // UNDEFINED
		cmlm &operator=(const cmlm &); // UNDEFINED

	public:
		cmlm();
		~cmlm();

		bool start(std::string extension, // DEFAULT env to couriermlm
			   std::string sender,	// SENDER env to couriermlm
			   std::string command	// First arg to couriermlm
			   )
		{
			std::vector<std::string> dummy;

			return start(extension, sender, command, dummy);
		}

		bool start(std::string extension,  // DEFAULT env to couriermlm
			   std::string sender,	   // SENDER env to couriermlm
			   std::string command,	   // First arg to couriermlm
			   const std::vector<std::string> &args // What's left
			   );
		bool wait();

		FILE *stdinptr() { return stdin_filep.fp; }
		FILE *stdoutptr() { return stdout_filep.fp; }

		void mk_received_header();

		std::string format_error_message();
	};
}

#endif
