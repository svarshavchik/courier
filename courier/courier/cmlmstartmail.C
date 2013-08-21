/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"cmlm.h"
#include	"cmlmstartmail.h"

#include	<fstream>


StartMail::StartMail(std::istream &m, std::string r): msg(m), verp_ret(r), n(0)
{
	recipients.resize(MAXRCPTS);
}

StartMail::~StartMail()
{
}

void StartMail::To(std::string s)
{
	if (n >= MAXRCPTS)
		Send();
	recipients[n++]=s;
}

void StartMail::Send()
{
	const char	*argvec[8];
	std::string	r;

	if (n == 0)	return;

	r=get_verp_return(verp_ret);

	argvec[0]="sendmail";
	argvec[1]="-bcc";
	argvec[2]="-verp";
	argvec[3]="-f";
	argvec[4]=r.c_str();
	argvec[5]="-N";
	argvec[6]="fail";
	argvec[7]=0;

	pid_t	p;
	int	fd=sendmail(argvec, p);
	afxopipestream ofs(fd);
	size_t	i;

	for (i=0; i<n; i++)
		ofs << "Bcc: " << recipients[i] << std::endl;

	n=0;

	msg.clear();
	msg.seekg(0);

	ofs << msg.rdbuf();
	ofs.close();

	(void)wait4sendmail(p);
}
