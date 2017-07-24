#
# Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
# distribution information.

CLEANFILES=sysconfdir.h localstatedir.h libexecdir.h sbindir.h lcrwstatic.h \
	permissions.dat

noinst_LTLIBRARIES=libcommon.la libcourier.la libmodule.la
noinst_PROGRAMS=testaliases

BUILT_SOURCES=sysconfdir.h localstatedir.h libexecdir.h sbindir.h lcrwstatic.h

libcommon_la_SOURCES= aliases.h comaliases1.C comaliases2.C \
	comaliases3.C comaliases4.C combatchsize.c comcargs.c comcargs.h \
	comconfig.c comconfig.h comctlfile.c \
	comctlfile.h comdsn.c comdsn2.c comerrmsg.c \
	commsgcancel.c commsgcancel.h comesmtpidstring.c comfax.c comfax.h \
	comorigaddr.c \
	comparseqid.c comparseqid.h comqueuename.c comqueuename.h \
	comqueuename2.c \
	comreadtime.c comreadtime.h comrwerrfunc.c comrwheader.c comrwmsg.c \
	comrwmsg7bit.c comrwmsg8bit.c \
	comrwinfoinit.c comrwinstall.c comrwinstall2.c comrwsearchdel.c \
	comsecurity.c \
	comsizelimit.c comstatfs.c comstrinode.c comstrinode.h \
	comstrtimestamp.c comstrtimestamp.h comstrtotime.c comstrtotime.h \
	comsubmitclient.c comsubmitclient.h comsubmitclient2.c comtmpfile.c \
	comtmpfile2.c comtrack.c comtrack.h \
	comtrigger.c comverp.c comverp.h mybuf.h
libcommon_la_LDFLAGS=-static

libcourier_la_SOURCES=rw.h rwint.h addrlower.c cgethostname.c courier_malloc.c \
	cdefaultdelivery.c cdomaincmp.c cfilename.c cmaildropfilter.c \
	cmaildropmda.c \
	cread1l.c cme.c cdefaultdomain.c chelohost.c cfilteracct.c \
	cmsgidhost.c courierdir.c islocal.c islocalt.c maxlongsize.h \
	readfile.c removecomments.c lcclog.c lcclog2.c lcclog3.c \
	smtproutes.h smtproutes.c \
	rwdefaulthost.c rwsearch.c lcrewrite.c
libcourier_la_LDFLAGS=-static

#  Testaliases

testaliases_SOURCES=testaliases.C
testaliases_DEPENDENCIES=libcommon.la libcourier.la ../../afx/libafx.a @dblibrary@
testaliases_LDADD=libcommon.la libcourier.la ../../afx/libafx.a @dblibrary@

#  libmodule.a uses either shared or static transport modules

MODULESTATIC=lcrwstatic.c lcrwstaticlist.c lcrwstaticlist.h
MODULESTATICH=lcrwstatic.h

MODULE=$(MODULESTATICH) $(MODULESTATIC)
libmodule_la_DEPENDENCIES=$(MODULESTATICH)
libmodule_la_SOURCES=$(MODULE) moduledel.h moduledel.c
libmodule_la_LDFLAGS=-static

EXTRA_DIST=$(MODULESTATIC)

#
# lcrwstatic.h is dynamically built to link with initialization functions
# of all the selected transports
#

lcrwstatic.h: config.status
	@echo "Creating lcrwstatic.h"
	@echo '' >lcrwstatic.h
	@for t in @transport@ ; do echo "DECLARE_STATICFUNCS($$t)" >>lcrwstatic.h ; done
	@echo 'struct rw_static_info rw_static[] = {' >>lcrwstatic.h
	@for t in @transport@ ; do echo "LIST_STATICFUNCS($$t)," >>lcrwstatic.h ; done
	@echo ' {0, 0, 0} };' >>lcrwstatic.h
	@rm -f lcrwstaticlist.o


# Define dependencies on lcrwstatic.o

lcrwstaticlist.o: lcrwstaticlist.c lcrwstatic.h

sysconfdir.h: config.status
	echo "#define SYSCONFDIR \"$(sysconfdir)\"" >sysconfdir.h

localstatedir.h: config.status
	echo "#define LOCALSTATEDIR \"$(localstatedir)\"" >localstatedir.h

libexecdir.h: config.status
	echo "#define LIBEXECDIR \"$(libexecdir)\"" >libexecdir.h

sbindir.h: config.status
	echo "#define SBINDIR \"$(sbindir)\"" >sbindir.h

dist-hook:
	rm -f $(distdir)/lcrwstatic.h

check-am:
	./testaliases
