/*
 *  Copyright (C) 2017-2018 Savoir-faire Linux Inc.
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
#include <vector>
#include <memory>
#include <cstdlib> // std::size_t
#include <ios> // std::streamsize

namespace DRing {

using DataTransferId = uint64_t;

enum class DataTransferEventCode : uint32_t
{
    created,
    unsupported,
    wait_peer_acceptance,
    wait_host_acceptance,
    ongoing,
    finished,
    closed_by_host,
    closed_by_peer,
    invalid_pathname,
    unjoinable_peer,
};

struct DataTransferInfo
{
    bool isOutgoing; ///< Outgoing or Incoming?
    DataTransferEventCode lastEvent { DataTransferEventCode::created }; ///< Latest event code sent to the user
    std::size_t totalSize {0} ; ///< Total number of bytes to sent/receive, 0 if not known
    std::streamsize bytesProgress {0}; ///< Number of bytes sent/received
    std::string displayName; ///< Human oriented transfer name
    std::string path; ///< associated local file path if supported (empty, if not)
    std::string accountId; ///< Identifier of the emiter/receiver account
    std::string peer; ///< Identifier of the remote peer (in the semantic of the associated account)
};

std::vector<DataTransferId> dataTransferList();

/// Asynchronously send a file to a peer using given account connection.
///
/// If given account supports a file transfer protocol this function creates
/// an internal data transfer and return its identification.
/// This identity code is used by signals and APIs to follow the transfer progress.
///
/// \param account_id existing account ID with file transfer support
/// \param peer_uri peer address suitable for the given account
/// \param file_path pathname of file to transfer
/// \param display_name optional textual representation given to the peer when the file is proposed.
/// When empty (or not given), \a file_path is used.
///
/// \return DataTransferId value representing the internal transfer.
///
/// \exception std::invalid_argument not existing account
/// \exception std::ios_base::failure file opening failures
///
/// \note If the account is valid but doesn't support file transfer, or if the peer is unjoignable,
/// or at events during the transfer, the function returns a valid DataTransferId and the user
/// will be signaled throught DataTransferEvent signal for such event.
/// There is no reserved or special values on DataTransferId type.
///
DataTransferId sendFile(const std::string& account_id, const std::string& peer_uri,
                        const std::string& file_path, const std::string& display_name={});

/// Accept an incoming file transfer.
///
/// Use this function when you receive an incoming transfer request throught DataTransferEvent signal.
/// The data reception and writting will occurs asynchronously.
/// User should listen to DataTransferEvent event to follow the transfer progess.
/// This function can be used only once per data transfer identifiant, when used more it's ignored.
///
/// \param id data transfer identification value as given by a DataTransferEvent signal.
/// \param file_path file path going to be open in binary write mode to put incoming data.
/// \param offset used to indicate the remote side about the number of bytes already received in
/// a previous transfer session, usefull in transfer continuation mode.
///
void acceptFileTransfer(const DataTransferId& id, const std::string& file_path, std::size_t offset);

/// Refuse or abort an outgoing or an incoming file transfer.
///
/// Use this function when you receive an incoming or when you want to abort an outgoing
/// data transfer.
/// The cancellation will occurs asynchronously, a cancel event will be generated when its effective.
/// This function can be used only once per data transfer identifiant, when used more it's ignored.
///
/// \param id data transfer identification value as given by a DataTransferEvent signal.
///
void cancelDataTransfer(const DataTransferId& id);

/// Return some information on given data transfer.
///
/// \param id data transfer identification value as given by a DataTransferEvent signal.
///
/// \return transfer information.
///
DataTransferInfo dataTransferInfo(const DataTransferId& id);

/// Return the amount of sent/received bytes of an existing data transfer.
///
/// \param id data transfer identification value as given by a DataTransferEvent signal.
///
/// \return number of successfuly transfered bytes.
///
std::streamsize dataTransferBytesProgress(const DataTransferId& id);

// Signal handlers registration
void registerDataXferHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

// Signals
struct DataTransferSignal
{
    struct DataTransferEvent
    {
        constexpr static const char* name = "DataTransferEvent";
        using cb_type = void(const DataTransferId& transferId, int eventCode);
    };
};

} // namespace DRing

#endif // DRING_DATATRANSFERI_H
