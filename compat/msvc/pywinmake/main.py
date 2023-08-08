#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test pywinmake modules
"""

from utils.logging import get_logger
from utils.exec import get_sh_executor
from env.vs_env import VSEnv
from builders.msb_builder import get_msb_builder

log = get_logger(verbose=True, do_indent=True)
sh = get_sh_executor()


def main():
    vs_env = VSEnv()
    if not vs_env.validated:
        log.error("A valid Visual Studio env is not installed on this machine.")
        return

    # Pretty-print the all the aquired data.
    log.info("Using the following VS build environment:")
    print(f'\tVisual Studio version: {vs_env.vs_version}')
    print(f'\tWindows SDK version: {vs_env.sdk_version} ({vs_env.latest_sdk_version})')
    print(f'\tMSBuild path: {vs_env.msbuild_path}')
    print(f'\tMSVC default toolset: {vs_env.toolset_default}')
    print(f'\tCMake generator string: {vs_env.cmake_generator}')

    # test msb builder
    msb = get_msb_builder()
    msb.setup_env(vs_env)
    log.info("Checking MsbBuilder...")
    print(f"\tMSBuild args: {msb.msbuild_args}")


if __name__ == "__main__":
    main()
