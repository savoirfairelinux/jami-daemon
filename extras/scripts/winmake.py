#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2023 Savoir-faire Linux

Uses pywinmake to build the daemon and its dependencies.
"""

import os
import time
from datetime import timedelta
import argparse

from pywinmake.utils import log, logger, sh_exec
from pywinmake.package import Versioner, Paths, Operation, Package
from pywinmake.package import get_default_parsed_args
from pywinmake.builders import MetaBuilder


def seconds_to_str(elapsed=None):
    return str(timedelta(seconds=elapsed))

def build_contrib(args, paths):
    versioner = Versioner(base_dir=paths.contrib_dir)

    # Exclude packages that are not needed for the daemon.
    # TODO: libjami: move these to their own contribs and remove the package.json files
    versioner.exclusion_list = [
        "liburcu",
        "lttng-ust",
        "minizip",
        "onnx",
        "opencv",
        "opencv_contrib",
    ]
    versioner.extra_output_dirs = ["msvc"]

    def vs_env_init_cb():
        # TODO: libjami: replace DAEMON_DIR in ffnvcodec with something else
        # TODO: libjami: CONTRIB_SRC_DIR is used by ffmpeg
        # NOTE: MSYS2_BIN defined in build_ffmpeg.bat (might want to remove it)
        # NOTE: paths.base_dir should be the daemon dir if initialized correctly
        sh_exec.append_extra_env_vars(
            {
                "DAEMON_DIR": paths.base_dir,
                "CONTRIB_SRC_DIR": os.path.join(paths.contrib_dir, "src"),
            }
        )
        # Find JOM if it is installed. (default C:/Qt/Tools/QtCreator/bin/jom)
        # Used to accelerate the build process when normally using nmake.
        qt_tools_dir = os.path.join(os.getenv("QTDIR", "C:\Qt"), "Tools")
        jom_path = os.path.join(qt_tools_dir, "QtCreator", "bin", "jom", "jom.exe")
        if os.path.exists(jom_path):
            log.info("Found JOM at " + jom_path)
            sh_exec.append_extra_env_vars({"MAKE_TOOL": jom_path})

    versioner.builder.set_vs_env_init_cb(vs_env_init_cb)

    op = Operation.from_string(args.subcommand)
    log.info(f"op={str(op)}, pkgs={args.pkg}, force={str(args.force)}")

    if op == Operation.CLEAN:
        versioner.clean_all() if args.pkg == "all" else versioner.clean_pkg(args.pkg)
    elif args.pkg == "all":
        versioner.exec_for_all(op=op, force=args.force)
    else:
        versioner.exec_for_pkg(args.pkg, op=op, force=args.force, recurse=args.recurse)

def build_from_dir(path, out_dir=None):
    """Pretty much just for building libjami."""
    # Make sure our paths are absolute.
    path = os.path.abspath(path)
    out_dir = os.path.abspath(out_dir)
    # Initialize the builder.
    builder = MetaBuilder(base_dir=path)
    # Build the package at the given path.
    out_dir = os.path.join(path, "build") if out_dir is None else out_dir
    pkg = Package(src_dir=path, buildsrc_dir=out_dir)
    builder.build(pkg)

def main():
    start_time = time.time()

    parser = argparse.ArgumentParser(
        description="Build the daemon and its dependencies."
    )
    parser.add_argument(
        "--base-dir",
        default=None,
        help="A directory containing a package or the contrib dir.",
    )
    parser.add_argument(
        "--out-dir",
        default=".",
        help="Output the build directory for the single package.",
    )
    args = get_default_parsed_args(parser)
    base_dir = args.base_dir

    logger.init(args.log_level, args.verbose, args.indent)
    sh_exec.set_quiet_mode(args.quiet)
    sh_exec.set_debug_cmd(args.verbosity == 2)

    build_from_dir_only = False
    try:
        # This will search for the base directory containing the contrib directory.
        paths = Paths(base_dir=base_dir, root_names=["daemon", "jami-daemon"])
        build_contrib(args, paths)
    except RuntimeError as e:
        build_from_dir_only = True

    if build_from_dir_only:
        build_from_dir(base_dir, args.out_dir)

    log.info("--- %s ---" % seconds_to_str(time.time() - start_time))

    # TODO: implement sha512 hash checking
    # TODO: implement define-list accumulation
    # TODO: clarify build vs build_src (also change dir to "native")


if __name__ == "__main__":
    main()
