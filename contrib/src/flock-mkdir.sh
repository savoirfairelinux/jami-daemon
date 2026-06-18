#!/bin/sh
# Portable flock(1) emulation using mkdir(1).
# mkdir is atomic on POSIX filesystems, making it safe as a lock primitive.
# Usage: flock-mkdir.sh <lockpath> <cmd> [args...]
LOCKDIR="${1}.lock"
shift
while ! mkdir "$LOCKDIR" 2>/dev/null; do
    sleep 1
done
trap 'rmdir "$LOCKDIR"' EXIT INT TERM
"$@"
