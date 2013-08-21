#!/usr/bin/perl
#
# Copyright 2003, Double Precision Inc.
#
# See COPYING for distribution information.

my $colnum=0;

while (<>)
{
    if (/^\+([^~]+)~(.*)/)
    {
	$currentGroup=$1;
	$groupName{$1}=$2;
	push @groups, $1;
	$colnum=0;
	next;
    }

    next unless /^([^~]+)~(.*)/;

    print "struct CustomColor color_$1={N_(\"$2 \"), \"$1\", "
	. ++$colnum . "};\n";

    push @{$groupList{$currentGroup}}, $1;
}

foreach (keys %groupList)
{
    print "\nstatic struct CustomColor *${_}[]={\n";
    foreach (@{$groupList{$_}})
    {
	print "\t&color_$_,\n";
    }
    print "\tNULL};\n";
}

print "\nstatic struct CustomColorGroup allGroups[]={\n";

foreach (@groups)
{
    print "\t{N_(\"$groupName{$_} \"), $_},\n";
}
print "\t{\"\", NULL}};\n";
