#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
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

from libc.stdint cimport *

from libcpp.string cimport string
from libcpp cimport bool as boolean
from libcpp.map cimport map as map
from libcpp.vector cimport vector

from ring_api.utils.std cimport *
from ring_api.interfaces.dring cimport *

cdef extern from "callmanager_interface.h" namespace "DRing":

    string placeCall(const string& accountID, const string& to)
    boolean refuse(const string& callID)
    boolean accept(const string& callID)
    boolean hangUp(const string& callID)
    boolean hold(const string& callID)
    boolean unhold(const string& callID)
