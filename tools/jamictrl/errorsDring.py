#
# Copyright (C) 2004-2023 Savoir-faire Linux Inc.
#
# Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
#

"""Internal exceptions"""


class libjamiCtrlError(Exception):
    """Base class for all our exceptions."""

    def __init__(self, help=None):
        self.help = str(help)

    def __str__(self):
        return self.help

class libjamiCtrlDBusError(libjamiCtrlError):
    """General error for dbus communication"""

class libjamiCtrlDeamonError(libjamiCtrlError):
    """General error for daemon communication"""

class libjamiCtrlAccountError(libjamiCtrlError):
    """General error for account handling"""
