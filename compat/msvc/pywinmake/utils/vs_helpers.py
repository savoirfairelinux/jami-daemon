#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Helpers for using Visual Studio and MSBuild.
"""

import sys
import os
import platform
import subprocess
import multiprocessing

from utils.logging import get_logger

log = get_logger(verbose=True, do_indent=True)

# Visual Studio helpers
VS_WHERE_PATH = ""
if sys.platform == "win32":
    VS_WHERE_PATH = os.path.join(
        os.environ["ProgramFiles(x86)"],
        "Microsoft Visual Studio",
        "Installer",
        "vswhere.exe",
    )
WIN_SDK_VERSION = "10.0.18362.0"

# Build/project environment information
is_jenkins = "JENKINS_URL" in os.environ
host_is_64bit = (False, True)[platform.machine().endswith("64")]
this_dir = os.path.dirname(os.path.realpath(__file__))


def get_vs_prop(prop):
    """Get a visual studio property."""
    args = [
        "-latest",
        "-products *",
        "-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-property " + prop,
    ]
    cmd = [VS_WHERE_PATH] + args
    output = subprocess.check_output(" ".join(cmd)).decode("utf-8")
    if output:
        return output.splitlines()[0]
    else:
        return None


def find_ms_build():
    """Find the latest msbuild executable."""
    filename = "MSBuild.exe"
    vs_path = get_vs_prop("installationPath")
    if vs_path is None:
        return
    for root, _, files in os.walk(os.path.join(vs_path, "MSBuild")):
        if filename in files:
            return os.path.join(root, filename)


def get_ms_build_args(arch, config_str, toolset=""):
    """Get an array of msbuild command args."""
    msbuild_args = [
        "/nologo",
        "/verbosity:minimal",
        "/maxcpucount:" + str(multiprocessing.cpu_count()),
        "/p:Platform=" + arch,
        "/p:Configuration=" + config_str,
        "/p:useenv=true",
    ]
    if toolset != "":
        msbuild_args.append("/p:PlatformToolset=" + toolset)
    return msbuild_args


def get_vs_env(arch="x64", _platform="", version=""):
    """Get the vcvarsall.bat command."""
    vs_env_cmd = get_vs_env_cmd(arch, _platform, version)
    if vs_env_cmd is None:
        return {}
    env_cmd = f'set path=%path:"=% && {vs_env_cmd} && set'
    proc = subprocess.Popen(env_cmd, shell=True, stdout=subprocess.PIPE)
    stdout, _ = proc.communicate()
    out = stdout.decode("utf-8", errors="ignore").split("\r\n")[5:-1]
    return dict(s.split("=", 1) for s in out)


def get_vs_env_cmd(arch="x64", _platform="", version=""):
    """Get the vcvarsall.bat command."""
    vs_path = get_vs_prop("installationPath")
    if vs_path is None:
        return
    vc_env_init = [
        os.path.join(vs_path, "VC", "Auxiliary", "Build") + r"\"vcvarsall.bat"
    ]
    if _platform != "":
        args = [arch, _platform, version]
    else:
        args = [arch, version]
    if args:
        vc_env_init.extend(args)
    vc_env_init = 'call "' + " ".join(vc_env_init)
    return vc_env_init


def getCMakeGenerator(vs_version):
    if vs_version == "17":
        return '"Visual Studio 17 2022" -A x64'
    elif vs_version == "16":
        return '"Visual Studio 16 2019" -A x64'
    else:
        log.critical("Can't return CMake generator for VS " + vs_version)
        return ""
