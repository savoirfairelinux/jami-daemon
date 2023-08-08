#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VLC-style contrib package versioning and build information management.
"""

import hashlib
import json
import os
import sys

from utils.logger import log
from utils.process import sh_exec


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

        self.name = self.info.get('name')
        self.version = self.info.get('version')
        self.url = self.info.get('url')
        self.deps = self.info.get('deps', [])
        self.use_cmake = self.info.get('use_cmake', False)
        self.defines = self.info.get('defines', [])
        self.patches = self.info.get('patches', [])
        self.win_patches = self.info.get('win_patches', [])
        self.project_paths = self.info.get('project_paths', [])
        self.with_env = self.info.get('with_env', None)
        self.custom_scripts = self.info.get('custom_scripts', {})

        self.ver_uptodate = False
        self.build_version = None
        self.build_uptodate = False
        self.md5 = None

    def print_info(self):
        # pretty-print the json info
        # log.info("Package info:" + json.dumps(self.info, indent=4))
        log.info("Package: " + self.name)
        log.info("Version: " + self.version)
        log.info("URL: " + self.url)
        log.info("Dependencies: " + str(self.deps))
        log.info("Use CMake: " + str(self.use_cmake))
        log.info("Defines: " + str(self.defines))
        log.info("Patches: " + str(self.patches))
        log.info("Windows patches: " + str(self.win_patches))
        log.info("Project paths: " + str(self.project_paths))
        log.info("With env: " + str(self.with_env))
        log.info("Custom scripts: " + str(self.custom_scripts))

        log.info("Build version: " + str(self.build_version))
        log.info("MD5: " + str(self.md5))
        log.info("Version Up-to-date: " + str(self.ver_uptodate))
        log.info("Build Up-to-date: " + str(self.build_uptodate))

    @staticmethod
    def __load_info(pkg_name, base_dir):
        pkg_json_file = os.path.join(base_dir, pkg_name, PKG_FILE_NAME)
        if not os.path.exists(pkg_json_file):
            return None
        with open(pkg_json_file, encoding="utf8", errors="ignore") as json_file:
            return json.load(json_file)


class Versioner:
    def __init__(self):
        pass

    def setup(self, base_dir):
        self.validated = False
        self.pkgs = []
        self.base_dir = base_dir
        # check base_dir and verify that its structure is valid.
        if not os.path.exists(self.base_dir):
            log.error("Base directory does not exist: " + self.base_dir)
            return False
        
        self.src_dir = os.path.join(self.base_dir, "src")
        self.build_dir = os.path.join(self.base_dir, "build")
        self.fetch_dir = os.path.join(self.base_dir, "tarballs")

        self.validated = True

    # Load a package's versioning information from its JSON file, and
    # analyze it to determine the package's version and build information,
    # as well as its dependencies.
    def get_package_status(self, pkg_name):
        pkg = Package(pkg_name, self.src_dir)
        if pkg is None:
            log.error("No package info for " + pkg_name)
            return None
        
        # Check if the package has already been built, if so, set the build
        # version, and mark it as up-to-date if the package's src dir md5 matches
        # the build version (src dir md5 of the last build).
        pkg.md5 = self.get_md5_for_pkg(pkg_name)
        pkg_build_dir = os.path.join(self.build_dir, pkg_name)
        if os.path.exists(pkg_build_dir):
            pkg.build_version = self.get_build_version(pkg_name)
            if pkg.build_version == pkg.md5:
                pkg.ver_uptodate = True

        pkg.build_uptodate = self.is_build_uptodate(pkg_name)

        pkg.print_info()
        
        # Add the package to the list of packages.
        self.pkgs.append(pkg)

    # The build version file is named .<pkg_name> within contrib build directory,
    # and contains the package's version as a single line of text.
    def get_build_version(self, pkg_name):
        build_version_file = os.path.join(self.build_dir, "." + pkg_name)
        if not os.path.exists(build_version_file):
            return None
        with open(build_version_file, "r") as f:
            return f.readline().strip()

    @staticmethod 
    def get_md5_for_pkg(path):
        hasher = hashlib.md5()
        for root, _, files in os.walk(path, topdown=True):
            for name in files:
                fileName = (os.path.join(root, name))
                with open(str(fileName), 'rb') as aFile:
                    buf = aFile.read()
                    hasher.update(buf)
        return hasher.hexdigest()
    
    # Check if the package's build directory is up-to-date with respect to
    # the package's source directory. Used to trigger rebuilds when the
    # source build directory has been modified.
    def is_build_uptodate(self, pkg_name):
        build_version_file = os.path.join(self.build_dir, "." + pkg_name)
        pkg_build_dir = os.path.join(self.build_dir, pkg_name)
        file_list = []
        for path, _, files in os.walk(pkg_build_dir):
            for name in files:
                file_list.append(os.path.join(path, name))
        if not file_list:
            return False
        latest_file = max(file_list, key=os.path.getmtime)
        t_mod = os.path.getmtime(latest_file)
        t_build = os.path.getmtime(build_version_file)
        return t_mod < t_build
    
    def track_build(self, pkg_name, version):
        build_version_file = os.path.join(self.build_dir, "." + pkg_name)
        f = open(build_version_file, "w+", encoding="utf8", errors='ignore')
        f.write(version)
        f.close()


versioner = Versioner()
