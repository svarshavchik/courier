#! perl
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.

my $fh=new FileHandle;

my $pid;

die $cgi->header("text/plain") . "Cannot start process: $!\n"
    unless (defined $fh) && (defined ($pid=open($fh, "-|")));

if (! $pid)
{
    my $errflag=0;

    open(STDERR, ">&STDOUT");

    foreach (ReadDirList("added"))
    {

	my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	    $atime,$mtime,$ctime,$blksize,$blocks)
	    = stat("$sysconfdir/webadmin/added/$_");

	next unless $mode;

	print "Creating $_...\n";
	mkdir("$sysconfdir/$_", $mode);
    }
    unlink("$sysconfdir/webadmin/subdirs-added");

    foreach (ReadConfigFiles())
    {
	if ( -f "$sysconfdir/webadmin/added/$_" )
	{
	    print "Installing $_...\n";

	    my $newfilename="$sysconfdir/$_";

	    $newfilename=$authdaemonrc if $newfilename eq "$sysconfdir/authdaemonrc";
	    $newfilename=$authldaprc if $newfilename eq "$sysconfdir/authldaprc";
	    $newfilename=$authmysqlrc if $newfilename eq "$sysconfdir/authmysqlrc";
	    $newfilename=$authpgsqlrc if $newfilename eq "$sysconfdir/authpgsqlrc";

	    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
		$atime,$mtime,$ctime,$blksize,$blocks)
		= stat($newfilename);

	    if ($mode)
	    {
		chmod ($mode, "$sysconfdir/webadmin/added/$_");
		chown ($uid, $gid, "$sysconfdir/webadmin/added/$_");
	    }
	    ($errflag=1, print "ERROR: $!\n")
		if ! rename ("$sysconfdir/webadmin/added/$_", $newfilename);
	}

	if ( -f "$sysconfdir/webadmin/removed/$_" )
	{
	    print "Deleting $_...\n";
	    ($errflag=1, print "ERROR: $!\n")
		if ! unlink ("$sysconfdir/webadmin/removed/$_",
			     "$sysconfdir/$_");
	}
    }

    foreach (ReadDirList("removed"))
    {
	print "Removing $_...\n";
	rmdir("$sysconfdir/$_");
    }
    unlink("$sysconfdir/webadmin/subdirs-removed");

    $fh=new FileHandle "$sysconfdir/webadmin/changed";

    if (defined $fh)
    {
	my $line;

	while (defined($line=<$fh>))
	{
	    next if $errflag;

	    chomp $line;
	    next unless $line =~ /(.+)/;

	    $line=$1;

	    print "Executing $line...\n";
	    $errflag=1 if system($line) != 0;
	}
	close($fh);
    }

    my @leftovers=ReadConfigFiles();

    unlink ("$sysconfdir/webadmin/changed")
	if $#leftovers < 0 && $errflag == 0;

    exit 0;
}

my $output=htmlescape(join("", <$fh>));
close($fh);

display_form("admin-save.html",
	     {
		 "OUTPUT" => $output
		 }
	     );
exit 0
