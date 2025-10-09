#
# Copyright 2002-2009, Double Precision Inc.
#
# See COPYING for distribution information.
#

noinst_LTLIBRARIES=libmail.la
noinst_PROGRAMS=mailtool testsuite autodeps

BUILT_SOURCES=
DISTCLEANFILES=
EXTRA_DIST=

AM_CFLAGS = $(LIBIDN_CFLAGS)
AM_CXXFLAGS = @COURIER_UNICODE_CXXFLAGS@

libmail_la_SOURCES=misc.H mail.H mail.C \
			addmessage.C addmessage.H \
			addmessageimport.C \
			addmessageremoveattachments.C \
			addressbook.H addressbook.C \
			addressbookadd.H addressbookadd.C \
			addressbookget.H addressbookget.C \
			addressbookopen.H addressbookopen.C \
			addressbooksearch.H addressbooksearch.C \
			attachments.C attachments.H \
			autodecoder.C autodecoder.H \
			envelope.C envelope.H \
			base64.C base64.H \
			copymessage.C copymessage.H \
			decoder.C decoder.H \
			driver.C driver.H \
			expungelist.C expungelist.H \
			fd.C fd.H \
			fdtls.C fdtls.H \
			file.C file.H \
			generic.C generic.H \
			genericdecode.C genericdecode.H \
			headers.C headers.H \
			imap.C imap.H \
			imapacl.C imapacl.H \
			imapidle.C imapidle.H \
			imaplogin.C imaplogin.H \
			imaplogout.C \
			imapfetchhandler.C imapfetchhandler.H \
			imaphandler.C imaphandler.H \
			imaphmac.C imaphmac.H \
			imapfolder.C imapfolder.H \
			imapfolders.C imapfolders.H \
			imaplisthandler.C imaplisthandler.H \
			imapparsefmt.C imapparsefmt.H \
			imapstatushandler.C imapstatushandler.H \
			logininfo.C logininfo.H \
			maildir.C maildir.H \
			maildiradd.C maildiradd.H \
			maildirfolder.C maildirfolder.H \
			mbox.C mbox.H \
			mboxadd.C mboxadd.H \
			mboxexpunge.C mboxexpunge.H \
			mboxfolder.C \
			mboxgetmessage.C mboxgetmessage.H \
			mboxlock.C mboxlock.H \
			mboxmagictag.C mboxmagictag.H \
			mboxmultilock.C mboxmultilock.H \
			mboxopen.C mboxopen.H \
			mboxread.C mboxread.H \
			mboxsighandler.C mboxsighandler.H \
			mimetypes.C mimetypes.H \
			namespace.H \
			nntp.C nntp.H \
			nntpadd.C nntpadd.H \
			nntpcache.C nntpcache.H \
			nntpchecknew.C nntpchecknew.H \
			nntpfetch.C nntpfetch.H \
			nntpfolder.C nntpfolder.H \
			nntpgroup.C nntpgroup.H \
			nntpgroupinfo.C nntpgroupinfo.H \
			nntpgroupopen.C nntpgroupopen.H \
			nntplistactive.C nntplistactive.H \
			nntplogin.C nntplogin.H \
			nntplogin2.C nntplogin2.H \
			nntplogout.C nntplogout.H \
			nntpnewsrc.C nntpnewsrc.H \
			nntppost.C nntppost.H \
			nntpxover.C nntpxover.H \
			nntpxpat.C nntpxpat.H \
			objectmonitor.C objectmonitor.H \
			poll.C \
			pop3.C pop3.H \
			pop3folder.C \
			pop3maildrop.C pop3maildrop.H \
			qp.C qp.H \
			rfc2047decode.C rfc2047decode.H \
			rfcaddr.C rfcaddr.H \
			runlater.C runlater.H \
			search.C search.H \
			smap.C smap.H \
			smapacl.C smapacl.H \
			smapadd.C smapadd.H \
			smapaddmessage.C smapaddmessage.H \
			smapcopy.C smapcopy.H \
			smapcreate.C smapcreate.H \
			smapdelete.C smapdelete.H \
			smapfetch.C smapfetch.H \
			smapfetchattr.C smapfetchattr.H \
			smapidle.C smapidle.H \
			smaplist.C smaplist.H \
			smapmsgrange.C smapmsgrange.H \
			smapnewmail.C smapnewmail.H \
			smapnoopexpunge.C smapnoopexpunge.H \
			smapopen.C smapopen.H \
			smapsearch.C smapsearch.H \
			smapsendfolder.C smapsendfolder.H \
			smapstatus.C smapstatus.H \
			smapstore.C smapstore.H \
			smtp.C smtp.H \
			smtpfolder.H smtpfolder.C \
			smtpinfo.H \
			snapshot.C snapshot.H \
			sortfolders.C \
			structure.C structure.H \
			sync.C sync.H \
			tmpaccount.C tmpaccount.H \
			tmpaccountadd.C tmpaccountfolder.C
libmail_la_LIBADD = $(LIBIDN_LIBS)

mailtool_SOURCES=mailtool.C
mailtool_DEPENDENCIES=libmail.la \
	../libs/rfc2045/librfc2045.la \
	../libs/rfc822/librfc822.la ../libs/rfc2045/libencode.la \
	../libs/maildir/libmaildir.la \
	@LIBCOURIERTLS@ \
	../libs/liblock/liblock.la \
	../libs/libhmac/libhmac.la ../libs/md5/libmd5.la ../libs/sha1/libsha1.la \
	../libs/numlib/libnumlib.la ../libs/tcpd/libspipe.la ../libs/soxwrap/libsoxwrap.a \
	../libs/soxwrap/soxlibs.dep

mailtool_LDADD=libmail.la ../libs/rfc2045/librfc2045.la \
	../libs/rfc822/librfc822.la ../libs/rfc2045/libencode.la \
	../libs/maildir/libmaildir.la \
	@LIBCOURIERTLS@ -lcourier-unicode \
	../libs/liblock/liblock.la \
	../libs/libhmac/libhmac.la ../libs/md5/libmd5.la ../libs/sha1/libsha1.la \
	../libs/numlib/libnumlib.la ../libs/tcpd/libspipe.la ../libs/soxwrap/libsoxwrap.a \
	`cat ../libs/soxwrap/soxlibs.dep` -lpthread
mailtool_LDFLAGS=-static

testsuite_SOURCES=testsuite.C
testsuite_LDADD=-lcourier-unicode

EXTRA_DIST += testsuite.txt

check-am:
	./testsuite | cmp -s - $(srcdir)/testsuite.txt

install-data-hook: @INSTINCLUDES@

uninstall-local: @UNINSTINCLUDES@

autodeps_SOURCES=autodeps.C

# Build autodeps.includes in maintainer mode, and include it in the tarball
# so it does not have to be rebuilt.

BUILT_SOURCES += autodeps.includes
DISTCLEANFILES += autodeps.includes
EXTRA_DIST    += autodeps.includes

autodeps.includes: autodeps.o
	test -f .deps/autodeps.Po || test -f autodeps.includes
	test -f .deps/autodeps.Po || touch autodeps.includes
	test -f .deps/autodeps.Po || exit 0; \
	set -x; rm -f autodeps.includes.tmp; \
	for f in `sed 's/.*://' <.deps/autodeps.Po | tr -d '\\\\' | tr ' ' '\\012' | sed '/^\//d;/autodeps.C/d' | sort | uniq` ; do \
		origf="$$f"; case $$f in \
		*) \
			f="`echo $$f | sed 's:../libs/::'`" ;\
			echo $$origf libmail/$$f >> autodeps.includes.tmp;\
			;; \
		esac ; done; \
	mv -f autodeps.includes.tmp autodeps.includes

install-includes: autodeps.includes
	cat $(srcdir)/autodeps.includes | while read F T; do \
		$(MKDIR_P) "`dirname $(DESTDIR)$(includedir)/$$T`"; \
		$(INSTALL_DATA) `test -f $$F || echo $(srcdir)/`$$F $(DESTDIR)$(includedir)/$$T; done

uninstall-includes:
	rm -rf $(DESTDIR)$(includedir)/libmail

BUILT_SOURCES += libcouriertls.h
DISTCLEANFILES += libcouriertls.h mimetypefiles.h

libcouriertls.h: ../libs/tcpd/couriertls.config
	cp /dev/null libcouriertls.h ; . ../libs/tcpd/couriertls.config ; \
		test "$$couriertls" != "" || exit 0 ; \
		echo '#define HAVE_LIBCOURIERTLS 1' >libcouriertls.h
