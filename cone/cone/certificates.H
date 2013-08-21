/*
** Copyright 2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef certificates_H
#define certificates_H

#include <vector>
#include <map>
#include <string>

class Certificates {

public:
	class cert {
	public:
		std::string name;
		std::string cert;
	};

	std::map<std::string, cert> certs;
};


#endif
