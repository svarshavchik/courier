#
# Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
# distribution information.

noinst_LTLIBRARIES=libuserdb.la

libuserdb_la_SOURCES=userdb.c userdb.h userdb2.c userdbmkpw.c

BUILT_SOURCES=	makeuserdb.html.in makeuserdb.8.in \
		userdb.html.in userdb.8.in \
		userdbpw.html.in userdbpw.8.in

noinst_SCRIPTS=makeuserdb pw2userdb dummy
noinst_PROGRAMS=userdbpw
noinst_DATA=makeuserdb.html userdb.html userdbpw.html

userdbpw_SOURCES=userdbpw.c
userdbpw_LDADD=libuserdb.la @HMACLIB@ @MD5LIB@ @SHA1LIB@ @CRYPTLIBS@
userdbpw_LDFLAGS=-static

man8=makeuserdb.8 userdb.8 userdbpw.8 pw2userdb.8
man_MANS=$(man8)

CLEANFILES=$(man8) $(noinst_DATA) dummy pw2userdb.8

makeuserdb.html: makeuserdb.html.in
	./config.status --file=$@

makeuserdb.8: makeuserdb.8.in
	./config.status --file=$@

userdb.html: userdb.html.in
	./config.status --file=$@

userdb.8: userdb.8.in
	./config.status --file=$@

userdbpw.html: userdbpw.html.in
	./config.status --file=$@

userdbpw.8: userdbpw.8.in
	./config.status --file=$@

pw2userdb.8: makeuserdb.8
	echo ".so man8/makeuserdb.8" >pw2userdb.8

if HAVE_SGML
makeuserdb.html.in: makeuserdb.sgml ../libs/docbook/sgml2html
	../libs/docbook/sgml2html makeuserdb.sgml makeuserdb.html.in

makeuserdb.8.in: makeuserdb.sgml ../libs/docbook/sgml2man
	../libs/docbook/sgml2man makeuserdb.sgml makeuserdb.8.in
	mv makeuserdb.8 makeuserdb.8.in

userdb.html.in: userdb.sgml ../libs/docbook/sgml2html
	../libs/docbook/sgml2html userdb.sgml userdb.html.in

userdb.8.in: userdb.sgml ../libs/docbook/sgml2man
	../libs/docbook/sgml2man userdb.sgml userdb.8.in
	mv userdb.8 userdb.8.in

userdbpw.html.in: userdbpw.sgml ../libs/docbook/sgml2html
	../libs/docbook/sgml2html userdbpw.sgml userdbpw.html.in

userdbpw.8.in: userdbpw.sgml ../libs/docbook/sgml2man
	../libs/docbook/sgml2man userdbpw.sgml userdbpw.8.in
	mv userdbpw.8 userdbpw.8.in

endif

EXTRA_DIST=$(BUILT_SOURCES)

# Temporary autoconf kludge:

dummy: $(man8)
	touch dummy
