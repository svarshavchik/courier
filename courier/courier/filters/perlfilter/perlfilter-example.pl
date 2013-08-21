#
# Copyright 2000 Double Precision, Inc.  See COPYING for
# distribution information.
#
# This is an example Perl filter.  Install this filter by initializing
# filters/perlfilter control file to contain the pathname to this file.
# See courierfilter(8) for more information.
#
# This example Perl filter blocks messages with a long Date: header.
#
# *** DO NOT MODIFY THIS FILE VERBATIM ***  When upgrading Courier this
# file will be overwritten.  Make a copy of this file, and point
# filters/perlfilter to your modified copy.

use IO::File;

# The number of the filedescriptor that's connected to the socket is
# passed to us on STDIN.

my $filedesc=shift @ARGV;

my $socket=new IO::File "+<&$filedesc";

die "$!" unless defined $socket;

my $line;
my $first=1;
my $errmsg="200 Ok";

#
# Read lines from the socket.  Each line contains a filename.  An empty line
# terminates the list.  The first line is the filename of the datafile
# containing the message text.  The subsequent lines are filename(s) of
# control files.
#

while (defined ($line=<$socket>))
{
my $msg;

	chomp $line;
	last unless $line;

	if ($first)
	{
		$msg=filterdata($line);
	}
	else
	{
		$msg=filtercontrol($line);
	}
	$first=0;
	$errmsg=$msg if $msg;
}

$errmsg .= "\n" unless $errmsg =~ /\n$/;
print $socket $errmsg;

$socket->close;

sub filterdata
{
my $filename=shift;

#  Here's where the custom content filter is implemented.  Use filehandles
#  so that cleanup's automatic.

my $fh=new IO::File "< $filename";

	return "" unless defined $fh;

my $line;

	while ( defined ($line=<$fh>))
	{
		chomp $line;
		last if $line eq "";	# End of headers

		return "500 Invalid Date header."
			if $line =~ /^Date:......................................................................../i;
	}

	return "";
}

sub filtercontrol
{
my $filename=shift;

return "";
}
