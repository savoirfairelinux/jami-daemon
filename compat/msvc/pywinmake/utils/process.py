#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
A helper class and functions for running scripts using various Windows shells.
Shell options are:
    - cmd.exe
    - powershell.exe
    - bash.exe (Git Bash or Windows Subsystem for Linux)
"""

import json
import os
import shlex
import shutil
import subprocess
import sys

from utils.logger import log
from env.host_config import config


class ScriptType:
    ps1 = 0
    cmd = 1
    sh = 2


def shellquote(s, windows=False):
    if not windows:
        return "'" + s.replace("'", "'''") + "'"
    else:
        return '"' + s + '"'


class ShExecutor:
    def __init__(self):
        sys_path = (r"\Sysnative", r"\system32")[config.python_is_64bit]
        full_sys_path = os.path.expandvars("%systemroot%") + sys_path

        # powershell
        self.ps_path = os.path.join(
            full_sys_path, "WindowsPowerShell", "v1.0", "powershell.exe"
        )
        if not os.path.exists(self.ps_path):
            log.error("Powershell not found at %s." % self.ps_path)
            sys.exit(1)

        # bash
        if not os.environ.get("JENKINS_URL"):
            self.sh_path = os.path.join(full_sys_path, "bash.exe")
        else:
            self.sh_path = os.path.join("C:", "Program Files", "Git", "git-bash.exe")

        if not os.path.exists(self.sh_path):
            log.warning("Bash not found at " + self.sh_path)
            self.sh_path = shutil.which("bash.exe")
            if not os.path.exists(self.sh_path):
                log.error("No bash found")
                sys.exit(1)
            else:
                self.sh_path = shellquote(self.sh_path, windows=True)
                log.debug("Using alternate bash found at " + self.sh_path)

        self.base_env_vars = {}
        self.extra_env_vars = {}

    def set_base_env_vars(self, env_vars):
        self.base_env_vars.update(env_vars)

    def set_extra_env_vars(self, env_vars):
        self.extra_env_vars = {}
        self.extra_env_vars = self.base_env_vars.copy()
        self.extra_env_vars.update()

    def exec_script(self, script_type=ScriptType.cmd, script=None, args=[]):
        if script_type is ScriptType.cmd:
            cmd = [script]
            if not args:
                cmd = shlex.split(script)
        elif script_type is ScriptType.ps1:
            cmd = [self.ps_path, "-ExecutionPolicy", "ByPass", script]
        elif script_type is ScriptType.sh:
            cmd = [self.sh_path, "-c ", '"' + script]
        if args:
            cmd.extend(args)
        if script_type is ScriptType.sh:
            cmd[-1] = cmd[-1] + '"'
            cmd = " ".join(cmd)
        run_env = (
            self.extra_env_vars
            if self.extra_env_vars
            else self.base_env_vars
            if self.base_env_vars
            else None
        )
        p = subprocess.Popen(
            cmd,
            shell=True,
            stderr=sys.stderr,
            stdout=sys.stdout,
            env=run_env,
        )
        rtrn, perr = p.communicate()
        rcode = p.returncode
        data = None
        if perr:
            data = json.dumps(perr.decode("utf-8", "ignore"))
        else:
            data = rtrn
        return rcode, data

    def cmd(self, script=None, args=[]):
        return self.exec_script(ScriptType.cmd, script, args)

    def ps1(self, script=None, args=[]):
        return self.exec_script(ScriptType.ps1, script, args)

    def bash(self, script=None, args=[]):
        return self.exec_script(ScriptType.sh, script, args)


sh_exec = ShExecutor()
