#
# Copyright 2003-2008, Double Precision Inc.
#
# See COPYING for distribution information.
#
SUBDIRS=intl po

noinst_PROGRAMS=cone leaf htmlparsertest pwtest
noinst_SCRIPTS=cone.sh cone.dist

HTMLMACROFILES=cone.html

MANMACROFILES=man1/cone.1

noinst_DATA=help.txt http.handler

AUTOMAKE_OPTIONS=dist-bzip2

IMAGEFILTERS=IMAGE.GIF.filter IMAGE.JPEG.filter IMAGE.PNG.filter

pkgdata_SCRIPTS=APPLICATION.PDF.filter

cone_SOURCES= acl.C acl.H addressbook.C addressbook.H \
	addressbookinterface.C addressbookinterface.H \
	addressbookinterfaceldap.C addressbookinterfaceldap.H \
	addressbookinterfacemail.C addressbookinterfacemail.H \
	cache.H \
	certificates.C certificates.H \
	colors.C colors.H colors_inc.h \
	cone.C \
	configscreen.C configscreen.H \
	ctrlchandler.C ctrlchandler.H \
	cursesaddresslist.C cursesaddresslist.H \
	cursesattachmentdisplay.C cursesattachmentdisplay.H \
	cursesedit.C cursesedit.H \
	curseseditmessage.C curseseditmessage.H \
	curseshierarchy.C curseshierarchy.H \
	cursesindexdisplay.C cursesindexdisplay.H \
	cursesmessage.C cursesmessage.H \
	cursesmessagedisplay.C cursesmessagedisplay.H \
	cursesmessageflowedtext.C cursesmessageflowedtext.H \
	cursesmessagehtmlparser.C cursesmessagehtmlparser.H \
	disconnectcallbackstub.C disconnectcallbackstub.H \
	encryptionmenu.C encryptionmenu.H \
	filter.C filter.H \
	filtereditscreen.C filtereditscreen.H \
	fkeytraphandler.C fkeytraphandler.H \
	gettext.C gettext.H \
	globalkeys.C globalkeys.H \
	gpg.C gpg.H \
	hierarchy.C hierarchy.H \
	htmlentity.C htmlentity.h \
	htmlparser.C htmlparser.H \
	init.C init.H \
	macros.C macros.H \
	mainmenu.C mainmenu.H \
	menuscreen.C menuscreen.H \
	messagesize.C messagesize.H \
	myfolder.C myfolder.H \
	myfolderfilter.C myfolderfilter.H \
	mymessage.C mymessage.H \
	myreferences.C myreferences.H \
	myserver.C myserver.H \
	myservercallback.C myservercallback.H \
	myserverconfig.C \
	myserverlogincallback.C myserverlogincallback.H \
	myserverpromptinfo.C myserverpromptinfo.H \
	myserverremoteconfig.C myserverremoteconfig.H \
	myservertask.C myservertask.H \
	myreadfolders.C myreadfolders.H \
	nntpcommand.C nntpcommand.H \
	opendialog.C opendialog.H \
	opensubfolders.C opensubfolders.H \
	outputdialog.C outputdialog.H \
	passwordlist.C passwordlist.H \
	previousscreen.C previousscreen.H \
	savedialog.C savedialog.H \
	searchcallback.C searchcallback.H \
	searchprompt.C searchprompt.H \
	specialfolder.C specialfolder.H \
	spellchecker.H \
	spellcheckerbase.C spellcheckerbase.H \
	tags.C tags.H \
	typeahead.C typeahead.H

leaf_SOURCES=\
	curseseditmessage.C \
	fkeytraphandler.C \
	gettext.C \
	htmlentity.C \
	init.C \
	leaf.C \
	macros.C macros.H \
	messagesize.C \
	myserverpromptinfo.C \
	opendialog.C \
	savedialog.C \
	spellcheckerbase.C

htmlparsertest_SOURCES=htmlparsertest.C htmlparser.C htmlparser.H \
			htmlentity.C htmlentity.h
htmlparsertest_LDADD=../curses/libcurses.a -lcourier-unicode

check-am:
	./htmlparsertest <$(srcdir)/testsuite1 | cmp -s - $(srcdir)/testsuite1.out
	./pwtest | cmp -s - $(srcdir)/pwtest.txt

man3src=$(srcdir)/man/*.3x
man1src=$(srcdir)/man/*.1

man1_MANS=$(man1src)
man3_MANS=@MAN3@
lib_LTLIBRARIES=@LIBMAIL@

EXTRA_MANS=$(man3src) $(man5src)
EXTRA_LTLIBRARIES=libmail.la

noinst_DATA += html/index.html html/manpage.css man/.timestamp

LAMAILLIBS=../libmail/libmail.la ../libs/maildir/libmaildir.la \
	../libs/rfc2045/librfc2045.la \
	../libs/tcpd/libspipe.la @LIBCOURIERTLS@ \
	../libs/rfc822/librfc822.la ../libs/rfc822/libencode.la \
	../libs/liblock/liblock.la \
	../libs/libhmac/libhmac.la ../libs/md5/libmd5.la ../libs/sha1/libsha1.la \
	../libs/numlib/libnumlib.la

libmail_la_SOURCES=
libmail_la_LIBADD=$(LAMAILLIBS) -lcourier-unicode
libmail_la_DEPENDENCIES=$(LAMAILLIBS)
libmail_la_LDFLAGS=-static -rpath $(libdir)

#libmail.a: $(AMAILLIBS) $(LAMAILLIBS)
#	rm -rf libmail.a.tmp
#	rm -f libmail.a
#	mkdir libmail.a.tmp
#	cd libmail.a.tmp ; \
#	for l in $(AMAILLIBS); do N=`echo $$l | sed 's:\.\./::;s/\/.*//'` ; test -d $$N || mkdir $$N || exit 1 ; cd $$N && $(AR) x ../../$$l && cd .. ; done ;\
#	for l in $(LAMAILLIBS); do N=`echo $$l | sed 's:\.\./::;s/\/.*//'` ; test -d $$N || mkdir $$N || exit 1 ; . ../$$l; cd $$N && $(AR) x ../../`echo $$l | sed 's:[^/]*$$:.libs:'`/$$old_library && cd .. ; done ;\
#	for l in * ; do for N in `ls $$l` ; do mv $$l/$$N $${l}_$$N ; done ; rmdir $$l ; done ; $(AR) cr ../libmail.a *
#	rm -rf libmail.a.tmp
#	$(RANLIB) libmail.a

# Spell checker

SPELLSRC=spellcheckerAspell.C spellcheckerAspell.H \
	spellcheckerPspell.C spellcheckerPspell.H \
	spellcheckerNone.C spellcheckerNone.H

BUILT_SOURCES=helpfile.H buildversion.H colors_inc.h

if HAVE_SGML

GITFILES=cone.dist.in

BUILT_SOURCES += $(GITFILES)

$(GITFILES): %: %.git
	perl -p -e 's/\$$Id:[^\$$]*/\$$Id: '"`git log --pretty=format:\"%H\" -n 1 -- $<`-`date +%Y%m%d%H%M%S`/" <$< >$@.tmp
	mv -f $@.tmp $@
endif

helpfile.H: config.status
	h='#define HELPFILE "${pkgdatadir}/cone.hlp"'; echo "$$h" | cmp -s - helpfile.H && exit 0; echo "$$h" >helpfile.H

buildversion.H: config.status
	echo '#define BUILDVERSION "@target_cpu@-@target_vendor@-@target_os@/'`date`'"' >buildversion.H

cone_DEPENDENCIES=../libs/tcpd/libtlsclient.la ../curses/libcurses.a \
	../libs/gpglib/libgpg.la \
	libmail.la \
	dummyspellchecker.a ../libs/soxwrap/libsoxwrap.a \
	@LDAPSEARCH@ \
	../libs/soxwrap/soxlibs.dep @SPELL_OBJ@

cone_LDADD=@SPELL_OBJ@ $(SPELL_LIB) ../libs/tcpd/libtlsclient.la \
	../curses/libcurses.a ../libs/gpglib/libgpg.la libmail.la \
	@LDAPSEARCH@ \
	../libs/soxwrap/libsoxwrap.a \
	`cat ../curses/curseslib ../libs/soxwrap/soxlibs.dep` \
	 @LIBINTL@ @XMLLIBS@ \
	`cat ../libs/maildir/maildir.libdeps` \
	`. ../libs/tcpd/couriertls.config; echo $$cryptlibs`

leaf_DEPENDENCIES=../curses/libcurses.a dummyspellchecker.a @SPELL_OBJ@ \
	../libmail/objectmonitor.o \
	../libs/maildir/libmaildir.la

leaf_LDADD=@SPELL_OBJ@ $(SPELL_LIB) ../curses/libcurses.a \
	../libs/maildir/libmaildir.la \
	../libmail/objectmonitor.o \
	`cat ../curses/curseslib` \
	 @LIBINTL@ -lcourier-unicode

pwtest_SOURCES=pwtest.c
pwtest_DEPENDENCIES=../libs/tcpd/libtlsclient.la \
	../libs/md5/libmd5.la
pwtest_LDADD=../libs/tcpd/libtlsclient.la ../libs/md5/libmd5.la \
	`. ../libs/tcpd/couriertls.config; echo $$cryptlibs`
pwtest_LDFLAGS=-static

dummyspellchecker.a: $(ALLSPELLOBJ)
	rm -f dummyspellchecker.a
	$(AR) cr dummyspellchecker.a $(ALLSPELLOBJ)
	$(RANLIB) dummyspellchecker.a

dist-hook: $(noinst_DATA)
	$(mkinstalldirs) $(distdir)/html
	d=`test -d $(srcdir)/html && echo $(srcdir)/html || echo html` ; \
		ln $$d/* $(distdir)/html
	$(mkinstalldirs) $(distdir)/man
	d=`test -d $(srcdir)/man && echo $(srcdir)/man || echo man` ; \
		ln $$d/*.[35]x $$d/*.1 $$d/.timestamp $(distdir)/man

help.txt: config.status html/index.html
	for f in $(srcdir)/html/cone[0-9]*html; do @PERL@ $(srcdir)/mkhtmlmsg.pl <$$f ; done >help.tmp
	rm -f help.txt
	../libmail/mailtool -copyto mbox:`pwd`/help.txt -tofolder "" -fromfolder "" mbox:`pwd`/help.tmp
	sed 's:^From [^ ][^ ]*:From nobody@localhost:' <help.txt >help.tmp
	mv -f help.tmp help.txt.in
	$(SHELL) $(top_builddir)/config.status --file=help.txt
	rm -f help.txt.in

if HAVE_SGML

$(man1src) $(man3src) $(man5src) html/index.html html/readme.html html/install.html: sgmltimestamp

sgmltimestamp: sgml/*.sgml
	rm -rf html
	$(mkinstalldirs) html
	cp -p icon.gif html
	xsltproc -o html/ $(srcdir)/cone.xsl $(srcdir)/sgml/book.sgml
	for f in html/*.html; do xsltproc --nonet $(srcdir)/../libs/docbook/fixhtml.xsl $$f | tidy -i -q --doctype transitional --input-encoding utf8 --output-encoding ascii -asxml >html/.t; xsltproc --nonet $(srcdir)/postproc.xsl html/.t >$$f; done
	rm -f html/.t
	for f in html/cone*html ; do @PERL@ $(srcdir)/fixonlinehelp.pl $$f ; done
	touch sgmltimestamp

html/manpage.css: sgmltimestamp
	cp -p manpage.css html

man/.timestamp: sgmltimestamp
	$(mkinstalldirs) man
	rm -f man/*
	xsltproc --nonet --stringparam man.output.in.separate.dir 1 \
		--stringparam man.output.subdirs.enabled 0 \
		http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl \
		 $(srcdir)/sgml/book.sgml
	for f in man/* ; do sed -n '20,$$d;/<!--/p' <$(srcdir)/sgml/book.sgml | sed 's/\"/ /g;s/^/\.\\\"/g' >man/.tmp; \
		cat $$f >>man/.tmp; \
		mv man/.tmp $$f; done
	>man/.timestamp

onlinehelp: help.txt
	cp help.txt $$HOME/Mail/libmail
endif


install-data-local: install-man
	$(mkinstalldirs) $(DESTDIR)$(htmldatadir)
	${INSTALL} -m 444 $(srcdir)/html/* $(DESTDIR)$(htmldatadir)
	for f in $(HTMLMACROFILES) ; do mv -f $(DESTDIR)$(htmldatadir)/$$f $(DESTDIR)$(htmldatadir)/$$f.in ; $(SHELL) $(top_builddir)/config.status --file=$(DESTDIR)$(htmldatadir)/$$f ; rm -f $(DESTDIR)$(htmldatadir)/$$f.in ; done
	for f in $(MANMACROFILES) ; do mv -f $(DESTDIR)$(mandir)/$$f $(DESTDIR)$(mandir)/$$f.in ; $(SHELL) $(top_builddir)/config.status --file=$(DESTDIR)$(mandir)/$$f; rm -f $(DESTDIR)$(mandir)/$$f.in ; done

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(htmldatadir)
	${INSTALL} -m 755 IMAGE.filter $(DESTDIR)$(pkgdatadir)/IMAGE.filter
	for f in $(IMAGEFILTERS) ; do rm -f $(DESTDIR)$(pkgdatadir)/$$f ; ln $(DESTDIR)$(pkgdatadir)/IMAGE.filter $(DESTDIR)$(pkgdatadir)/$$f; done
	rm -f $(DESTDIR)$(pkgdatadir)/IMAGE.filter
	rm -f $(DESTDIR)$(pkgdatadir)/http.handler
	rm -f $(DESTDIR)$(pkgdatadir)/https.handler
	$(INSTALL) -m 555 http.handler $(DESTDIR)$(pkgdatadir)/http.handler
	ln $(DESTDIR)$(pkgdatadir)/http.handler $(DESTDIR)$(pkgdatadir)/https.handler

uninstall-local:
	l=`ls $(srcdir)/html`; cd $(DESTDIR)$(htmldatadir) && rm -f $$l
	for f in $(IMAGEFILTERS) ; do rm -f $(DESTDIR)$(pkgdatadir)/$$f; done
	rm -f $(DESTDIR)$(pkgdatadir)/http.handler
	rm -f $(DESTDIR)$(pkgdatadir)/https.handler

EXTRA_DIST = ../config.rpath testsuite1 testsuite1.out pwtest.txt colors.txt mkcolors.pl \
	fixonlinehelp.pl mkhtmlmsg.pl postproc.xsl $(SPELLSRC) \
	m4/ChangeLog $(srcdir)/m4/*.m4

colors_inc.h: colors.txt mkcolors.pl
	@PERL@ $(srcdir)/mkcolors.pl <$(srcdir)/colors.txt >colors_inc.h

CLEANFILES=dummyspellchecker.a libmail.la man.timestamp \
          helpfile.H help.txt
DISTCLEANFILES=sgmltimestamp buildversion.H

ACLOCAL_AMFLAGS = -I m4
