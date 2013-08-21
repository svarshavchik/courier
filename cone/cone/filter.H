/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef filter_H
#define filter_H

#include "config.h"
#include "libmail/search.H"

#include <string>

class myServer;

class Filter {
public:

	// A single filtering step

	class Step {
	public:
		enum Type {
			filter_step_selectall, // Select all msgs
			filter_step_selectsrch, // Use searchParams

			filter_step_delete, // Delete msgs
			filter_step_undelete, // Undelete msgs
			filter_step_delexpunge, // Delete and expunge

			filter_step_mark, // Mark msgs
			filter_step_unmark, // Unmark msgs

			filter_step_tag, // Tag msgs

			filter_step_copy, // Copy msgs to anoter folder
			filter_step_move, // Move msgs to another folder

			filter_step_watch, // Watch
			filter_step_unwatch // Unwatch
		} type;

		mail::searchParams searchType;

		std::string toserver; // Copy/move msgs to this server
		std::string tofolder; // Copy/move msgs to this folder

		std::string name_utf8; // Name of destination folder

		operator std::string () const; // Serialize

		Step();
		~Step();
		Step(std::string); // Deserialize

		std::string getDescription() const; // Describe this step
	};

	class editScreen;

	static void filterEditScreen(void *);

private:
	static bool doEditScreen(myServer *, std::string, std::string &,
				 Filter::Step::Type &, bool &);

};

#endif
