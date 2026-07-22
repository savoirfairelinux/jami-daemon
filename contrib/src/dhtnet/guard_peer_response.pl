#!/usr/bin/env perl

use strict;
use warnings;

my $guard = "    if (!response.from)\n        return;\n";

sub add_guard {
    my ($source) = @_;
    return ($source, 0) if index($source, '!response.from') >= 0;

    my $signature = 'ConnectionManager::Impl::onPeerResponse';
    my $signature_index = index($source, $signature);
    die "$signature definition not found\n" if $signature_index < 0;

    my $body_index = index($source, '{', $signature_index);
    die "$signature body not found\n" if $body_index < 0;

    my $body_prefix = substr($source, $signature_index, $body_index - $signature_index);
    die "$signature does not use the expected response parameter\n"
        unless $body_prefix =~ /\bresponse\b/;

    my $newline = index($source, "\r\n") >= 0 ? "\r\n" : "\n";
    my $insert = $newline . $guard;
    $insert =~ s/\n/$newline/g if $newline ne "\n";

    substr($source, $body_index + 1, 0) = $insert;
    return ($source, 1);
}

sub self_test {
    my $sample = <<'EOF';
void
ConnectionManager::Impl::onPeerResponse(PeerConnectionRequest&& response)
{
    auto deviceId = response.from->getId();
}
EOF

    my ($patched, $changed) = add_guard($sample);
    die "guard was not inserted\n" unless $changed;
    die "guard missing\n" unless index($patched, "if (!response.from)\n        return;\n") >= 0;
    die "guard inserted after dereference\n"
        unless index($patched, 'if (!response.from)') < index($patched, 'response.from->getId()');

    my ($second, $changed_again) = add_guard($patched);
    die "guard insertion is not idempotent\n" if $changed_again;
    die "idempotent patch changed source\n" unless $second eq $patched;
}

sub patch_file {
    my ($path) = @_;
    open(my $in, '<:encoding(UTF-8)', $path) or die "cannot read $path: $!\n";
    local $/;
    my $source = <$in>;
    close($in);

    my ($patched, $changed) = add_guard($source);
    if ($changed) {
        open(my $out, '>:encoding(UTF-8)', $path) or die "cannot write $path: $!\n";
        print {$out} $patched;
        close($out);
    }
}

if (@ARGV == 1 && $ARGV[0] eq '--self-test') {
    self_test();
    exit 0;
}

die "usage: $0 <connectionmanager.cpp> | --self-test\n" unless @ARGV == 1;
patch_file($ARGV[0]);
