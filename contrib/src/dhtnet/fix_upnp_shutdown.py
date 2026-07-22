#!/usr/bin/env python
"""Keep dhtnet's UPnP shutdown handler from dereferencing a reset context."""

from __future__ import print_function

import os
import re
import sys


def find_matching_brace(source, open_brace):
    depth = 0
    for index in range(open_brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
    raise RuntimeError("unterminated function body")


def patch_shutdown(source):
    match = re.search(r"\bUPnPContext::shutdown\s*\([^)]*\)\s*(?:noexcept\s*)?\{", source)
    if not match:
        return None

    body_open = source.find("{", match.start())
    body_close = find_matching_brace(source, body_open)
    body = source[body_open + 1:body_close]

    if "shutdownIoContext" in body:
        return source

    lambda_match = re.search(r"\[(?:\s*this\s*)\]", body)
    if not lambda_match or not re.search(r"ioContext_->", body[lambda_match.end():]):
        raise RuntimeError("UPnPContext::shutdown handler does not match expected form")

    statement_start = body.rfind("\n", 0, lambda_match.start()) + 1
    indent = re.match(r"\s*", body[statement_start:]).group(0)

    patched_body = (
        body[:lambda_match.start()]
        + "[this, shutdownIoContext]"
        + body[lambda_match.end():]
    )
    patched_body = (
        patched_body[:statement_start]
        + indent
        + "auto shutdownIoContext = __UPNP_IO_CONTEXT__;\n"
        + patched_body[statement_start:]
    )
    patched_body = patched_body.replace("ioContext_->", "shutdownIoContext->")
    patched_body = patched_body.replace("__UPNP_IO_CONTEXT__", "ioContext_")

    return source[:body_open + 1] + patched_body + source[body_close:]


def main():
    if len(sys.argv) != 2:
        print("usage: fix_upnp_shutdown.py <dhtnet-source-dir>", file=sys.stderr)
        return 2

    source_dir = sys.argv[1]
    for root, _, files in os.walk(source_dir):
        for filename in files:
            if not filename.endswith((".cpp", ".cc", ".cxx", ".h", ".hpp")):
                continue
            path = os.path.join(root, filename)
            with open(path, "r") as source_file:
                source = source_file.read()
            patched = patch_shutdown(source)
            if patched is None:
                continue
            if patched != source:
                with open(path, "w") as source_file:
                    source_file.write(patched)
                print("patched {0}".format(path))
            else:
                print("already patched {0}".format(path))
            return 0

    print("UPnPContext::shutdown not found", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
