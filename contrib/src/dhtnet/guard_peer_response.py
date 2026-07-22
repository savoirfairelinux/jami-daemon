#!/usr/bin/env python3

import argparse
import re
import sys
from pathlib import Path


GUARD = """\
    if (!response.from)
        return;
"""


def add_guard(source):
    if "!response.from" in source:
        return source, False

    signature = "ConnectionManager::Impl::onPeerResponse"
    signature_index = source.find(signature)
    if signature_index < 0:
        raise RuntimeError(f"{signature} definition not found")

    body_index = source.find("{", signature_index)
    if body_index < 0:
        raise RuntimeError(f"{signature} body not found")

    body_prefix = source[signature_index:body_index]
    if not re.search(r"\bresponse\b", body_prefix):
        raise RuntimeError(f"{signature} does not use the expected response parameter")

    insert_index = body_index + 1
    newline = "\r\n" if "\r\n" in source else "\n"
    guard = newline + GUARD.replace("\n", newline)
    return source[:insert_index] + guard + source[insert_index:], True


def patch_file(path):
    source_path = Path(path)
    with source_path.open("r", encoding="utf-8", newline="") as source_file:
        source = source_file.read()
    patched, changed = add_guard(source)
    if changed:
        with source_path.open("w", encoding="utf-8", newline="") as source_file:
            source_file.write(patched)
    return changed


def self_test():
    sample = """\
void
ConnectionManager::Impl::onPeerResponse(PeerConnectionRequest&& response)
{
    auto deviceId = response.from->getId();
}
"""
    patched, changed = add_guard(sample)
    assert changed
    assert "if (!response.from)\n        return;\n" in patched
    assert "if (!response.from)" in patched.split("response.from->getId()")[0]

    second, changed_again = add_guard(patched)
    assert not changed_again
    assert second == patched


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", nargs="?")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        self_test()
        return 0

    if not args.source:
        parser.error("source path is required unless --self-test is used")

    try:
        patch_file(args.source)
    except Exception as exc:
        print(f"guard_peer_response.py: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
