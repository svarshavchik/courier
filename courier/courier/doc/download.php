<?php
echo "<";
echo '?xml version="1.0"?';
echo ">";
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml">
<head>
  <title>Courier Mail Server - latest releases</title><!-- $Id$ -->
  <!-- Copyright 2000-2014 Double Precision, Inc.  See COPYING for -->
  <!-- distribution information. -->
  <link rel="icon" href="icon.gif" type="image/gif" />
  <style type="text/css">
/*<![CDATA[*/

  h2 a { color: #000000 }
  h2 a:visited { color: #000000 }

         .downloadbox { padding: .5em; white-space: nowrap;
                        border-style: inset; border-width: 1px;
                        border-color: #000000;
                        background-color: #eeeeee;
                 }
         .downloadbox tr, .downloadbox td { padding-left: .5em;
                                            vertical-align: top; }
         .downloadbox td { font-family: "Liberation Mono", "Courier New", courier, monospace; }
         .siglink { font-size: 80% }

  div.banner { position: fixed;
       top: 2em; right: 5px; bottom: auto; left: auto }

  div.main { top: 1em;
     left: 0em;
     padding-right: 170px;
     z-index: 1;
     bottom: auto; }
  /*]]>*/
  </style><!-- Workaround: MSIE does not grok position: fixed -->
  <!--[if gte IE 5]>
<style type="text/css">

body { height: 100%; overflow-y: hidden; margin: 0px; padding: 0px; }
body div.banner { position: absolute; right: 20px }
body div.main { height: 100%; width: 100%; overflow: auto;
        margin: 0px; padding-right: 170px; }
</style>
<![endif]-->
</head>

<body>
  <?php
                     /*<![CDATA[*/

                     $dir=".";

                     $filename="$dir/download.lst";

                     $old_err=error_reporting(60);
                             $fp=fopen($filename, "r");
                             if (! $fp)
                             {
                                     $filename="/var/www/download.lst";
                                     $fp=fopen($filename, "r");
                             }
                             error_reporting($old_err);
                             if (! $fp)
                             {
                                     echo "Unable to read configuration file.\n";
                                     exit();
                             }
                             while (!feof($fp))
                             {
                                     $line=fgets($fp, 4096);
                                     if (strlen($line) == 0)
                                     {
                                             continue;
                                     }

				     $foo = preg_split( "[ \t\n]", $line);

                                     list($package, $version_s, $date_s, $filename_s, $size_s)
                                     =preg_split( "/[ \t\n]/", $line);

                                     $Version[$package]=$version_s;
                                     $Date[$package]=$date_s;
                                     $Filename[$package]=$filename_s;
                                     $Size[$package]=$size_s;
                             }
                             fclose($fp);

                             function download_link($package, $version,
                                                    $date, $filename, $size)
                             {
                                     $httpurl="https://sourceforge.net/projects/courier/files/";

                                     print "<tr><th>Date</th><th>Version</th><th>Filename</th><th>Size</th></tr>";
                                     print "<tr><td>$date</td><td>$version</td>";
                                     print "<td><a href='$httpurl$package/$version/$filename/download'>$filename</a><br />";
                                     print "<span class='siglink'>(<a href='$httpurl$package/$version/$filename.sig/download'>PGP signature</a>)</span>";

                                     print "</td><td>$size</td></tr>";

                             }

                             function download_version($package, $changelog)
                             {
                                     global $Version, $Date, $Filename, $Size;

                                     $has_devel=0;

                                     $packagedevel = $package . "-devel";

                                     if (array_key_exists($packagedevel, $Version))
                                     {
                                             $has_devel=1;
                                     }

                                     print "<table class='downloadbox'>";
                                     print "<tbody>";
                                     print "<tr><th colspan='4'>Current release</th></tr>";

                                     download_link($package,
                                                   $Version[$package],
                                                   $Date[$package],
                                                   $Filename[$package],
                                                   $Size[$package]);

                                     if ($has_devel)
                                     {
                                             print "<tr><td colspan='4'><hr/></td></tr>";
                                             print "<tr><th colspan='4'>Latest development release (<a href='$changelog'>changelog</a>)</th></tr>";
                                             download_link($packagedevel,
                                                           $Version[$packagedevel],
                                                           $Date[$packagedevel],
                                                           $Filename[$packagedevel],
                                                           $Size[$packagedevel]);
                                     }

                                     print "</tbody></table>";

                             }
                             /*]]>*/
                             ?>

  <div class='main'>
    <h1>Courier Mail Server - latest releases</h1>
    <hr />

    <p><a href="/KEYS.bin">PGP distribution signing key</a></p>

    <p><a href=
    "https://github.com/svarshavchik/courier-contrib">Public Github
    repository</a> with miscellaneous scripts and tools.</p>

    <h2><a name="courier" id="courier">Package:
    Courier</a></h2><?php
                                         download_version("courier",
                                                          "http://sourceforge.net/p/courier/courier.git/ci/master/tree/courier/ChangeLog");
                                         ?>

    <p>This package includes the entire source code for the
    <a href="http://www.courier-mta.org">Courier mail server</a> -
    the mail server, IMAP server, webmail server, and the maildrop
    mail filter. You do not need to install those individual
    packages if you download this package.</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
    <hr />

    <h2><a name="authlib" id="authlib">Package: Courier
    authentication library</a></h2><?php

                                         download_version("authlib",
                                                          "http://sourceforge.net/p/courier/courier.git/ci/master/tree/courier-authlib/ChangeLog");

                                         ?>

    <p>The Courier Authentication Library is a generic
    authentication API that encapsulates the process of validating
    account passwords. In addition to reading the traditional
    account passwords from <code>/etc/passwd</code>, the account
    information can alternatively be obtained from an LDAP
    directory; a MySQL or a PostgreSQL database; or a GDBM or a DB
    file. The Courier authentication library must be installed
    before building any Courier packages that needs direct access
    to mailboxes (in other words, all packages except for
    courier-sox, courier-analog, and courier-unicode).</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
    <hr />

    <h2><a name="unicode" id="unicode">Package: Courier Unicode
    Library</a></h2><?php
                                         download_version("courier-unicode",
                                                          "http://sourceforge.net/p/courier/courier-libs.git/ci/master/tree/unicode/ChangeLog");
                                         ?>

    <p>The <a href="http://www.courier-mta.org/unicode/">Courier
    Unicode Library</a> is used by most other Courier packages, and
    needs to be installed in order to use them or build them.</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
    <hr />

    <h2><a name="analog" id="analog">Package:
    Courier-analog</a></h2><?php
                                         download_version("analog",
                                                          "http://sourceforge.net/p/courier/courier.git/ci/master/tree/courier-analog/ChangeLog");
                                         ?>

    <p>This is an optional package, the Courier log analyzer.
    Courier-analog generates log summaries for incoming and
    outgoing SMTP connections, and IMAP and POP3 activity.
    courier-analog can generate output in text or HTML format.</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
    <hr />

    <h2><a name="imap" id="imap">Package:
    Courier-IMAP</a></h2><?php

                                         download_version("imap",
                                                          "http://sourceforge.net/p/courier/courier.git/ci/master/tree/courier-imap/ChangeLog");
                                         ?>

    <p>This package contains the standalone Courier IMAP server,
    which is used to provide IMAP access to local mailboxes.
    Courier-IMAP is provided here as a separate package that can be
    used with other mail servers as well.</p>

    <p><a href="http://www.courier-mta.org/imap/">More information
    about the Courier-IMAP server</a>.</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
    <hr />

    <h2><a name="sqwebmail" id="sqwebmail">Package:
    SqWebMail</a></h2><?php

                                         download_version("webmail",
                                                          "http://sourceforge.net/p/courier/courier-libs.git/ci/master/tree/sqwebmail/ChangeLog");
                                         ?>

    <p>This package contains the SqWebMail webmail CGI. This CGI is
    used by the Courier mail server to provide webmail access to
    local mailboxes. SqWebMail is provided here as a separate
    package that can be used with other mail servers as well.</p>

    <p><a href="http://www.courier-mta.org/sqwebmail/">More
    information about SqWebMail</a>.</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
    <hr />

    <h2><a name="maildrop" id="maildrop">Package:
    maildrop</a></h2><?php
                                         download_version("maildrop",
                                                          "http://sourceforge.net/p/courier/courier.git/ci/master/tree/maildrop/ChangeLog");
                                         ?>

    <p>This package contains the maildrop delivery agent/mail
    filter. This mail filter module is included in the Courier mail
    server, which uses it to filter incoming mail. Maildrop is
    provided here as a separate package that can be used with other
    mail servers as well.</p>

    <p><a href="http://www.courier-mta.org/maildrop/">More
    information about maildrop</a>.</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
    <hr />

    <h2><a name="sox" id="sox">Package: courier-sox</a></h2><?php
                                         download_version("sox",
                                                          "http://sourceforge.net/p/courier/courier.git/ci/master/tree/courier-sox/ChangeLog");
                                         ?>

    <p>This package contains the Courier Socks 5 Proxy client
    library, which allows Courier to send outgoing mail using a
    Socks 5 proxy. You will need to install this package before
    building Courier in order to use a Socks proxy to send outgoing
    mail.</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
    <hr />

    <h2><a name="cone" id="cone">Package: Cone</a></h2><?php
                                         download_version("cone",
                                                          "http://sourceforge.net/p/courier/courier.git/ci/master/tree/cone/ChangeLog");
                                         ?>

    <p>This package contains <span style=
    "font-family: serif; font-style: italic">Cone</span>, a
    text-based mail client based, in part, on Courier
    libraries.</p>

    <p><a href="http://www.courier-mta.org/cone/index.html">More
    information about <span style=
    "font-family: serif; font-style: italic">Cone</span></a>.</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
    <hr />

    <h2><a name="sysconftool" id="sysconftool">Package:
    sysconftool</a></h2><?php
                                         download_version("sysconftool",
                                                          "http://sourceforge.net/p/courier/courier.git/ci/master/tree/sysconftool/ChangeLog");
                                         ?>

    <p>The sysconftool utility contains an additional autoconf
    macro used by Courier to install configuration files. You only
    need to install sysconftool if you intend to check out Courier
    from Subversion. You do not need to install sysconftool in
    order to compile and install tarballed releases.</p>

    <p><a href="http://www.courier-mta.org/sysconftool/">More
    information about sysconftool</a>.</p>

    <p><font size="-1">(<a href=
    "http://sourceforge.net/project/showfiles.php?group_id=5404">Download
    older/other releases</a>)</font></p>
  </div>

  <div class='banner'>
    <script type="text/javascript">
<![CDATA[
    google_ad_client = "ca-pub-3196826191648604";
    /* download */
    google_ad_slot = "9611116675";
    google_ad_width = 160;
    google_ad_height = 600;
    ]]>
    </script> <script type="text/javascript" src=
    "http://pagead2.googlesyndication.com/pagead/show_ads.js">
</script>
  </div>
</body>
</html>
