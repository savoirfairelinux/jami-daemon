#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Author: Seva Ivanov <seva.ivanov@savoirfairelinux.com>
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

cdef extern from "configurationmanager_interface.h" namespace "DRing":

    # account id != ring id
    vector[string] getAccountList()
    map[string, string] getAccountDetails(const string& accountID);

    # to: ring_id_dest, payloads: map[<mime-type>, <message>]
    uint64_t sendAccountTextMessage(const string& accountID, const string& to,
            const map[string, string]& payloads);
