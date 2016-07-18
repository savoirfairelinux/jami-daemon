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

#ifndef DRING_DATATRANSFERI_H
#define DRING_DATATRANSFERI_H

#include "dring.h"

#include <string>
#include <cstdlib> // std::size_t
#include <map>
#include <memory>
#include <ios> // std::streamsize_t

namespace DRing {

/* File transfer */
using DataConnectionId = uint64_t;
using DataTransferId = uint64_t;
using DataTransferSize = std::size_t;

enum DataConnectionCode {
    CODE_UNKNOWN=0,

    // 1xx Informational
    CODE_TRYING=100,
    CODE_PROGRESSING=183,

    // 2xx Success
    CODE_OK=200,
    CODE_CREATED=201,
    CODE_ACCEPTED=202,

    // 4xx Request Error
    CODE_BAD_REQUEST=400,
    CODE_UNAUTHORIZED=401,
    CODE_NOT_FOUND=404,

    // 5xx Process Error
    CODE_INTERNAL=500,
    CODE_NOT_IMPLEMENTED=501,
    CODE_SERVICE_UNAVAILABLE=503,
    CODE_DISCONNECTED=504

    // more may be defined
};

struct DataConnectionInfo {
    std::string account; // account used for transfer
    std::string peer; // remote identification
    bool isClient; // true if we are initiator of the connection (client vs server)
    int code; // latest status code (set by DataConnectionStatus signal change)
};

struct DataTransferInfo {
    DataConnectionId connectionId; // used connection id
    std::string name; // public name sent to peer, (e.g.: a filename for file transfer)
    std::streamsize size; // total size to transfer, -1 if unknown
    int code; // latest status code (set by DataTransferStatus signal change)
};

// strings used to serialize structs
namespace DataTransfer {
    constexpr static const char ACCOUNT       [] = "Account";
    constexpr static const char PEER          [] = "Peer";
    constexpr static const char CODE          [] = "Code";
    constexpr static const char CONNECTION_ID [] = "ConnectionID";
    constexpr static const char NAME          [] = "Name";
    constexpr static const char SIZE          [] = "Size";
} //namespace DRing::DataTransfer

// Signal handlers registration
void registerDataXferHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

// Data connection API
DataConnectionId connectToPeer(const std::string& accountId, const std::string& peerUri);
bool dataConnectionInfo(const DataConnectionId& id, const DataConnectionInfo& info);
bool closeDataConnection(const DataConnectionId& id);

// Generic data transfer API
bool cancelDataTransfer(const DataTransferId& id); // could be used by local or remote (refuse or stop)
std::streamsize dataTransferSentBytes(const DataTransferId& id);
bool dataTransferInfo(const DataTransferId& id, DataTransferInfo& info);

// File transfer API
DataTransferId sendFile(const std::string& accountId, const std::string& peerUri,
                        const std::string& pathname, const std::string& xfer_name={});
void acceptFileTransfer(const DataTransferId& id, const std::string& pathname);

// Signals
struct DataTransferSignal {
    struct DataConnectionStatus {
        constexpr static const char* name = "DataConnectionStatus";
        using cb_type = void(const DataConnectionId& /* connectionId */, int /* code */);
    };
    struct DataTransferStatus {
        constexpr static const char* name = "DataTransferStatus";
        using cb_type = void(const DataTransferId& /* transferId */, int /* code */);
    };
};

} // namespace DRing

#endif // DRING_DATATRANSFERI_H
