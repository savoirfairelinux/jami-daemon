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

namespace DRing {

/* File transfer */
using DataConnectionId = std::string;
using DataTransferId = std::string;
using DataTransferSize = std::size_t;

enum class DataConnectionStatus : int {
    DISCONNECTED=0, // No connection available (reason provided)
    CONNECTING, // trying to find peer and create a secure connection
    IDLE, // no active transfer
    BUSY, // one or more transfer are active
};

enum class DataTransferError : int {
    NONE=0,
    SYSTEM, // system ressource error
    IO, // underlaying transport io error
    REFUSED, // peer not found or refused the connection
    LOST, // connection lost

    // more later...
};

struct DataConnectionInfo {
    DataConnectionStatus status; // connection status
    DataTransferError error;
    std::string peer; // remote identification
    std::string account; // account used for transfer
};

enum class FileTransferStatus : int {
    FINISHED=0, // all data sent and received successfully
    PENDING, // sent by local node, waiting for peer approval
    REQUESTED, // remote file sent by peer, waiting for confirmation
    PROGRESSING, // data in transfer
    CANCELLED, // could be set in case of cancelFileTransfer() or disconnection by error
    REFUSED, // refused or cancelled by peer

   // more later...
};

struct FileTransferInfo {
    std::string connection; // used connection id
    std::string filename;
    std::size_t size;
    FileTransferStatus status;
    DataTransferError error;
};

void registerDataXferHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

std::string connectToPeer(const std::string& accountId, const std::string& peerUri);
bool dataConnectionInfo(const std::string& id, const DataConnectionInfo& info);
bool closeDataConnection(const std::string& id);

std::string sendFile(const std::string& accountId, const std::string& peerUri, const std::string& filename);
bool fileTransferInfo(const std::string& id, FileTransferInfo& info);
std::size_t fileTransferProgress(const std::string& id);
bool cancelFileTransfer(const std::string& id); // could be used by local or remote (refuse or stop)

// Configuration signal type definitions
struct DataTransferSignal {
    struct DataConnectionStatus {
        constexpr static const char* name = "DataConnectionStatus";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*id*/,
                             DRing::DataConnectionStatus /*status*/,
                             DataTransferError /*error*/);
    };
    struct FileTransferStatus {
        constexpr static const char* name = "FileTransferStatus";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*id*/,
                             DRing::FileTransferStatus /*status*/,
                             DataTransferError /*error*/);
    };
};

} // namespace DRing

#endif // DRING_DATATRANSFERI_H
