/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef gpg_H
#define gpg_H

#include "config.h"

#include "libmail/rfcaddr.H"

#include <vector>
#include <string>
#include <iostream>

//
// GnuPG interface
//

class GPG {

public:
	class Key {
	public:
		Key(std::string, std::string, std::string, bool);
		~Key();

		std::string fingerprint;
		std::string shortname;
		std::string longname;
		bool invalid;

		std::vector<mail::address> address; // Parsed shortname

		void getDescription(std::vector<std::string> &descrArray,
				    size_t width);

		static void getDescriptionColumns
		(std::vector<std::pair<std::string, int> > &columns);
	};

	class dialog;

	std::vector<Key> secret_keys, public_keys;
	typedef std::vector<Key>::iterator Key_iterator;

	GPG();
	~GPG();

	void init();

	class initHelper;

	std::string select_public_key(std::string fingerprint,
				      std::string title,
				      std::string prompt,
				      std::string enterDescr,
				      std::string abortDescr);
	std::string select_private_key(std::string fingerprint,
				       std::string title,
				       std::string prompt,
				      std::string enterDescr,
				      std::string abortDescr);
	
private:
	std::string select_key( std::vector<Key> &, std::string,
				std::string, std::string,
				std::string enterDescr,
				std::string abortDescr);
public:

	void select_public_keys(std::vector<std::string> &fingerprints,
				std::string title,
				std::string prompt,
				std::string enterDescr,
				std::string abortDescr);

	Key_iterator get_secret_key(std::string fingerprint);
	Key_iterator get_public_key(std::string fingerprint);

	void find_secret_keys(std::vector<mail::address> &,
			     std::vector<Key_iterator> &);
	void find_public_keys(std::vector<mail::address> &,
			     std::vector<Key_iterator> &);

	static bool gpg_installed();

	static std::string exportKey(std::string fingerprint,
				     bool secret,
				     std::ostream &);

	class exportKeyHelper;

private:

	static Key_iterator get_key( std::vector<Key> &, std::string);
	static void find_keys( std::vector<Key> &,
			       std::vector<mail::address> &,
			       std::vector<Key_iterator> &);

public:
	static GPG gpg;


	static bool confirmKeySelection(std::string prompt,
					Key &selectedKey,
					Key *key2,
					std::string okDescr,
					std::string cancelDescr);

	class confirmHelper;

	std::string extraEncryptSignOptions;
	std::string extraDecryptVerifyOptions;

	static void create_argv(std::vector<std::string> &argv,
				std::vector< std::vector<char> > &argv_ptr,
				std::vector<char *> &argv_cp);

};

#endif
