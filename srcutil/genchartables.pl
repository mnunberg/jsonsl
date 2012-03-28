#!/usr/bin/perl
# This script generates the character table for 'special' lookups
#
use strict;
use warnings;

my @defs;
$defs[ord('-')] = 'JSONSL_SPECIALf_SIGNED';
$defs[ord('t')] = 'JSONSL_SPECIALf_TRUE';
$defs[ord('f')] = 'JSONSL_SPECIALf_FALSE';
$defs[ord('n')] = 'JSONSL_SPECIALf_NULL';
$defs[ord($_)]  = 'JSONSL_SPECIALf_UNSIGNED' for (0..9);

my @lines;
my $cur = { begin => 0, items => [], end => 0 };
push @lines, $cur;

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

my $special_last = 0;
for (; $i < 255; $i++) {
    my $v = $defs[$i];
    if (defined $v) {
        $v = sprintf("$v /* %s */", chr($i));
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
