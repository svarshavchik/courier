# Courier Mail Server

The Courier mail transfer agent (MTA) is an integrated mail/groupware
server based on open commodity protocols, such as ESMTP, IMAP, POP3,
LDAP, SSL, and HTTP. Courier provides ESMTP, IMAP, POP3, webmail, and
mailing list services within a single, consistent, framework. Individual
components can be enabled or disabled at will. The Courier mail server
now implements basic web-based calendaring and scheduling services
integrated in the webmail module.

The Courier mail server's source code should compile on most POSIX-based
operating systems based on Linux, and BSD-derived kernels. It should also
compile on Solaris and AIX, with some help from Sun's or IBM's freeware
add-on tools for their respective operating systems.

## How to build

This is a shared repository by all Courier-related projects. Each project
is in a separate subdirectory. Some additional setup is needed before
hacking on the source code. All projects also pull a shared repository
that contains modules that are shared by multiple Courier projects. If
you want to hack the "courier" project, you must pull the shared repository
into the courier/libs directory. Additionally, clean and smudge filters
must be set up in order to properly support sysconftool-based configuration
files.

To set up a project directory, run this script with two parameters:

- A) the project directory to set up.
- B) The URL to the shared library module git repository.

For example:

    sh INSTALLME courier http://www.example.com/courier-libs.git

The URL for the shared library module is purposefully not given in this
INSTALLME in case it changes in the future. If you're reading this INSTALLME,
you should already know what it is.


_Copyright 1998-2013 Double Precision, Inc. This software is distributed
under the terms of the GNU General Public License. See COPYING for
additional information._

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
