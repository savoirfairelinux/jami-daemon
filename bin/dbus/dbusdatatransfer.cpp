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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dbusdatatransfer.h"
#include "datatransfer_interface.h"
#include "string_utils.h"

DBusDataTransfer::DBusDataTransfer(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/DataTransfer")
{}

auto
DBusDataTransfer::connectToPeer(const std::string& accountId, const std::string& peerUri) -> decltype(DRing::connectToPeer(accountId, peerUri))
{
    return DRing::connectToPeer(accountId, peerUri);
}

std::map<std::string, std::string>
DBusDataTransfer::getDataConnectionInfo(const uint64_t& dataConnectionId)
{
    DRing::DataConnectionInfo info;
    if (DRing::dataConnectionInfo(dataConnectionId, info)) {
        // serialize the struct
        return {
            {DRing::DataTransfer::ACCOUNT, info.account},
            {DRing::DataTransfer::PEER,    info.peer},
            {DRing::DataTransfer::CODE,    ring::to_string(info.code)},
        };
    } else {
        return {};
    }
}

auto
DBusDataTransfer::closeDataConnection(const uint64_t& dataConnectionId) -> decltype(DRing::closeDataConnection(dataConnectionId))
{
    return DRing::closeDataConnection(dataConnectionId);
}

auto
DBusDataTransfer::cancelDataTransfer(const uint64_t& dataTransferId) -> decltype(DRing::cancelDataTransfer(dataTransferId))
{
    return DRing::cancelDataTransfer(dataTransferId);
}

int64_t
DBusDataTransfer::getDataTransferSentBytes(const uint64_t& dataTransferId)
{
    return DRing::dataTransferSentBytes(dataTransferId);
}

std::map<std::string, std::string>
DBusDataTransfer::getDataTransferInfo(const uint64_t& dataTransferId)
{
    DRing::DataTransferInfo info;
    if (DRing::dataTransferInfo(dataTransferId, info)) {
        // serialize the struct
        return {
            {DRing::DataTransfer::CONNECTION_ID, ring::to_string(info.connectionId)},
            {DRing::DataTransfer::NAME,          info.name},
            {DRing::DataTransfer::SIZE,          ring::to_string(info.size)},
            {DRing::DataTransfer::CODE,          ring::to_string(info.code)},
        };
    } else {
        return {};
    }
}

auto
DBusDataTransfer::sendFile(const std::string& accountID,
                           const std::string& peerUri,
                           const std::string& pathname,
                           const std::string& name) -> decltype(DRing::sendFile(accountID, peerUri, pathname, name))
{
    return DRing::sendFile(accountID, peerUri, pathname, name);
}

void
DBusDataTransfer::acceptFileTransfer(const uint64_t& dataTransferId, const std::string& pathname)
{
    DRing::acceptFileTransfer(dataTransferId, pathname);
}
