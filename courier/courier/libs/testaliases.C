#include "aliases.h"

#include <string.h>
#include <unistd.h>
#include <iostream>
#include <set>

#define BUSTED do { std::cerr << "Self test in testaliases, line " << __LINE__ << " failed." << std::endl; exit(1); } while (0)

static std::string dummyrec(int n)
{
	char buf[251];

	memset(buf, 'A'+n, 250);
	buf[250]=0;
	return buf;
}

int main(int argc, char **argv)
{
	int n;

	DbObj testObj;

	unlink("testaliases.gdbm");

	testObj.Open("testaliases.gdbm", "N");

	AliasRecord rec(testObj);

	rec.Init("alias");

	std::list<std::string> l;

	for (n=0; n<16; n++)
	{
		l.insert(l.end(), dummyrec(n));
	}
	rec.Add(l, 0);

	rec.Init("alias");

	rec.Delete(dummyrec(15).c_str());
	rec.Delete(dummyrec(8).c_str());

	rec.Init("alias");
	rec.StartForEach();

	std::set<std::string> a;
	std::string ar;

	while ((ar=rec.NextForEach()).size() > 0)
	{
		a.insert(ar);
	}

	for (n=0; n<15; n++)
		if (n != 8 && a.find(dummyrec(n)) == a.end())
			BUSTED;

	if (a.find(dummyrec(15)) != a.end())
		BUSTED;

	if (a.find(dummyrec(8)) != a.end())
		BUSTED;

	rec.Init("alias2");
	std::list<std::string> l2;

	l2.insert(l2.end(), "AAAA");

	rec.Add(l2, 0);

	rec.Delete("AAAA");

	rec.Init("alias2");
	rec.StartForEach();
	if (rec.NextForEach() != "")
		BUSTED;

	testObj.Close();
	unlink("testaliases.gdbm");
	return (0);
}
