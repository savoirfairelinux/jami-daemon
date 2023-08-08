#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Helpers for using Visual Studio and MSBuild.
"""

import multiprocessing
import os
import subprocess
import winreg


from utils.logging import get_logger

log = get_logger(verbose=True, do_indent=True)


# A class used to discover and manage the Visual Studio env.
class VSEnv:
    def __init__(self, arch="x64", version=None):
        self.arch = arch

        # VSWhere is a tool that locates installed Visual Studio instances.
        # It is required to discover various properties of the VS installation.
        self.vswhere_path = self.__get_vswhere_path()
        if self.vswhere_path is None:
            log.critical("Could not find vswhere.exe.")
            return

        self.vs_version = version or self.__get_latest_vs_version()
        if self.vs_version is None:
            log.critical("Could not find Visual Studio.")
            return
        
        if self.vs_version == "17":
            self.toolset_default = "143"
        elif self.vs_version == "16":
            self.toolset_default = "142"
        else:
            self.toolset_default = "144"

        self.sdk_version = "10.0.18362.0"
        self.validated = False
        log.info(
            "Discovering VS build environment " f"({self.arch} / {self.sdk_version})..."
        )

        self.__get_windows_sdk_version_info()
        # Make sure the SDK version we want is installed.
        if self.sdk_version not in self.sdk_versions:
            log.critical(f"SDK version {self.sdk_version} is not installed.")
            return

        self.vs_env = self.__get_vs_env(self.arch, self.sdk_version)
        if self.vs_env is None:
            log.critical("Could not find Visual Studio environment.")
            return

        self.msbuild_path = self.__find_ms_build()
        if self.msbuild_path is None:
            log.critical("Could not find MSBuild.")
            return

        self.cmake_generator = self.__get_cmake_generator(self.vs_version)
        if self.cmake_generator is None:
            log.critical("Could not find CMake generator.")
            return

        self.validated = True

    @staticmethod
    def get_ms_build_args(arch, config_str, toolset="", use_env=True):
        """Get an array of msbuild command args."""
        msbuild_args = [
            "/nologo",
            "/verbosity:minimal",
            "/maxcpucount:" + str(multiprocessing.cpu_count()),
            "/p:Platform=" + arch,
            "/p:Configuration=" + config_str,
            "/p:useenv=" + str(use_env).lower(),
        ]
        if toolset != "":
            msbuild_args.append("/p:PlatformToolset=" + toolset)
        return msbuild_args

    @staticmethod
    def __get_vswhere_path():
        return os.path.join(
            os.environ["ProgramFiles(x86)"],
            "Microsoft Visual Studio",
            "Installer",
            "vswhere.exe",
        )

    def __get_latest_vs_version(self):
        args = [
            "-latest",
            "-products *",
            "-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property installationVersion",
        ]
        cmd = [self.vswhere_path] + args
        output = subprocess.check_output(" ".join(cmd)).decode("utf-8")
        if output:
            return output.splitlines()[0].split(".")[0]
        else:
            return

    def __get_vs_env_cmd(self, arch="x64", version=""):
        """Get the vcvarsall.bat command."""
        vs_path = self.__get_vs_prop("installationPath")
        if vs_path is None:
            return
        vc_env_init = [
            os.path.join(vs_path, "VC", "Auxiliary", "Build") + r"\"vcvarsall.bat"
        ]
        vc_env_init.extend([arch, version])
        vc_env_init = 'call "' + " ".join(vc_env_init)
        return vc_env_init

    def __get_vs_env(self, arch="x64", sdk_version="latest"):
        """Get the vcvarsall.bat command."""
        # example call: vcvarsall.bat [arch] [platform_type] [winsdk_version]
        # We omit the platform_type because we don't build store/uwp apps.
        if sdk_version == "latest":
            sdk_version = self.latest_sdk_version
        vs_env_cmd = self.__get_vs_env_cmd(arch, sdk_version)
        if vs_env_cmd is None:
            return {}
        env_cmd = f'set path=%path:"=% && {vs_env_cmd} && set'
        proc = subprocess.Popen(env_cmd, shell=True, stdout=subprocess.PIPE)
        # If this fails, it's probably because the SDK version is not installed.
        # Check the return code and print the output.
        (
            stdout,
            _,
        ) = proc.communicate()
        retcode = proc.returncode
        if retcode != 0:
            log.critical(f"Failed to load VS environment for sdk: {sdk_version}")
            return None
        out = stdout.decode("utf-8", errors="ignore").split("\r\n")[5:-1]
        return dict(s.split("=", 1) for s in out)

    def __get_vs_prop(self, prop):
        """Get a visual studio property."""
        args = [
            "-latest",
            "-products *",
            "-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property " + prop,
        ]
        cmd = [self.vswhere_path] + args
        output = subprocess.check_output(" ".join(cmd)).decode("utf-8")
        if output:
            return output.splitlines()[0]
        else:
            return None

    def __find_ms_build(self):
        """Find the latest msbuild executable."""
        filename = "MSBuild.exe"
        vs_path = self.__get_vs_prop("installationPath")
        if vs_path is None:
            return
        for root, _, files in os.walk(os.path.join(vs_path, "MSBuild")):
            if filename in files:
                return os.path.join(root, filename)

    def __get_cmake_generator(self, vs_version):
        """Get the cmake generator string."""
        if vs_version == "17":
            return f'"Visual Studio 17 2022" -A {self.arch}'
        elif vs_version == "16":
            return f'"Visual Studio 16 2019" -A {self.arch}'
        else:
            log.critical("Unsupported Visual Studio version: " + vs_version)
            return None

    @staticmethod
    def __get_installed_windows_sdk_versions():
        try:
            reg_path = os.path.join(
                "SOFTWARE", "Microsoft", "Windows Kits", "Installed Roots"
            )
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, reg_path) as key:
                subkeys_count = winreg.QueryInfoKey(key)[0]
                sdk_versions = []

                for i in range(subkeys_count):
                    sdk_versions.append(winreg.EnumKey(key, i))

                if sdk_versions:
                    return sdk_versions
                else:
                    return None
        except Exception as e:
            log.critical(f"Error: {e}")
            return None

    def __get_windows_sdk_version_info(self):
        self.sdk_versions = self.__get_installed_windows_sdk_versions()
        if self.sdk_versions:
            self.latest_sdk_version = sorted(self.sdk_versions)[-1]
        else:
            self.latest_sdk_version = "None"
