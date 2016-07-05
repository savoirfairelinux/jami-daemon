/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#ifndef __RING_DBUSDATATRANSFER_H__
#define __RING_DBUSDATATRANSFER_H__

#include <string>
#include <map>

#include "dbus_cpp.h"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbusdatatransfer.adaptor.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

class DBusDataTransfer :
    public cx::ring::Ring::DataTransfer_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor
{
public:
    DBusDataTransfer(DBus::Connection& connection);

    // Methods
    // These wrap the datatransfer_interface.h API in order to (de)serialize the data structures
    uint64_t connectToPeer(const std::string& accountId, const std::string& peerUri);
    std::map<std::string, std::string> getDataConnectionInfo(const uint64_t& dataConnectionId);
    bool closeDataConnection(const uint64_t& dataConnectionId);

    bool cancelDataTransfer(const uint64_t& dataTransferId);
    int64_t getDataTransferSentBytes(const uint64_t& dataTransferId);
    std::map<std::string, std::string> getDataTransferInfo(const uint64_t& dataTransferId);

    uint64_t sendFile(const std::string& accountID, const std::string& peerUri,
                         const std::string& pathname, const std::string& name);
    void acceptFileTransfer(const uint64_t& dataTransferId, const std::string& pathname);

};

#endif // __RING_DBUSDATATRANSFER_H__
