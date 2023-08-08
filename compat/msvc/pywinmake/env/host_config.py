#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Various constants and functions to determine the environment in which the
program is running.
"""

import os
import platform
import struct


class Config:
    def __init__(self):
        self.is_jenkins = "JENKINS_URL" in os.environ
        self.host_is_64bit = (False, True)[platform.machine().endswith("64")]
        self.python_is_64bit = (False, True)[8 * struct.calcsize("P") == 64]


config = Config()
