/*
**
** Copyright 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef webmlmddirs_H
#define webmlmddirs_H

#include <string>
#include <vector>

namespace webmlmd {

	// List of mailing list directories managed by webmlmd

	class dirs : public std::vector<std::string> {

		class OrderByLastComponent;
		class OrderByListName;
		class EqualLastComponent;

	public:
		dirs();
		~dirs();

		bool initialize(std::string colonpath); // From here
	};
}

#endif
