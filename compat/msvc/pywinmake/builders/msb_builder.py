#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Used to build a package with msbuild
"""

import sys
import re
import fileinput

from env.host_config import config
from utils.logger import log
from utils.process import sh_exec


class MsbBuilder:
    def __init__(self):
        self.msbuild = None
        self.msbuild_args = None

    # Requires a validated VS environment to be set up.
    def setup_env(self, vs_env):
        self.vsenv = vs_env
        self.msbuild = self.vsenv.msbuild_path
        self.set_msbuild_configuration()

    def set_msbuild_configuration(self, with_env="false", configuration="Release"):
        self.msbuild_args = self.vsenv.get_ms_build_args(configuration, with_env)

    def build(self, pkg_name, proj_path, sdk_version, toolset):
        if config.is_jenkins:
            self.replace_vs_prop(proj_path, "DebugInformationFormat", "None")
        # force chosen sdk, toolset
        self.replace_vs_prop(proj_path, "WindowsTargetPlatformVersion", sdk_version)
        self.replace_vs_prop(proj_path, "PlatformToolset", toolset)
        
        args = []
        args.extend(self.msbuild_args)
        args.append(proj_path)
        result = sh_exec.exec_batch(self.msbuild, args)
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


msb_builder = MsbBuilder()
