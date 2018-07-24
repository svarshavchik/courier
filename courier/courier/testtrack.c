#include "courier.h"
#undef TRACKDIR
#define TRACKDIR "testtrack.tmp"

#include "libs/comtrack.c"
/* comtrack.c doesn't use ulocallower */
#include "libs/lcclog.c"
#define config_localfilename strdup
#include "libs/addrlower.c"
#undef config_localfilename
#include "libs/courier_malloc.c"
#include "libs/cdomaincmp.c"

static const struct {
	const char *needle;
	const char *haystack;
	int should_be_there;
} tests[]={
	   {"испытание.example.com", "ИСПЫТАНИЕ.example.com", 0},
	   {"ИСПЫТАНИЕ.example.com", "ИСПЫТАНИЕ.example.com", 0},
	   {"ИСПЫТАНИЕ.example.com", "испытание.example.com", 0},
	   {"example.com", "ИСПЫТАНИЕ.example.com", 1},

	   {"some.ИСПЫТАНИЕ.example.com", "испытание.example.com", 1},
	   {"испытание.example.com", ".испытание.example.com", 1},
	   {"испытание.example.com", ".ИСПЫТАНИЕ.example.com", 1},
	   {"some.ИСПЫТАНИЕ.example.com", ".испытание.example.com", 0},
	   {"some.испытание.example.com", ".ИСПЫТАНИЕ.example.com", 0},
	   {"some.ИСПЫТАНИЕ.example.com", ".испытание.EXAMPLE.com", 0},


	   {"xn--80akhbyknj4f.example.com", "ИСПЫТАНИЕ.example.com", 0},
	   {"испытание.example.com", "xn--80akhbyknj4f.example.com", 0},

};

int main(int argc, char **argv)
{
	time_t ignore;
	size_t i;

	track_save_broken_starttls("ИСПЫТАНИЕ.example.com");
	track_save_broken_starttls("испытание2.example.com");

	track_save_email("nobody@ИСПЫТАНИЕ.example.com", TRACK_ADDRFAILED);
	track_save_email("nobody@испытание2.example.com", TRACK_ADDRFAILED);

	if (track_find_broken_starttls("испытание.example.com", &ignore) !=
	    TRACK_BROKEN_STARTTLS ||
	    track_find_broken_starttls("ИСПЫТАНИЕ2.example.com", &ignore) !=
	    TRACK_BROKEN_STARTTLS ||
	    track_find_email("nobody@ИСПЫТАНИЕ.example.com", &ignore) !=
	    TRACK_ADDRFAILED ||
	    track_find_email("nobody@испытание2.example.com", &ignore) !=
	    TRACK_ADDRFAILED)

	{
		fprintf(stderr, "Doesn't work\n");
		exit(1);
	}

	for (i=0; i<sizeof(tests)/sizeof(tests[0]); ++i)
	{
		if ((config_domaincmp(tests[i].needle,
				      tests[i].haystack,
				      strlen(tests[i].haystack)) ? 1:0)
		    != tests[i].should_be_there)
		{
			fprintf(stderr, "%s in %s result is wrong\n",
				tests[i].needle,
				tests[i].haystack);
			exit(1);
		}
	}

	return 0;
}
