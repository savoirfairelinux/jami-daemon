#!/usr/bin/env python3

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PATCH = "0002-disable-nss-keylog-on-Windows.patch"


def test_gnutls_keylog_patch_is_wired():
    package = json.loads((ROOT / "contrib/src/gnutls/package.json").read_text())
    pre_build = package["custom_scripts"]["pre_build"]
    rules = (ROOT / "contrib/src/gnutls/rules.mak").read_text()

    assert f"../../src/gnutls/{PATCH}" in "\n".join(pre_build)
    assert f"$(SRC)/gnutls/{PATCH}" in rules


def test_gnutls_keylog_patch_disables_windows_keylog_init():
    patch = (ROOT / "contrib/src/gnutls" / PATCH).read_text()

    assert "static void keylog_once_init(void)" in patch
    assert "#ifdef _WIN32" in patch
    assert "return;" in patch
    assert "secure_getenv()/fopen()" in patch


if __name__ == "__main__":
    test_gnutls_keylog_patch_is_wired()
    test_gnutls_keylog_patch_disables_windows_keylog_init()
