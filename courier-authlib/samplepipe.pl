#!/usr/local/bin/perl -w

# This is a sample authentication module for authpipe. It uses the same
# protocol that pop3d/imapd/webmail use to communicate with authdaemon.
#
# If you want to indicate a temporary failure (e.g. database unavailable)
# then simply exit without sending any response. This will be indicated as
# a tempfail response, and a new child process will be started for the
# next authentication request, therefore ensuring that it is reinitialized.
#
# You can run this from the command line to test it. Example requests:
#
# PRE . imap fred@example.com	-- display data for this account
#
# AUTH 35			-- 35 is length of
# imap				-- "imap\nlogin\nfred@example.com\nfoobar\n"
# login
# fred@example.com
# foobar
#
# PASSWD imap<tab>fred@example.com<tab>foobar<tab>newpass
#
# ENUMERATE

my %authdata = (
  'fred@example.com' => ['foobar', 81, 81, '/home/fred', 'Maildir', 'disablepop3=0', '10000000S'],
  'wilma@example.com' => ['bazbaz', 81, 81, '/home/wilma'],
);

$|=1;

sub sendres($$)
{
  my $uid = shift;
  my $ref = shift;
  # see authdamond.c for full list of possible fields
  print "ADDRESS=$uid\n";
  print "PASSWD2=$ref->[0]\n";
  print "UID=$ref->[1]\n";
  print "GID=$ref->[2]\n";
  print "HOME=$ref->[3]\n";
  print "MAILDIR=$ref->[4]\n" if $ref->[4];
  print "OPTIONS=$ref->[5]\n" if $ref->[5];
  print "QUOTA=$ref->[6]\n" if $ref->[6];
  print ".\n";
}

print STDERR "Sample module starting\n";

while (<>)
{
  if (/^PRE (\S+) (\S+) (.*)$/)
  {
    # $1=. $2=service $3=uid
    if ($authdata{$3})
    {
      sendres($3, $authdata{$3});
      next;
    }
  }
  elsif (/^AUTH (\d+)$/ && read(STDIN,$buf,$1) == $1 &&
         $buf =~ /^(.*)\n(login)\n(.*)\n(.*)$/)
  {
    # $1=service, $2=authtype, $3=user, $4=password
    if ($authdata{$3} && $authdata{$3}->[0] eq $4)
    {
      sendres($3, $authdata{$3});
      next; 
    }
  }
  elsif (/^PASSWD (.*?)\t(.*?)\t(.*?)\t(.*?)$/)
  {
    # $1=service, $2=user, $3=oldpasswd, $4=newpasswd
    if ($authdata{$2} && $authdata{$2}->[0] eq $3)
    {
      # not a useful example unless you have set MAXDAEMONS=1
      $authdata{$2}->[0] = $4;
      print "OK\n";
      next;
    }
  }
  elsif (/^ENUMERATE$/) {
    foreach $k (keys %authdata) {
      $d = $authdata{$k};
      print "$k\t$d->[1]\t$d->[2]\t$d->[3]\t$d->[4]\t$d->[5]\n";
    }
    print ".\n";
    next;
  }
  print "FAIL\n";
}
