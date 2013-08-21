#! perl
#
# TITLE: Inbound FAX
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $errstr="";

sub encode {
  my $mode=shift;
  my $hdr=shift;

  my $pid=open(REFORMIME, "-|");

  die "Content-Type: text/plain\n\n$!" unless defined $pid;

  if (!$pid)
    {
      $hdr =~ /(.*)/;
      $hdr= $1;
      my $chset="iso-8859-1";

      $chset=$1 if $ENV{"HTTP_ACCEPT_CHARSET"} =~ /^([^,\s]+)/;

      exec "$bindir/reformime", "-c", $chset, $mode, $hdr;
      exit 0;
    }

  $hdr=join("", <REFORMIME>);
  close(REFORMIME);
  chomp $hdr;
  return ($hdr);
}

if ($cgi->param("Save"))
  {
    ReplaceEnvVarConfigFile("faxnotifyrc", "DODELETE",
			    $cgi->param("DODELETE") ? "1":"0");

    ReplaceEnvVarConfigFile("faxnotifyrc", "MAILFROM",
			    $cgi->param("MAILFROM"));
    ReplaceEnvVarConfigFile("faxnotifyrc", "RCPTTO",
			    $cgi->param("RCPTTO"));
    ReplaceEnvVarConfigFile("faxnotifyrc", "FROMHDR",
			    encode("-O", $cgi->param("FROMHDR")));
    ReplaceEnvVarConfigFile("faxnotifyrc", "TOHDR",
			    encode("-O", $cgi->param("TOHDR")));
    ReplaceEnvVarConfigFile("faxnotifyrc", "SUBJECTHDR",
			    encode("-o", $cgi->param("SUBJECTHDR")));
    changed("");
    $errstr="\@SAVED\@";
  }

my $faxnotify=ReadEnvVarConfigFile("faxnotifyrc");

sub hdr {
  my $hdr=shift;

  my $pid=open(REFORMIME, "-|");

  die "Content-Type: text/plain\n\n$!" unless defined $pid;

  if (!$pid)
    {
      $hdr =~ /(.*)/;

      exec "$bindir/reformime", "-h", $1;
      exit 0;
    }

  $hdr=join("", <REFORMIME>);
  close(REFORMIME);
  chomp $hdr;
  return (htmlescape($hdr));
}

display_form("admin-34fax.html",
	     {
	      "ERROR" => $errstr,
	      "DODELETE" => ("<input type=\"checkbox\" name=\"DODELETE\""
			     . ($$faxnotify{"DODELETE"} ? " checked=\"checked\" />":" />")),
	      "MAILFROM" => $$faxnotify{"MAILFROM"},
	      "RCPTTO" => $$faxnotify{"RCPTTO"},
	      "FROMHDR" => hdr($$faxnotify{"FROMHDR"}),
	      "TOHDR" => hdr($$faxnotify{"TOHDR"}),
	      "SUBJECTHDR" => hdr($$faxnotify{"SUBJECTHDR"})

	     }
	    );
