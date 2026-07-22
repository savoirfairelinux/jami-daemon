#!/usr/bin/env perl
use strict;
use warnings;
use File::Find;

sub find_matching_brace {
    my ($source, $open_brace) = @_;
    my $depth = 0;
    for (my $index = $open_brace; $index < length($source); ++$index) {
        my $char = substr($source, $index, 1);
        if ($char eq "{") {
            ++$depth;
        } elsif ($char eq "}") {
            --$depth;
            return $index if $depth == 0;
        }
    }
    die "unterminated function body\n";
}

sub patch_shutdown {
    my ($source) = @_;
    return undef unless $source =~ /\bUPnPContext::shutdown\s*\([^)]*\)\s*(?:noexcept\s*)?\{/g;

    my $body_open = index($source, "{", $-[0]);
    my $body_close = find_matching_brace($source, $body_open);
    my $body = substr($source, $body_open + 1, $body_close - $body_open - 1);

    return $source if $body =~ /shutdownIoContext/;
    die "UPnPContext::shutdown handler does not match expected form\n"
        unless $body =~ /\[(?:\s*this\s*)\]/;

    my $lambda_start = $-[0];
    my $lambda_end = $+[0];
    die "UPnPContext::shutdown handler does not match expected form\n"
        unless substr($body, $lambda_end) =~ /ioContext_->/;
    my $statement_start = rindex(substr($body, 0, $lambda_start), "\n") + 1;
    my ($indent) = substr($body, $statement_start) =~ /^(\s*)/;

    my $patched_body = substr($body, 0, $lambda_start)
        . "[this, shutdownIoContext]"
        . substr($body, $lambda_end);
    substr($patched_body, $statement_start, 0)
        = "${indent}auto shutdownIoContext = __UPNP_IO_CONTEXT__;\n";
    $patched_body =~ s{ioContext_->}{shutdownIoContext->}g;
    $patched_body =~ s/__UPNP_IO_CONTEXT__/ioContext_/g;

    return substr($source, 0, $body_open + 1)
        . $patched_body
        . substr($source, $body_close);
}

@ARGV == 1 or die "usage: fix_upnp_shutdown.pl <dhtnet-source-dir>\n";
my $source_dir = $ARGV[0];
my @candidates;
find(
    sub {
        return unless -f $_;
        return unless /\.(?:cpp|cc|cxx|h|hpp)$/;
        push @candidates, $File::Find::name;
    },
    $source_dir
);

for my $path (@candidates) {
    open(my $in, "<:encoding(UTF-8)", $path) or die "cannot read $path: $!\n";
    local $/;
    my $source = <$in>;
    close($in);

    my $patched = patch_shutdown($source);
    next unless defined $patched;

    if ($patched ne $source) {
        open(my $out, ">:encoding(UTF-8)", $path) or die "cannot write $path: $!\n";
        print {$out} $patched;
        close($out);
        print "patched $path\n";
    } else {
        print "already patched $path\n";
    }
    exit 0;
}

die "UPnPContext::shutdown not found\n";
