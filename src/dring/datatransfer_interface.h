/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
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
#include <map>
#include <memory>
#include <cstdlib> // std::size_t
#include <ios> // std::streamsize

namespace DRing {

using DataTransferId = uint64_t;

enum class DataTransferEventCode : int {
    UNSUPPORTED=0,
    WAIT_PEER_ACCEPTANCE,
    WAIT_HOST_ACCEPTANCE,
    FINISHED,
    CLOSED_BY_HOST,
    CLOSED_BY_PEER,
    INVALID_PATHNAME,
    UNJOINABLE_PEER,
};

struct DataTransferInfo {
    bool isOutgoing; ///< Outgoing or Incoming?
    DataTransferEventCode lastEvent; ///< Latest event code sent to the user
    std::string displayName; ///< Human oriented transfer name
    std::size_t totalSize; ///< Total number of bytes to sent/receive, 0 if not known
    std::streamsize bytesProgress; ///< Number of bytes sent/received
    std::string path; ///< associated local file path if supported (empty, if not)
};

///
/// Asynchronously send a file to a peer using given account connection.
/// If given account supports file transfer protocol this function creates
/// a data transfer object and return its identification.
/// This last is used in signals and API to follow the transfer progress.
///
/// \param[in] accountId account identification supporting the file transfer
/// \param[in] peerUri peer address suitable for selected account
/// \param[in] pathname a full qualified path on the file to transfer
/// \param[in] displayName an optional string representation given to peer when the file is proposed.
///            when empty or not given, the last component of the pathname string is used.
///
/// \return a DataTransferId value representing the transfer object.
///
/// \exception std::invalid_argument when arguments are empty strings or account doesn't exist.
///
/// \note If the account is valid but doesn't support file transfer, or if the peer is unjoignable,
///       or if the file cannot be open for reading, or any events during the transfer,
///       the function returns a valid DataTransferId and the user is signaled throught
///       DataTransferEvent signal for such event. There is no "bad value" with DataTransferId.
///
DataTransferId sendFile(const std::string& accountId, const std::string& peerUri,
                        const std::string& pathname, const std::string& displayName={});

///
/// Accept an incoming file transfer.
/// Use this function when you receive an incoming transfer request throught DataTransferEvent signal.
/// The data reception and writting will occurs asynchronously.
/// User should listen to DataTransferEvent event to follow the transfer progess.
/// This function can be used only once per data transfer identifiant, when used more it's ignored.
///
/// \param[in] id data transfer identification value as given by a DataTransferEvent signal.
/// \param[in] pathname a full qualified path going to be open in binary write mode to put incoming data.
///
void acceptFileTransfer(const DataTransferId& id, const std::string& pathname);

///
/// Refuse or abort an outgoing or an incoming file transfer.
/// Use this function when you receive an incoming or when you want to abort an outgoing
/// data transfer.
/// The cancellation will occurs asynchronously, a cancel event will be generated when its effective.
/// This function can be used only once per data transfer identifiant, when used more it's ignored.
///
/// \param[in] id data transfer identification value as given by a DataTransferEvent signal.
///
void cancelDataTransfer(const DataTransferId& id);

///
/// Return some information on an active data transfer.
///
/// \param[in] id data transfer identification value as given by a DataTransferEvent signal.
///
/// \return transfer information.
///
DataTransferInfo dataTransferInfo(const DataTransferId& id);

///
/// Return the transfer amount of bytes of an existing data transfer.
///
/// \param[in] id data transfer identification value as given by a DataTransferEvent signal.
///
/// \return number of successfuly transfered bytes.
///
std::streamsize dataTransferSentBytes(const DataTransferId& id);

// Signal handlers registration
void registerDataXferHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

// Signals
struct DataTransferSignal {
    struct DataTransferEvent {
        constexpr static const char* name = "DataTransferEvent";
        using cb_type = void(const DataTransferId& transferId, int eventCode);
    };
};

} // namespace DRing

#endif // DRING_DATATRANSFERI_H
