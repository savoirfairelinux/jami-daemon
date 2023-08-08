#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test pywinmake modules
"""

import os
from pathlib import Path

from utils.logger import log, logger
from utils.process import sh_exec
from env.vs_env import VSEnv
from builders.msb_builder import msb_builder
from package.versioner import versioner


# A structure to hold all the absolute paths used by the build system.
class Paths:
    def __init__(self, base_dir=None):
        self.daemon_dir = None
        if base_dir is None:
            base_dir = Path(__file__)

            # Climb up the directory tree until we find the libjami dir or hit root.
            while base_dir.name not in ["daemon", "jami-daemon"]:
                base_dir = base_dir.parent
                if base_dir == base_dir.parent:
                    raise RuntimeError("Could not find daemon dir.")

            self.daemon_dir = base_dir
        else:
            self.daemon_dir = os.path.abspath(base_dir)

        if not self.daemon_dir or not os.path.isdir(self.daemon_dir):
            raise RuntimeError("Could not find daemon dir.")

        self.daemon_msvc_dir = os.path.join(self.daemon_dir, "msvc")
        self.contrib_dir = os.path.join(self.daemon_dir, "contrib")


def main():
    logger.init(verbose=True, do_indent=True)

    # # test sh_exec
    # ret = sh_exec.bash("echo", ["hello world"])
    # log.debug(f"sh_exec.cmd() returned: {ret}")

    paths = Paths()
    
    # vs_env = VSEnv()
    # if not vs_env.validated:
    #     log.error("A valid Visual Studio env is not installed on this machine.")
    #     return

    # # Pretty-print the all the aquired data.
    # log.info("Using the following VS build environment:")
    # print(f"\tVisual Studio version: {vs_env.vs_version}")
    # print(f"\tWindows SDK version: {vs_env.sdk_version} ({vs_env.latest_sdk_version})")
    # print(f"\tMSBuild path: {vs_env.msbuild_path}")
    # print(f"\tMSVC default toolset: {vs_env.toolset_default}")
    # print(f"\tCMake generator string: {vs_env.cmake_generator}")

    # # test msb builder
    # msb_builder.setup_env(vs_env)
    # print(f"\tMSBuild args: {msb_builder.msbuild_args}")

    # test versioner with current dir
    versioner.setup(base_dir=paths.contrib_dir)
    if not versioner.validated:
        log.error("Versioner is not validated.")
        return
    
    # check a package
    versioner.get_package_status("opendht")


if __name__ == "__main__":
    main()
