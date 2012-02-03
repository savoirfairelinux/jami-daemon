#!/bin/sh -e

# Workaround for http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=565663
mkdir -p m4

HOOKS_DIR="`git rev-parse --git-dir`"/hooks
# install pre-commit hook for doing clean commits
if test ! \( -x ${HOOKS_DIR}/hooks/pre-commit -a -L ${HOOKS_DIR}/pre-commit \);
then
    rm -f ${HOOKS_DIR}/pre-commit
    ln -s ${HOOKS_DIR}/pre-commit.sample ${HOOKS_DIR}/pre-commit
fi

autoreconf --force --install --verbose -Wall -I m4
