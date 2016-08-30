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

from libcpp.string cimport string
from libcpp cimport bool as boolean
from libcpp.utility cimport pair

from ring_api.utils.std cimport *


cdef extern from "dring.h" namespace "DRing":

    cdef enum InitFlag:
        DRING_FLAG_DEBUG = 1 << 0
        DRING_FLAG_CONSOLE_LOG = 1 << 1
        DRING_FLAG_AUTOANSWER = 1 << 2

    const char* version()
    boolean init(InitFlag flags)
    boolean start(const string& config_file)
    boolean start()
    void fini()
    void pollEvents()

    # CallbackWrapper class and exportable_callback method are not needed.
    # The register of cython callbacks happens directly in C++ in the
    # CallbacksClient class of callbacks/cb_client.cpp
