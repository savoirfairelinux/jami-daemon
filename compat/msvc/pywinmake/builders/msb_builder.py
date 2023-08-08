#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Used to build a package with msbuild
"""

import os
import sys
import re
import fileinput

from env.host_config import config
from utils.logging import get_logger
from utils.exec import get_sh_executor
from utils.singleton import Singleton

log = get_logger(verbose=True, do_indent=True)


@Singleton
class MsbBuilder:
    def __init__(self):
        self.msbuild = None
        self.msbuild_args = None

    # Requires a validated VS environment to be set up.
    def setup_env(self, vs_env):
        self.vsenv = vs_env
        self.sh = get_sh_executor()
        self.msbuild = self.vsenv.msbuild_path
        self.set_msbuild_configuration()

    def set_msbuild_configuration(self, with_env="false", configuration="Release"):
        self.msbuild_args = self.vsenv.get_ms_build_args(configuration, with_env)

    def build(self, pkg_name, proj_path, sdk_version, toolset):
        if not os.path.isfile(self.msbuild):
            raise IOError("msbuild.exe not found. path=" + self.msbuild)
        if config.is_jenkins:
            log.info("Jenkins Clear DebugInformationFormat")
            self.__class__.replace_vs_prop(proj_path, "DebugInformationFormat", "None")
        # force chosen sdk
        self.__class__.replace_vs_prop(
            proj_path, "WindowsTargetPlatformVersion", sdk_version
        )
        # force chosen toolset
        self.__class__.replace_vs_prop(proj_path, "PlatformToolset", toolset)
        args = []
        args.extend(self.msbuild_args)
        args.append(proj_path)
        result = self.sh.exec_batch(self.msbuild, args)
        if result[0] == 1:
            log.error("Build failed when building " + pkg_name)
            sys.exit(1)

    @staticmethod
    def replace_vs_prop(filename, prop, val):
        p = re.compile(r"(?s)<" + prop + r"\s?.*?>(.*?)<\/" + prop + r">")
        val = r"<" + prop + r">" + val + r"</" + prop + r">"
        with fileinput.FileInput(filename, inplace=True) as file:
            for line in file:
                print(re.sub(p, val, line), end="")


def get_msb_builder():
    return MsbBuilder.instance()
