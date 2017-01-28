#!/usr/bin/perl
#
# This is basically persistent.pl from perlembed, with a few minor
# changes.
#
# This is not what you're looking for.  You want perlfilter-example.pl
# Also, you might as well read the perlfilter(8) man page.
#
package Embed::Persistent;
 
use strict;
use vars '%Cache';
 
sub valid_package_name {
    my($string) = @_;
    $string =~ s/([^A-Za-z0-9\/])/sprintf("_%2x",unpack("C",$1))/eg;
    # second pass only for words starting with a digit
    $string =~ s|/(\d)|sprintf("/_%2x",unpack("C",$1))|eg;
    # Dress it up as a real package name
    $string =~ s|/|::|g;
    return "Embed" . $string;
}
 
#borrowed from Safe.pm
sub delete_package {
    my $pkg = shift;
    my ($stem, $leaf);
 
    no strict 'refs';
    $pkg = "main::$pkg\::";    # expand to full symbol table name
    ($stem, $leaf) = $pkg =~ m/(.*::)(\w+::)$/;
 
    my $stem_symtab = *{$stem}{HASH};
 
    delete $stem_symtab->{$leaf};
}
sub eval_file {
    my $filename=shift;
    my $delete=shift;
    my @args=@_;
    my $package = valid_package_name($filename);
    my $mtime = -M $filename;
    if(defined $Cache{$package}{mtime}
       &&
       $Cache{$package}{mtime} <= $mtime)
    {
       # we have compiled this subroutine already,
       # it has not been updated on disk, nothing left to do
       # print STDERR "already compiled $package->handler\n";
    }
    else {
       local *FH;
       open FH, $filename or die "open '$filename' $!";
       local($/) = undef;
       my $sub = <FH>;
       close FH;
 
       #wrap the code into a subroutine inside our unique package
       my $eval = qq{package $package;
			sub handler { \@ARGV=\@_; shift \@ARGV; $sub; }};
       {
           # hide our variables within this block
           my($filename,$mtime,$package,$sub);
           my(@args);
           eval $eval;
       }
       die $@ if $@;
 
       #cache it unless we're cleaning out each time
       $Cache{$package}{mtime} = $mtime unless $delete;
    }
    eval {$package->handler(@args);};
    die $@ if $@;
 
    delete_package($package) if $delete;
 
    #take a look if you want
    #print Devel::Symdump->rnew($package)->as_string, $/;
}
 
1;
