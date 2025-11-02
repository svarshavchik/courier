#ifndef testfuncs_init_h
#define testfuncs_init_h

inline void testfuncsinit()
{
	umask(022);
	mkdir(FILTERBINDIR, 0777);
	mkdir(FAXTMPDIR, 0777);
	mkdir(SUBTMPDIR, 0777);

	{
		std::ofstream o{FILTERBINDIR "/text-plain.filter"};

		o << "#! /bin/sh\n"
			"exec >" SUBTMPDIR "/page1\n"
			"echo \"FAXRES=$FAXRES\"\n"
			"exec >" SUBTMPDIR "/page2\n"
			"cat\n";
		o.close();
	}
	{
		std::ofstream o{FILTERBINDIR "/coverpage"};

		o << "#! /bin/sh\n"
			"exec >" SUBTMPDIR "/page1\n"
			"echo \"FAXRES=$FAXRES\"\n"
			"echo \"FROM=$FROM\"\n"
			"echo \"TO=$TO\"\n"
			"echo \"SUBJECT=$SUBJECT\"\n"
			"echo \"DATE=$DATE\"\n"
			"echo \"PAGES=$PAGES\"\n"
			"echo \"\"\n"
			"cat\n"
			"\n";
		o.close();
	}
	chmod(FILTERBINDIR "/text-plain.filter", 0755);
	chmod(FILTERBINDIR "/coverpage", 0755);
}

#endif
