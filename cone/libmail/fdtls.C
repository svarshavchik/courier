/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "fdtls.H"
#include <cstdlib>

#if HAVE_LIBCOURIERTLS

// libcouriertls.a callback - get a config setting

const char *mail::fdTLS::get_tls_config_var(const char *varname, void *vp)
{
	return ((mail::fdTLS *)vp)->get_tls_config_var(varname);
}

// libcouriertls.a callback - report a tls error msg

void mail::fdTLS::get_tls_err_msg(const char *errmsg, void *vp)
{
	((mail::fdTLS *)vp)->get_tls_err_msg(errmsg);
}

// libcouriertls.a callback - retrieve SSL/TLS certificate

int mail::fdTLS::get_tls_client_certs(size_t i,
				      const char **cert_array_ret,
				      size_t *cert_array_size_ret,
				      void *vp)
{
	return ((mail::fdTLS *)vp)->get_tls_client_certs(i, cert_array_ret,
							 cert_array_size_ret);
}

// libcouriertls.a callback - release all SSL/TLS certificates

void mail::fdTLS::free_tls_client_certs(void *vp)
{
	((mail::fdTLS *)vp)->free_tls_client_certs();
}


// Get a config setting, for now, use getenv.

const char *mail::fdTLS::get_tls_config_var(const char *varname)
{
	if (strcmp(varname, "TLS_PROTOCOL") == 0 && tlsflag)
		varname="TLS_STARTTLS_PROTOCOL";

	if (strcmp(varname, "TLS_VERIFYPEER") == 0)
	{
		if (domain.size() == 0)
			return "NONE";
	}

	return getenv(varname);
}

int mail::fdTLS::get_tls_client_certs(size_t i,
				      const char **cert_array_ret,
				      size_t *cert_array_size_ret)
{
	if (i < certs.size())
	{
		*cert_array_ret=certs[i].c_str();
		*cert_array_size_ret=certs[i].size();
		return 1;
	}

	return 0;
}

void mail::fdTLS::free_tls_client_certs()
{
}

// libcouriertls.a callback - report a tls error msg

void mail::fdTLS::get_tls_err_msg(const char *errmsgArg)
{
	errmsg=errmsgArg;
}
#else

mail::fdTLS::fdTLS()
{
}

mail::fdTLS::~fdTLS()
{
}

#endif
