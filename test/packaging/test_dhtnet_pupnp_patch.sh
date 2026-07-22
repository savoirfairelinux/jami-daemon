#!/bin/sh

set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
patch_name="0001-upnp-stop-search-after-control-point-shutdown.patch"
patch_path="$root_dir/contrib/src/dhtnet/$patch_name"

test -f "$patch_path"
grep -Fq 'ctrlptHandle_ == -1' "$patch_path"
grep -Fq "\$(APPLY) \$(SRC)/dhtnet/$patch_name" "$root_dir/contrib/src/dhtnet/rules.mak"
grep -Fq "\"$patch_name\"" "$root_dir/contrib/src/dhtnet/package.json"
