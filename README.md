# Courier Mail Server

The Courier mail transfer agent (MTA) is an integrated mail/groupware
server based on open commodity protocols, such as ESMTP, IMAP, POP3,
LDAP, SSL, and HTTP. Courier provides ESMTP, IMAP, POP3, webmail, and
mailing list services within a single, consistent, framework. Individual
components can be enabled or disabled at will. The Courier mail server
now implements basic web-based calendaring and scheduling services
integrated in the webmail module.

## How to build

This is a shared repository by all Courier-related projects. Each project
is in a separate subdirectory. Some additional setup is needed before
hacking on the source code. All projects also pull a shared repository
that contains modules that are shared by multiple Courier projects. If
you want to hack the "courier" project, you must pull the shared repository
into the courier/libs directory. The sysconftool project must be built
AND installed before building any other project.

To set up a project directory, run this script with two parameters:

- A) the project directory to set up.
- B) The URL to the shared library module git repository.

For example:

    sh INSTALLME courier http://www.example.com/courier-libs.git

The URL for the shared library module is purposefully not given in this
INSTALLME in case it changes in the future. If you're reading this INSTALLME,
you should already know what it is.

Note: for historical reasons, the courier-unicode project is in the libs
repository, the unicode subdirectory. It does not require sysconftool, and
can be built by itself/

_Copyright 1998-2022 Double Precision, Inc. This software is distributed
under the terms of the GNU General Public License. See COPYING for
additional information._

## Additional dependencies when building from this repository

This repository does not contain any automatically or script-generated
source files. As such, building from this repository requires additional
dependencies that are not needed when building packaged tarballs.

Additional packages that you may need to install are (they need to be
installed prior to running INSTALLME):

1) autoconf, automake, libtool

INSTALLME automatically runs the "autobloat" script to autogenerate all
autotool-generated scripts.

2) gettext

The repository does not contain gettext-generated ekeleton code, either.
The "autobloat" script will also run gettextize to add gettext scaffolding,
where needed.

3) xsltproc, docbook, and docbook DTDs.

- Fedora: libxslt, docbook-dtds, docbook-style-dsssl, docbook-utils,
docbook-style-xsl, docbook5-schemas, docbook5-style-xsl

- Ubuntu: xsltproc, docbook, docbook-xml, docbook-xsl, w3c-sgml-lib

4) elinks, tidy

5) libtool, libltdl

6) courier-authlib dependencies: openldap, mysql, postgresql, sqlite, gdbm/bdb.

- Fedora: openldap-devel, mariadb-devel, zlib-devel, sqlite-devel,
  postresql-devel, gdbm-devel, pam-devel, expect, libtool-ltdl-devel

- Ubuntu: libldap2-dev, libmariadb-dev, libz-dev, libsqlite3-dev,
  libpq-dev, libgdbm-dev, libpam0g-dev, expect, libltdl-dev

## How to debug

If you want to debug login troubles a nice trick is to save the "imaplogin" binary to "imaplogin.save" and replace it with a shell script: (debian path examples) 

```sh
#!/bin/sh
env >/tmp/imapd.environ
DATE=$(date)
echo "new imapdlogin $DATE from $TLS_SUBJECT_CN" >> /tmp/imap_chat_in.txt
echo "new imapdlogin $DATE from $TLS_SUBJECT_CN" >> /tmp/imap_chat_out.txt
exec tee -a /tmp/imap_chat_in.txt | usr/lib/courier/courier/imaplogin.debug "$@" 2>>/tmp/imaplogin_errout.txt | tee -a /tmp/imap_chat_out.txt
```
Make sure your imaplogin process is executable and has write access to these files.
