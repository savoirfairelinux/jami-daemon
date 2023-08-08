#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
A wrapper around the logging module to provide a more useful logging format.
"""

import logging
import traceback

from utils.singleton import Singleton

root_logger = logging.getLogger(__name__)


class CustomAdapter(logging.LoggerAdapter):
    @staticmethod
    def indent():
        indentation_level = len(traceback.extract_stack())
        return indentation_level - 4 - 2  # Remove logging infrastructure frames

    def process(self, msg, kwargs):
        return "{i}{m}".format(i=" " * (self.indent()), m=msg), kwargs


@Singleton
class Logger:
    def __init__(self):
        self.logger = None

    def init(self, lvl=logging.DEBUG, verbose=False, do_indent=False):
        if self.logger is not None:
            return
        self.logger = self.__get_logger(lvl, verbose, do_indent)

    def get(self):
        return self.logger

    def __get_logger(self, lvl=logging.DEBUG, verbose=False, do_indent=False):
        # Try to use coloredlogs if it's available.
        format = ""
        if verbose:
            location_part = "%(filename)16s:%(funcName)10s:%(lineno)4s"
            format = "[ %(levelname)-8s %(created).1f " + location_part + " ] "
        fmt = format + "%(message)s"
        try:
            import coloredlogs

            coloredlogs.install(
                level=lvl,
                logger=root_logger,
                fmt=fmt,
                level_styles={
                    "debug": {"color": "blue"},
                    "info": {"color": "green"},
                    "warn": {"color": "yellow"},
                    "error": {"color": "red"},
                    "critical": {"color": "red", "bold": True},
                },
                field_styles={
                    "asctime": {"color": "magenta"},
                    "created": {"color": "magenta"},
                    "levelname": {"color": "cyan"},
                    "funcName": {"color": "black", "bold": True},
                    "lineno": {"color": "black", "bold": True},
                },
            )
        except ImportError:
            root_logger.setLevel(logging.DEBUG)
            logging.basicConfig(level=lvl, format=fmt)

        # Return a logger that either indents or doesn't.
        if do_indent:
            global log
            return CustomAdapter(logging.getLogger(__name__), {})
        else:
            return logging.getLogger(__name__)


def get_logger(lvl=logging.DEBUG, verbose=False, do_indent=False):
    Logger.instance().init(lvl=lvl, verbose=verbose, do_indent=do_indent)
    return Logger.instance().get()
