#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VLC-style contrib package versioning and build information management.
"""

import json
import os
import sys

from utils.logging import get_logger
from utils.exec import get_sh_executor

log = get_logger(verbose=True, do_indent=True)

PKG_FILE_NAME = "package.json"

"""
A helper class to track and manage a VLC-style contrib package's version
and build information.
"""


class Package:
    """
    A class to hold version and build information extracted from a
    package's JSON versioning file.

    A package can be defined with a json like this:
    {
        "name": "mylibrary",
        "version": "76a5006623539a58262d33458a5605be096b3a10",
        "url": "https://git.example.com/gorblok/mylibrary/archive/__VERSION__.tar.gz",
        "deps": ["mydep"],
        "use_cmake" : true,
        "defines": ["SEGFAULTS=0", "MY_CMAKE_DEFINE=true"],
        "patches": ["some_patch.patch"],
        "win_patches": ["some_windows_line_ending_patch.patch"],
        "project_paths": ["mylibrary-static.vcxproj"],
        "with_env" : "10.0.18362.0",
        "custom_scripts": { "pre_build": [], "build": [], "post_build": [] }
    }
    """

    def __init__(self, pkg_name, base_dir=None):
        self.info = self.__load_info(pkg_name, base_dir)
        if self.info is None:
            log.critical("No package info for " + pkg_name)
            sys.exit(1)

        self.name = self.info["name"]
        self.version = self.info["version"]
        self.build_version = self.__get_build_version(base_dir)

    @staticmethod
    def __load_info(pkg_name, base_dir):
        pkg_json_file = os.path.join(base_dir, pkg_name, PKG_FILE_NAME)
        if not os.path.exists(pkg_json_file):
            return None
        with open(pkg_json_file, encoding="utf8", errors="ignore") as json_file:
            return json.load(json_file)
        
    @staticmethod
    def __get_build_version(base_dir):
        return None


class Versioner:
    def __init__(self):
        pass

    def setup(self, base_dir):
        self.validated = False
        self.base_dir = base_dir
        # check base_dir and verify that its structure is valid.
        if not os.path.exists(self.base_dir):
            log.error("Base directory does not exist: " + self.base_dir)
            return False
        
        self.sh = get_sh_executor()

        self.validated = True


versioner = Versioner()