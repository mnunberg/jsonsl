#!/usr/bin/perl
# This script generates the character table for 'special' lookups
#
use strict;
use warnings;
use Getopt::Long;

my @tokdefs;
$tokdefs[ord('-')] = 'JSONSL_SPECIALf_SIGNED';
$tokdefs[ord('t')] = 'JSONSL_SPECIALf_TRUE';
$tokdefs[ord('f')] = 'JSONSL_SPECIALf_FALSE';
$tokdefs[ord('n')] = 'JSONSL_SPECIALf_NULL';
$tokdefs[ord($_)]  = 'JSONSL_SPECIALf_UNSIGNED' for (0..9);

my @strdefs;
$strdefs[ord('\\')] = 1;
$strdefs[ord('"')] = 1;

#Tokens which terminate a 'special' sequence. Basically all JSON tokens
#themselves
my @special_end;
{
    my @toks = qw([ { } ] " : \\ );
    push @toks, ',';
    $special_end[ord($_)] = 1 for (@toks);
}

#RFC 4627 allowed whitespace
my @wstable;
foreach my $x (0x20, 0x09, 0xa, 0xd) {
    $wstable[$x] = 1;
    $special_end[$x] = 1;
}

my @lines;
my $cur = { begin => 0, items => [], end => 0 };
push @lines, $cur;

my $Table;

GetOptions(
    'special' => \my $SpecialTable,
    'strings' => \my $TokstrTable,
    'special_end' => \my $SpecialEnd,
    'whitespace' => \my $Whitespace,
);

unless ($SpecialTable || $TokstrTable || $SpecialEnd || $Whitespace) {
    die ("Please specify --special, --special_end or --strings to select table");
}

if ($SpecialTable) {
    $Table = \@tokdefs;
} elsif ($SpecialEnd) {
   $Table = \@special_end;
} elsif ($Whitespace) {
    $Table = \@wstable;
} else {
    $Table = \@strdefs;
}

my $i = 0;
my $cur_col = 0;

my $special_last = 0;

sub add_to_grid {
    my $v = shift;

    if ($special_last) {
        $cur = { begin => $i, end => $i, items => [ $v ]};
        push @lines, $cur;
        $special_last = 0;
        $cur_col = 1;
        return;
    } else {
        push @{$cur->{items}}, $v;
        $cur->{end} = $i;
        $cur_col++;
    }

    if ($cur_col >= 32) {
        $cur = {
            begin => $i+1, end => $i+1, items => [] };
        $cur_col = 0;
        push @lines, $cur;
    }
}

sub add_special {
    my $v = shift;
    push @lines, { items => [ $v ], begin => $i, end => $i };
    $special_last = 1;
}

$special_last = 0;
for (; $i < 255; $i++) {
    my $v = $Table->[$i];
    if (defined $v) {
        my $char_pretty;
        if ($i == 0xa) {
            $char_pretty = '<LF>';
        } elsif ($i == 0xd) {
            $char_pretty = '<CR>';
        } elsif ($i == 0x9) {
           $char_pretty = '<TAB>';
        } elsif ($i == 0x20) {
           $char_pretty = '<SP>';
        } else {
            $char_pretty = chr($i);
        }
        $v = sprintf("$v /* %s */", $char_pretty);
        add_special($v);
    } else {
        add_to_grid(0);
    }
}

foreach my $line (@lines) {
    my $items = $line->{items};
    if (@$items) {
        printf("/* 0x%02x */ %s, /* 0x%02x */\n",
            $line->{begin}, join(",", @$items), $line->{end});
    }
}
