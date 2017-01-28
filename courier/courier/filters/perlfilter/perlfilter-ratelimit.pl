#!/usr/bin/perl
#
# Copyright 2014 Double Precision, Inc.  See COPYING for
# distribution information.
#
# To use this filter, at least perl 5.10 is required:
#
# * Must set $sysconfdir/filters/perlfilter-numprocs to 1:
#
#  echo 1 > $sysconfdir/filters/perlfilter-numprocs
#
# $sysconfdir is the Courier configuration directory, usually
# /usr/lib/courier/etc or /etc/courier.
#
# * Set perlfilter-mode to "all" (typical, see courierfilter(8) man page).
#
#  echo all > $sysconfdir/filters/perlfilter-mode
#
# * Set up the $sysconfdir/filters/perlfilter-ratelimit configuration file.
# The contents of this file are "name=value" settings:
#
# WINDOW=<n>
# MESSAGES=<n>
# LIMIT=<n>
# COUNT=(r|m)
# EXTERNAL=(0 or 1)
#
# The default configuration settings are:
#
# WINDOW=3600
# MESSAGES=1000
# LIMIT=100
# COUNT=r
# EXCLUDE=127.0.0.1
# EXTERNAL=0
#
# Default values do not need to be explicitly given in the config file, only
# any non-default valus.
#
# This means: use a one hour window (3600 seconds) of up to 1000 messages.
# We save messages that were processed in the last hour, but only up to
# the last 1000 messages in that window, so that a busy server that receives
# lots of mail is not going to have to waste precious RAM saving information
# about all mail received in the last hour.
#
# Anyway, the rate limiting module will accept up to 100 receipients ("r",
# the alternative setting is "m", number of messages) from the same sender.
#
# What constitutes a sender is either the sender's authenticated ID, or the
# sender's IP address.
#
# EXCLUDE is a comma separated list of authenticated IDs and/or IP addresses
# that are excluded from being rate limited. Put in here a list of your
# backup MXs that send you lots of mail. This is only needed if your backup
# MX authenticate, or their IP addresses have relayclient privileges via
# RELAYCLIENT. If a backup MX is just an ordinary sender that dumps mail on
# you for your domains, no different than any other mail source, this is
# not needed. The rate limiting applies only to clients with relaying
# privileges (by IP address or via authentication). This is the default with
# EXTERNAL=0. Setting EXTERNAL to 1 applies rate limiting to all senders, in
# which case any backup MXs not subject to rate limiting should be
# EXCLUDED
#
# * Set the pathname to this script
#
# echo $pathname >$sysconfdir/filters/perlfilter
#
# * Start it
#
# filterctl start perlfilter

use IO::File;
use feature 'state';
use Data::Dumper;

my $sysconfdir = $ENV{sysconfdir};

# Read configuration file

my $init_config = sub {

    my $config =
	IO::File->new("$sysconfdir/filters/perlfilter-ratelimit", "r");

    # Default settings
    my %default_config = (
	"WINDOW" => 3600,
	"MESSAGES" => 1000,
	"LIMIT" => 100,
	"COUNT" => "r",
	"EXCLUDE" => "127.0.0.1",
	"EXTERNAL" => 0,
	);

    if ($config)
    {
	foreach my $line (<$config>)
	{
	    chomp $line;
	    $line =~ s/[\s\"]//g;
	    $default_config{$1} = $2 if $line =~ /^([^=]*)=(.*)/;
	}
	close($config);
    }

    # EXCLUDE is a comma-separated list. Turn it into a lookup hash.

    $default_config{EXCLUDE} = {
	map {
	    ($_ => 1);
	} split(/,+/, $default_config{EXCLUDE})
    };

    return \%default_config;
    return {};
};

# Private static variables

state $counts = {};
state $messages = [];
state $config = $init_config->();

# The number of the filedescriptor that's connected to the socket is
# passed to us on STDIN.

my $filedesc=shift @ARGV;

my $socket=IO::File->new("+<&$filedesc");

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
    my $filename = shift;

    return "";
}

sub filtercontrol
{
    my $filename=shift;

    my $authname="";
    my %envs;
    my $count=0;

    my $fh=IO::File->new($filename, "r");

    if ($fh)
    {
	foreach my $line (<$fh>)
	{
	    chomp $line;

	    # See comctlfile.h

	    $authname=$1 if $line =~ /^i(.*)/;
	    $envs{$1}=$2 if $line =~ /^O([^=]*)=(.*)/;
	    ++$count if $line =~ /^r/;
	}
	close($fh);
    }

    # Does rate limiting apply?

    $envs{RELAYCLIENT}=1 if $config->{EXTERNAL};
    return unless exists $envs{RELAYCLIENT};

    # Yes it does, but on a per message basis
    $count=1 if $config->{COUNT} eq "m";

    # Ok, who do we charge this again?

    my $who_is_this;

    if ($authname)
    {
	$who_is_this=$authname;
    }
    elsif ($envs{TCPREMOTEIP})
    {
	$who_is_this=$envs{TCPREMOTEIP};

	$who_is_this=~s/^::ffff://;  # Convert IPv6 to IPv4 format
    }
    else
    {
	# Shouldn't happen

	return "";
    }

    # Give these guys a pass
    return "" if exists $config->{EXCLUDE}{$who_is_this};

    # First, purge stale cache data

    my $now=time;

    my $c;

    while ( ($c=scalar @$messages) >= $config->{MESSAGES} ||
	($c && $messages->[0]->{timestamp} < $now - $config->{WINDOW}))
    {
	my $oldest=shift @$messages;

	delete $counts->{$oldest->{who}}
	    if ($counts->{$oldest->{who}} -= $oldest->{count}) == 0;
    }

    # Now, drop this one into the cache
    my $new_count = (($counts->{$who_is_this} //= 0) += $count);

    push @$messages, {
	who => $who_is_this,
	count => $count,
	timestamp => $now,
    };

    # To dump the contents of the filter's state, create an empty
    # perlfilter-dump file. Note, it must be writable by the filtering
    # process, which runs under a non-root, but a system userid (typically
    # "mail" or "daemon"). The next time this filter wakes up, it will write
    # its current state to the perlfilter-dump file.

    if (-z "$sysconfdir/filters/perlfilter-dump")
    {
	my $f = IO::File->new("$sysconfdir/filters/perlfilter-dump", "w");

	if ($f)
	{
	    print $f Data::Dumper::Dumper( {
		counts => $counts,
		buffer => $messages,
		});
	    close($f);
	}
    }

    # Decide this message's fate

    return $new_count > $config->{LIMIT}
    ? "552 $who_is_this is sending too much mail, slow down.":"";
}
