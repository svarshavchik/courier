#
# Copyright 1998 - 2021 S. Varshavchik.  See COPYING for
# distribution information.

noinst_LTLIBRARIES=libcommon.la libcourier.la libmodule.la
noinst_PROGRAMS=testaliases testconfig

BUILT_SOURCES=sysconfdir.h localstatedir.h libexecdir.h sbindir.h lcrwstatic.h \
	courier_version.h

CLEANFILES=$(BUILT_SOURCES) permissions.dat

courier_version.h: ../../version
	echo '#define COURIER_VERSION "'`cat ../../version`'"' >courier_version.h

libcommon_la_SOURCES= aliases.h \
	comaliases1.C comaliases2.C \
	comaliases3.C comaliases4.C combatchsize.c comcargs.c comcargs.h \
	comconfig.c comconfig.h comctlfile.c \
	comctlfile.h comdsn.c comdsn2.c comerrmsg.c \
	commsgcancel.c commsgcancel.h comesmtpidstring.c comfax.C comfax.h \
	comparseqid.c comparseqid.h comqueuename.c comqueuename.h \
	comqueuename2.c \
	comreadtime.c comreadtime.h comrwerrfunc.C comrwheader.C \
	comrwinfoinit.C comrwinstall.C comrwinstall2.c comrwsearchdel.C \
	comsecurity.c \
	comsizelimit.c comstatfs.c comstrinode.c comstrinode.h \
	comstrtimestamp.c comstrtimestamp.h comstrtotime.c comstrtotime.h \
	comsubmitclient.c comsubmitclient.h comsubmitclient2.c comtmpfile.c \
	comtmpfile2.c comtrack.c comtrack.h \
	comtrigger.c comuidgid.C comuidgid.h comverp.c comverp.h mybuf.h
libcommon_la_LDFLAGS=-static

libcourier_la_SOURCES=rw.h addrlower.c cgethostname.c \
	cgethostname_sys.c \
	courier_malloc.c \
	cdefaultsep.c \
	cdefaultdelivery.c cdomaincmp.c cfilename.c cmaildropfilter.c \
	comsts.c comsts2.c comsts.h \
	cmaildropmda.c \
	cread1l.c cme.C cdefaultdomain.C chelohost.c cfilteracct.c \
	cmsgidhost.c courierdir.c islocal.c islocalt.C maxlongsize.h \
	readfile.c removecomments.c lcclog.c lcclog2.c lcclog3.c \
	rwdefaulthost.C rwsearch.C lcrewrite.C

#  Testaliases

testaliases_SOURCES=testaliases.C
testaliases_DEPENDENCIES=libcommon.la libcourier.la ../../afx/libafx.a @dblibrary@
testaliases_LDADD=libcommon.la libcourier.la ../../afx/libafx.a @dblibrary@

# testconfig

testconfig_SOURCES=testconfig.C
testconfig_DEPENDENCIES=@dblibrary@
testconfig_LDADD=@LIBIDN_LIBS@ -lcourier-unicode @dblibrary@

#  libmodule.a uses either shared or static transport modules

MODULESTATIC=lcrwstatic.C lcrwstaticlist.C lcrwstaticlist.h
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
	@rm -f lcrwstaticlist.lo


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
	@VALGRIND@ ./testaliases
	@VALGRIND@ ./testconfig
