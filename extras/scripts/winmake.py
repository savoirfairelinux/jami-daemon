#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SPDX-License-Identifier: GPL-3.0-or-later
Copyright (C) 2004-2025 Savoir-faire Linux Inc.

Uses pywinmake to build the daemon and its dependencies.
"""

import os
import time
from datetime import timedelta
import argparse
import sys

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
        "freetype",
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
        # Allow supplying the path to the jom executable via the environment variable JOM_PATH.
        # It is used to accelerate the build processes that use nmake.
        jom_path = os.getenv("JOM_PATH")
        if jom_path and os.path.exists(jom_path):
            log.info(f"Using JOM from environment: {jom_path}")
        else:
            # Try to find JOM in the default Qt installation path
            qt_tools_dir = os.path.join(os.getenv("QTDIR", "C:\\Qt"), "Tools")
            jom_path = os.path.join(qt_tools_dir, "QtCreator", "bin", "jom", "jom.exe")

            if not os.path.exists(jom_path):
                # Fallback to looking in other common Qt Creator paths
                qt_creator_paths = [
                    os.path.join(qt_tools_dir, "QtCreator"),
                    os.path.join(os.getenv("ProgramFiles", "C:\\Program Files"), "Qt Creator"),
                    os.path.join(os.getenv("ProgramFiles(x86)", "C:\\Program Files (x86)"), "Qt Creator")
                ]

                for path in qt_creator_paths:
                    test_path = os.path.join(path, "bin", "jom", "jom.exe")
                    if os.path.exists(test_path):
                        jom_path = test_path
                        break

            if os.path.exists(jom_path):
                log.info(f"Found JOM at: {jom_path}")
            else:
                log.warning("JOM not found. Build performance may be reduced.")
                return

        sh_exec.append_extra_env_vars({"MAKE_TOOL": jom_path})

    versioner.builder.set_vs_env_init_cb(vs_env_init_cb)

    op = Operation.from_string(args.subcommand)
    log.info(f"op={str(op)}, pkgs={args.pkg}, force={str(args.force)}")

    if op == Operation.CLEAN:
        return versioner.clean_all() if args.pkg == "all" else versioner.clean_pkg(args.pkg)
    elif args.pkg == "all":
        return versioner.exec_for_all(op=op, force=args.force)
    else:
        return versioner.exec_for_pkg(args.pkg, op=op, force=args.force, recurse=args.recurse)

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
    if builder.build(pkg):
        log.info(f"Package {pkg.name} built successfully.")
        return True
    else:
        log.error(f"Package {pkg.name} failed to build.")
        return False

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

    success = False
    try:
        # This will search for the base directory containing the contrib directory.
        paths = Paths(base_dir=base_dir, root_names=["daemon", "jami-daemon"])
        log.info(f"Building contribs in {paths}.")
        success = build_contrib(args, paths)
    except RuntimeError as e:
        log.info(f"Building from directory {base_dir}.")
        success = build_from_dir(base_dir, args.out_dir)

    if not success:
        sys.exit(1)

    log.info("--- %s ---" % seconds_to_str(time.time() - start_time))

    # TODO: implement sha512 hash checking
    # TODO: implement define-list accumulation
    # TODO: clarify build vs build_src (also change dir to "native")


if __name__ == "__main__":
    main()
