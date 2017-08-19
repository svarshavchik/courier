#
# Copyright 1998 - 2009 Double Precision, Inc.  See COPYING for
# distribution information.


htmldocdir=@datadir@/htmldoc

CLEANFILES=permissions.dat

miscfiles=japanese_flag.png icon.gif bg.png manpage.css \
		draft-varshavchik-exdata-smtpext.txt \
		draft-varshavchik-verp-smtpext.txt \
		draft-varshavchik-security-smtpext.txt

nohtmlinfiles=  aliases.html \
		courierd.html \
		courierdsn.html \
		documentation.html \
		download.html \
		esmtp.html \
		FAQ.html \
		socks.html \
		index.html \
		install.html \
		layout.html \
		links.html \
		local.html \
		menu.html \
		modules.html \
		queue.html \
		rpm.html \
		status.html \
		structures.html \
		repo.html

nohtmlin=$(miscfiles) $(nohtmlinfiles)

htmlin=\
		aliaslookup.html \
		cancelmsg.html \
		courier.html \
		courierldapaliasd.html \
		couriermlm.html \
		courierpop3d.html \
		couriertcpd.html \
		couriertls.html \
		deliverquota.html \
		dot-courier.html \
		dot-forward.html \
		esmtpd.html \
		imapd.html \
		localmailfilter.html \
		lockmail.html \
		mailbot.html \
		maildiracl.html \
		maildir.html \
		maildirkw.html \
		maildirmake.html \
		maildirquota.html \
		maildropex.html \
		maildropfilter.html \
		maildropgdbm.html \
		maildrop.html \
		maildrop.README.html \
		maildroptips.html \
		mailq.html \
		makeacceptmailfor.html \
		makealiases.html \
		makedat.html \
		makehosteddomains.html \
		makeimapaccess.html \
		makemime.html \
		makepercentrelay.html \
		makesmtpaccess.html \
		mimegpg.html \
		mkdhparams.html \
		mkesmtpdcert.html \
		mkimapdcert.html \
		mkpop3dcert.html \
		pcp_README.html \
		pop3d.html \
		preline.html \
		reformail.html \
		reformime.html \
		sendmail.html \
		submit.html \
		testmxlookup.html \
		webmlmd.html


htmldoc_DATA=	$(nohtmlin) $(htmlin)

couriertcpd.html: ../../libs/tcpd/couriertcpd.html
	cp -f $(srcdir)/../../libs/tcpd/couriertcpd.html .

couriertcpd.1: ../../libs/tcpd/couriertcpd.1
	cp -f $(srcdir)/../../libs/tcpd/couriertcpd.1 .

couriertls.html: ../../libs/tcpd/couriertls.html
	cp -f $(srcdir)/../../libs/tcpd/couriertls.html .

couriertls.1: ../../libs/tcpd/couriertls.1
	cp -f $(srcdir)/../../libs/tcpd/couriertls.1 .

maildirmake.html: ../../libs/maildir/maildirmake.html
	cp -f ../../libs/maildir/maildirmake.html .

maildiracl.html: ../../libs/maildir/maildiracl.html
	cp -f ../../libs/maildir/maildiracl.html .

maildirkw.html: ../../libs/maildir/maildirkw.html
	cp -f $(srcdir)/../../libs/maildir/maildirkw.html .

maildirmake.1: ../../libs/maildir/maildirmake.1
	cp -f ../../libs/maildir/maildirmake.1 .

maildiracl.1: ../../libs/maildir/maildiracl.1
	cp -f ../../libs/maildir/maildiracl.1 .

maildirkw.1: ../../libs/maildir/maildirkw.1
	cp -f $(srcdir)/../../libs/maildir/maildirkw.1 .

maildir.html: ../../libs/maildir/maildir.html
	cp -f $(srcdir)/../../libs/maildir/maildir.html .

maildir.5: ../../libs/maildir/maildir.5
	cp -f $(srcdir)/../../libs/maildir/maildir.5 .

maildirquota.html: $(srcdir)/../../libs/maildir/maildirquota.html
	cp -f $(srcdir)/../../libs/maildir/maildirquota.html .

maildirquota.7: $(srcdir)/../../libs/maildir/maildirquota.7
	cp -f $(srcdir)/../../libs/maildir/maildirquota.7 .

makeimapaccess.8: ../../libs/imap/makeimapaccess.8
	cp -f ../../libs/imap/makeimapaccess.8 .

deliverquota.html: ../../libs/maildir/deliverquota.html
	cp -f ../../libs/maildir/deliverquota.html .

deliverquota.8: ../../libs/maildir/deliverquota.8
	cp -f ../../libs/maildir/deliverquota.8 .

imapd.html: ../../libs/imap/imapd.html
	cp -f ../../libs/imap/imapd.html .

imapd.8: ../../libs/imap/imapd.8
	cp -f ../../libs/imap/imapd.8 .

maildrop.README.html: ../../libs/maildrop/README.html
	cp -f ../../libs/maildrop/README.html maildrop.README.html

lockmail.html: ../../libs/liblock/lockmail.html
	cp -f $(srcdir)/../../libs/liblock/lockmail.html .

lockmail.1: ../../libs/liblock/lockmail.1
	cp -f $(srcdir)/../../libs/liblock/lockmail.1 .

maildrop.html: ../../libs/maildrop/maildrop.html
	cp -f ../../libs/maildrop/maildrop.html .

maildrop.1: ../../libs/maildrop/maildrop.1
	cp -f ../../libs/maildrop/maildrop.1 .

maildropgdbm.html: ../../libs/maildrop/maildropgdbm.html
	cp -f $(srcdir)/../../libs/maildrop/maildropgdbm.html .

maildropgdbm.7: ../../libs/maildrop/maildropgdbm.7
	cp -f $(srcdir)/../../libs/maildrop/maildropgdbm.7 .

maildropex.html: ../../libs/maildrop/maildropex.html
	cp -f $(srcdir)/../../libs/maildrop/maildropex.html .

maildropex.7: ../../libs/maildrop/maildropex.7
	cp -f $(srcdir)/../../libs/maildrop/maildropex.7 .

maildroptips.html: ../../libs/maildrop/maildroptips.html
	cp -f $(srcdir)/../../libs/maildrop/maildroptips.html .

makedat.html: ../../libs/maildrop/makedat.html
	cp -f $(srcdir)/../../libs/maildrop/makedat.html .

makedat.1: ../../libs/maildrop/makedat.1
	cp -f $(srcdir)/../../libs/maildrop/makedat.1 .

maildropfilter.html: ../../libs/maildrop/maildropfilter.html
	cp -f ../../libs/maildrop/maildropfilter.html .

maildropfilter.7: ../../libs/maildrop/maildropfilter.7
	cp -f ../../libs/maildrop/maildropfilter.7 .

reformail.html: ../../libs/maildrop/reformail.html
	cp -f $(srcdir)/../../libs/maildrop/reformail.html .

reformail.1: ../../libs/maildrop/reformail.1
	cp -f $(srcdir)/../../libs/maildrop/reformail.1 .

makeimapaccess.html: ../../libs/imap/makeimapaccess.html
	cp -f ../../libs/imap/makeimapaccess.html .

makemime.html: ../../libs/rfc2045/makemime.html
	cp -f $(srcdir)/../../libs/rfc2045/makemime.html .

makemime.1: ../../libs/rfc2045/makemime.1
	cp -f $(srcdir)/../../libs/rfc2045/makemime.1 .

mailbot.html: ../../libs/maildrop/mailbot.html
	cp -f $(srcdir)/../../libs/maildrop/mailbot.html .

mailbot.1: ../../libs/maildrop/mailbot.1
	cp -f $(srcdir)/../../libs/maildrop/mailbot.1 .

reformime.html: ../../libs/rfc2045/reformime.html
	cp -f $(srcdir)/../../libs/rfc2045/reformime.html .

reformime.1: ../../libs/rfc2045/reformime.1
	cp -f $(srcdir)/../../libs/rfc2045/reformime.1 .

mimegpg.html: ../../libs/gpglib/mimegpg.html
	cp -f $(srcdir)/../../libs/gpglib/mimegpg.html mimegpg.html

mimegpg.1: ../../libs/gpglib/mimegpg.html
	cp -f $(srcdir)/../../libs/gpglib/mimegpg.1 mimegpg.1

mkimapdcert.html: ../../libs/imap/mkimapdcert.html
	cp -f ../../libs/imap/mkimapdcert.html .

mkimapdcert.8: ../../libs/imap/mkimapdcert.8
	cp -f ../../libs/imap/mkimapdcert.8 .

mkpop3dcert.html: ../../libs/imap/mkpop3dcert.html
	cp -f ../../libs/imap/mkpop3dcert.html .

mkpop3dcert.8: ../../libs/imap/mkpop3dcert.8
	cp -f ../../libs/imap/mkpop3dcert.8 .

courierpop3d.html: ../../libs/imap/courierpop3d.html
	cp -f ../../libs/imap/courierpop3d.html .

courierpop3d.8: ../../libs/imap/courierpop3d.8
	cp -f ../../libs/imap/courierpop3d.8 .

man1=sendmail.1 preline.1 maildirmake.1 cancelmsg.1 lockmail.1 maildrop.1 \
	mailq.1 mailbot.1 makemime.1 mimegpg.1 maildiracl.1 \
	makedat.1 reformail.1 reformime.1 couriermlm.1 testmxlookup.1 \
	dot-forward.1 couriertls.1 couriertcpd.1 maildirkw.1 \
	webmlmd.1

man5=dot-courier.5 maildir.5
man7=localmailfilter.7 maildirquota.7 maildropgdbm.7 maildropfilter.7 \
	maildropex.7
man8=aliaslookup.8 courier.8 courierldapaliasd.8 courierpop3d.8 \
	deliverquota.8 makeimapaccess.8 \
	esmtpd.8 makeacceptmailfor.8 makehosteddomains.8\
	mkesmtpdcert.8 mkdhparams.8 mkimapdcert.8 mkpop3dcert.8 \
	makealiases.8 makepercentrelay.8 makesmtpaccess.8 \
	pop3d.8 pop3d-ssl.8 submit.8 imapd.8

pcp_README.html: ../../libs/pcp/README.html
	cp -f $(srcdir)/../../libs/pcp/README.html pcp_README.html


BUILT_SOURCES= \
	aliaslookup.html aliaslookup.8 \
	cancelmsg.html cancelmsg.1 \
	courier.html.in courier.8.in \
	courierldapaliasd.html.in courierldapaliasd.8.in \
	couriermlm.html.in couriermlm.1.in \
	dot-courier.html.in dot-courier.5.in \
	dot-forward.html dot-forward.1 \
	esmtpd.html.in esmtpd.8.in \
	localmailfilter.html.in localmailfilter.7.in \
	mailq.html mailq.1 \
	makeacceptmailfor.html.in makeacceptmailfor.8.in \
	makealiases.html.in makealiases.8.in \
	makehosteddomains.html.in makehosteddomains.8.in \
	makepercentrelay.html.in makepercentrelay.8.in \
	makesmtpaccess.html.in makesmtpaccess.8.in \
	mkdhparams.html.in mkdhparams.8.in \
	mkesmtpdcert.html.in mkesmtpdcert.8.in \
	pop3d.html.in pop3d.8.in \
	preline.html.in preline.1.in \
	sendmail.html sendmail.1 \
	submit.html.in submit.8.in \
	testmxlookup.html testmxlookup.1 \
	webmlmd.html.in webmlmd.1.in

EXTRA_DIST=$(nohtmlin) $(BUILT_SOURCES) dotforward.1 pop3d-ssl.8

if HAVE_SGML

aliaslookup.html: aliaslookup.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html aliaslookup.sgml aliaslookup.html

aliaslookup.8: aliaslookup.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man aliaslookup.sgml aliaslookup.8 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"


courier.html.in: courier.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html courier.sgml courier.html.in

courier.8.in: courier.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man courier.sgml courier.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv courier.8 courier.8.in

cancelmsg.html: cancelmsg.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html cancelmsg.sgml cancelmsg.html

cancelmsg.1: cancelmsg.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man cancelmsg.sgml cancelmsg.1 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"

mailq.html: mailq.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html mailq.sgml mailq.html

mailq.1: mailq.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man mailq.sgml mailq.1 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"

couriermlm.html.in: couriermlm.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html couriermlm.sgml couriermlm.html.in

couriermlm.1.in: couriermlm.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man couriermlm.sgml couriermlm.1.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv couriermlm.1 couriermlm.1.in

webmlmd.html.in: webmlmd.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html webmlmd.sgml webmlmd.html.in

webmlmd.1.in: webmlmd.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man webmlmd.sgml webmlmd.1.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv webmlmd.1 webmlmd.1.in

dot-courier.html.in: dot-courier.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html dot-courier.sgml dot-courier.html.in

dot-courier.5.in: dot-courier.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man dot-courier.sgml dot-courier.5.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv dot-courier.5 dot-courier.5.in

courierldapaliasd.html.in: courierldapaliasd.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html courierldapaliasd.sgml courierldapaliasd.html.in

courierldapaliasd.8.in: courierldapaliasd.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man courierldapaliasd.sgml courierldapaliasd.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv courierldapaliasd.8 courierldapaliasd.8.in

esmtpd.html.in: esmtpd.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html esmtpd.sgml esmtpd.html.in

esmtpd.8.in: esmtpd.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man esmtpd.sgml esmtpd.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv esmtpd.8 esmtpd.8.in

localmailfilter.html.in: localmailfilter.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html localmailfilter.sgml localmailfilter.html.in

localmailfilter.7.in: localmailfilter.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man localmailfilter.sgml localmailfilter.7.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv localmailfilter.7 localmailfilter.7.in

makeacceptmailfor.html.in: makeacceptmailfor.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html makeacceptmailfor.sgml makeacceptmailfor.html.in

makeacceptmailfor.8.in: makeacceptmailfor.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man makeacceptmailfor.sgml makeacceptmailfor.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv makeacceptmailfor.8 makeacceptmailfor.8.in

makealiases.html.in: makealiases.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html makealiases.sgml makealiases.html.in

makealiases.8.in: makealiases.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man makealiases.sgml makealiases.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv makealiases.8 makealiases.8.in

makehosteddomains.html.in: makehosteddomains.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html makehosteddomains.sgml makehosteddomains.html.in

makehosteddomains.8.in: makehosteddomains.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man makehosteddomains.sgml makehosteddomains.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv makehosteddomains.8 makehosteddomains.8.in

submit.html.in: submit.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html submit.sgml submit.html.in

submit.8.in: submit.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man submit.sgml submit.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv submit.8 submit.8.in

pop3d.html.in: pop3d.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html pop3d.sgml pop3d.html.in

pop3d.8.in: pop3d.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man pop3d.sgml pop3d.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv pop3d.8 pop3d.8.in

mkdhparams.html.in: mkdhparams.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html mkdhparams.sgml mkdhparams.html.in

mkdhparams.8.in: mkdhparams.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man mkdhparams.sgml mkdhparams.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv mkdhparams.8 mkdhparams.8.in

mkesmtpdcert.html.in: mkesmtpdcert.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html mkesmtpdcert.sgml mkesmtpdcert.html.in

mkesmtpdcert.8.in: mkesmtpdcert.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man mkesmtpdcert.sgml mkesmtpdcert.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv mkesmtpdcert.8 mkesmtpdcert.8.in

makesmtpaccess.html.in: makesmtpaccess.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html makesmtpaccess.sgml makesmtpaccess.html.in

makesmtpaccess.8.in: makesmtpaccess.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man makesmtpaccess.sgml makesmtpaccess.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv makesmtpaccess.8 makesmtpaccess.8.in

makepercentrelay.html.in: makepercentrelay.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html makepercentrelay.sgml makepercentrelay.html.in

makepercentrelay.8.in: makepercentrelay.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man makepercentrelay.sgml makepercentrelay.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv makepercentrelay.8 makepercentrelay.8.in

sendmail.html: sendmail.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html sendmail.sgml sendmail.html

sendmail.1: sendmail.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man sendmail.sgml sendmail.1 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"

testmxlookup.html: testmxlookup.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html testmxlookup.sgml testmxlookup.html

testmxlookup.1: testmxlookup.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man testmxlookup.sgml testmxlookup.1 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"

dot-forward.html: dot-forward.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html dot-forward.sgml dot-forward.html

dot-forward.1: dot-forward.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man dot-forward.sgml dot-forward.1 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"

preline.html.in: preline.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html preline.sgml preline.html.in

preline.1.in: preline.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man preline.sgml preline.1.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv preline.1 preline.1.in

manpage.css: ../../libs/docbook/manpage.css
	cp -f ../../libs/docbook/manpage.css .

endif

courier.html: courier.html.in
	./config.status --file=$@

courier.8: courier.8.in
	./config.status --file=$@

couriermlm.html: couriermlm.html.in
	./config.status --file=$@

couriermlm.1: couriermlm.1.in
	./config.status --file=$@

webmlmd.html: webmlmd.html.in
	./config.status --file=$@

webmlmd.1: webmlmd.1.in
	./config.status --file=$@

dot-courier.html: dot-courier.html.in
	./config.status --file=$@

dot-courier.5: dot-courier.5.in
	./config.status --file=$@

courierldapaliasd.html: courierldapaliasd.html.in
	./config.status --file=$@

courierldapaliasd.8: courierldapaliasd.8.in
	./config.status --file=$@

esmtpd.html: esmtpd.html.in
	./config.status --file=$@

esmtpd.8: esmtpd.8.in
	./config.status --file=$@

localmailfilter.html: localmailfilter.html.in
	./config.status --file=$@

localmailfilter.7: localmailfilter.7.in
	./config.status --file=$@

makeacceptmailfor.html: makeacceptmailfor.html.in
	./config.status --file=$@

makeacceptmailfor.8: makeacceptmailfor.8.in
	./config.status --file=$@

makealiases.html: makealiases.html.in
	./config.status --file=$@

makealiases.8: makealiases.8.in
	./config.status --file=$@

makehosteddomains.html: makehosteddomains.html.in
	./config.status --file=$@

makehosteddomains.8: makehosteddomains.8.in
	./config.status --file=$@

submit.html: submit.html.in
	./config.status --file=$@

submit.8: submit.8.in
	./config.status --file=$@

pop3d.html: pop3d.html.in
	./config.status --file=$@

pop3d.8: pop3d.8.in
	./config.status --file=$@

mkdhparams.html: mkdhparams.html.in
	./config.status --file=$@

mkdhparams.8: mkdhparams.8.in
	./config.status --file=$@

mkesmtpdcert.html: mkesmtpdcert.html.in
	./config.status --file=$@

mkesmtpdcert.8: mkesmtpdcert.8.in
	./config.status --file=$@

makesmtpaccess.html: makesmtpaccess.html.in
	./config.status --file=$@

makesmtpaccess.8: makesmtpaccess.8.in
	./config.status --file=$@

makepercentrelay.html: makepercentrelay.html.in
	./config.status --file=$@

makepercentrelay.8: makepercentrelay.8.in
	./config.status --file=$@

preline.html: preline.html.in
	./config.status --file=$@

preline.1: preline.1.in
	./config.status --file=$@

install-man-extra:
	cd $(DESTDIR)$(man8dir) ; \
		rm -f courierpop3login.8 makesmtpaccess-msa.8 esmtpd-msa.8; \
		$(LN_S) courierpop3d.8 courierpop3login.8; \
		$(LN_S) makesmtpaccess.8 makesmtpaccess-msa.8 ; \
		$(LN_S) esmtpd.8 esmtpd-msa.8
	cd $(DESTDIR)$(man1dir) ; rm -f rmail.1 dotforward.1 \
		; $(LN_S) sendmail.1 rmail.1 \
		; $(LN_S) dot-forward.1 dotforward.1

uninstall-local:
	rm -f $(DESTDIR)$(man8dir)/esmtpd-msa.8
	rm -f $(DESTDIR)$(man8dir)/makesmtpaccess-msa.8
	rm -f $(DESTDIR)$(man8dir)/courierpop3login.8
	rm -f $(DESTDIR)$(man1dir)/rmail.1
	rm -f $(DESTDIR)$(man1dir)/dotforward.1

man_LINKS=$(man8dir)/courierpop3login.8 \
	$(man1dir)/rmail.1 $(man1dir)/dotforward.1 \
	$(man8dir)/makesmtpaccess-msa.8 \
	$(man8dir)/esmtpd-msa.8

man1dir=@mandir@/man1
man5dir=@mandir@/man5
man7dir=@mandir@/man7
man8dir=@mandir@/man8

man_MANS=$(man1) $(man5) $(man7) $(man8)

clean-local:
	rm -rf cleanlist
	mkdir cleanlist
	for f in $(EXTRA_DIST) ; do echo timestamp >cleanlist/$$f ; done
	for f in $(man_MANS) $(htmldoc_DATA) ; do test -f cleanlist/$$f || rm -f $$f ; done
	rm -rf cleanlist

install-data-hook:
	$(MAKE) install-man
	$(MAKE) install-man-extra
	$(MAKE) install-htmldocDATA
	$(MAKE) do-man-install

do-man-install:
	for f in $(mandir) $(man1dir) $(man7dir) $(man8dir) ; do test -d $(DESTDIR)$$f && chmod 755 $(DESTDIR)$$f ; done ; exit 0
	for f in $(man1) ; do test -f $(DESTDIR)$(man1dir)/$$f && chmod 644 $(DESTDIR)$(man1dir)/$$f ; done ; exit 0
	for f in $(man5) ; do test -f $(DESTDIR)$(man5dir)/$$f && chmod 644 $(DESTDIR)$(man5dir)/$$f ; done ; exit 0
	for f in $(man7) ; do test -f $(DESTDIR)$(man7dir)/$$f && chmod 644 $(DESTDIR)$(man7dir)/$$f ; done ; exit 0
	for f in $(man8) ; do test -f $(DESTDIR)$(man8dir)/$$f && chmod 644 $(DESTDIR)$(man8dir)/$$f ; done ; exit 0
	test `id -u` = 0 || exit 0 ; for f in $(mandir) $(man1dir) $(man5dir) $(man7dir) $(man8dir) ; do test -d $(DESTDIR)$$f && chown bin $(DESTDIR)$$f && chgrp bin $(DESTDIR)$$f ; done ; exit 0
	test `id -u` = 0 || exit 0 ; for f in $(man1) ; do test -f $(DESTDIR)$(man1dir)/$$f && chown bin $(DESTDIR)$(man1dir)/$$f && chgrp bin $(DESTDIR)$(man1dir)/$$f ; done ; exit 0
	test `id -u` = 0 || exit 0 ; for f in $(man5) ; do test -f $(DESTDIR)$(man5dir)/$$f && chown bin $(DESTDIR)$(man5dir)/$$f && chgrp bin $(DESTDIR)$(man5dir)/$$f ; done ; exit 0
	test `id -u` = 0 || exit 0 ; for f in $(man7) ; do test -f $(DESTDIR)$(man7dir)/$$f && chown bin $(DESTDIR)$(man7dir)/$$f && chgrp bin $(DESTDIR)$(man7dir)/$$f ; done ; exit 0
	test `id -u` = 0 || exit 0 ; for f in $(man8) ; do test -f $(DESTDIR)$(man8dir)/$$f && chown bin $(DESTDIR)$(man8dir)/$$f && chgrp bin $(DESTDIR)$(man8dir)/$$f ; done ; exit 0
	for f in $(htmldoc_DATA) ; do chmod 444 $(DESTDIR)$(htmldocdir)/$$f ; done
	test `id -u` = 0 || exit 0; for f in $(htmldoc_DATA) ; do chown bin $(DESTDIR)$(htmldocdir)/$$f ; chgrp bin $(DESTDIR)$(htmldocdir)/$$f ; done

status.html: config.status

install-perms:
	@$(MAKE) install-perms-recursive
	@$(MAKE) install-perms-local

install-perms-recursive:
	@>permissions.dat
	@SUBDIRS="$(SUBDIRS)" ; for f in $$SUBDIRS ; do ( cd $$f && $(MAKE) $(AM_MAKEFLAGS) install-perms ) ; cat /dev/null >>$$f/permissions.dat ; cat $$f/permissions.dat >>permissions.dat ; done

install-perms-local:
	@echo @mandir@ 755 bin bin >>permissions.dat
	@echo $(man1dir) 755 bin bin >>permissions.dat
	@echo $(man5dir) 755 bin bin >>permissions.dat
	@echo $(man7dir) 755 bin bin >>permissions.dat
	@echo $(man8dir) 755 bin bin >>permissions.dat
	@for f in $(man1) ; do	\
	echo $(man1dir)/$$f 644 bin bin man >>permissions.dat ; done
	@for f in $(man5) ; do	\
	echo $(man5dir)/$$f 644 bin bin man >>permissions.dat ; done
	@for f in $(man7) ; do	\
	echo $(man7dir)/$$f 644 bin bin man >>permissions.dat ; done
	@for f in $(man8) ; do	\
	echo $(man8dir)/$$f 644 bin bin man >>permissions.dat ; done
	@for f in $(man_LINKS) ; do	\
	echo $$f 644 bin bin man >>permissions.dat ; done
	@echo $(htmldocdir) 755 bin bin >>permissions.dat
	@for f in $(htmldoc_DATA) ; do \
	echo $(htmldocdir)/$$f 444 bin bin doc >>permissions.dat ; done

htmldocfiles=$(nohtmlinfiles) $(htmlin) \
	../module.fax/courierfax.html \
	../filters/courierfilter.html \
	../filters/courierperlfilter.html \
	../module.uucp/courieruucp.html \
	../filters/dupfilter.html \
	../filters/ratefilter.html \
	../filters/verifyfilter.html

dollar=$$
id=$(dollar)Id

WWW=$$HOME/www/www.courier-mta.org

htmldoc: $(htmldocfiles)
	for f in $(htmldocfiles); do test "$$f" = "menu.html" && continue; b=`basename $$f`; d=$(WWW)/$$b; h=`test ! -f $$d || perl -ne ' print $$1 if /\\$(id):([^\\$$]*)\\$$/' <$$d`; echo $$f; xsltproc --nonet courier.xsl $$f | perl -ne 's~\$(id):.*\$$~\$(id):'"$$h"'\$$~; print' >$$d; done
	for f in menu.html; do  b=`basename $$f`; d=$(WWW)/$$b; h=`test ! -f $$d || perl -ne ' print $$1 if /\\$(id):([^\\$$]*)\\$$/' <$$d`; perl -ne 's~\$(id):.*\$$~\$(id):'"$$h"'\$$~; print' <$$f >$$d; done
	cp $(miscfiles) $(WWW)
