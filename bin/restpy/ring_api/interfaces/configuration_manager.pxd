#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Author: Seva Ivanov <seva.ivanov@savoirfairelinux.com>
#         Simon Zeni <simon.zeni@savoirfairelinux.com>
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

    map[string, string] getAccountDetails(const string& accountID)
    map[string, string] getVolatileAccountDetails(const string& accountID)
    void setAccountDetails(const string& accountID,
                           const map[string, string]& details)
    void setAccountActive(const string& accountID, const boolean& active)
    map[string, string] getAccountTemplate(const string& accountType)
    string addAccount(const map[string, string]& details)
    void removeAccount(const string& accoundID)
    vector[string] getAccountList()
    uint64_t sendAccountTextMessage(const string& accountID, const string& to,
                                    const map[string, string]& payloads)
    int getMessageStatus(uint64_t id)
    map[string, string] getTlsDefaultSettings()
    vector[string] getSupportedCiphers(const string& accountID)
    vector[unsigned] getCodecList()
    vector[string] getSupportedTlsMethod()
    map[string, string] getCodecDetails(const string& accountID,
                                        const unsigned& codecId)
    boolean setCodecDetails(const string& accountID, const unsigned& codecId,
                            const map[string, string]& details)
    vector[unsigned] getActiveCodecList(const string& accountID)
    void setActiveCodecList(const string& accountID,
                            const vector[unsigned]& codec_list)
    vector[string] getAudioPluginList()
    map[string, string] validateCertificate(const string& accountId,
                                            const string& certificate)
    map[string, string] getCertificateDetails(const string& certificate)
    vector[string] getPinnedCertificates()
    vector[string] pinCertificate(const vector[uint8_t]& certificate,
                                  const boolean& local)
    boolean unpinCertificate(const string& certId)
    boolean pinRemoteCertificate(const string& accountId, const string& ringID)
    boolean setCertificateStatus(const string& account, const string& certId,
                                 const string& status)
