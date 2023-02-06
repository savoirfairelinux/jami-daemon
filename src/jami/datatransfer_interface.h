/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#ifndef LIBJAMI_DATATRANSFERI_H
#define LIBJAMI_DATATRANSFERI_H

#include "def.h"

#include "jami.h"

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <bitset>

namespace libjami {

[[deprecated("Replaced by registerSignalHandlers")]] LIBJAMI_PUBLIC void registerDataXferHandlers(
    const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

using DataTransferId = uint64_t;

enum class LIBJAMI_PUBLIC DataTransferEventCode : uint32_t {
    invalid = 0,
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
    timeout_expired,
};

enum class LIBJAMI_PUBLIC DataTransferError : uint32_t {
    success = 0,
    unknown,
    io,
    invalid_argument,
};

/// Bit definition for DataTransferInfo.flags field
enum class LIBJAMI_PUBLIC DataTransferFlags {
    direction = 0, ///< 0: outgoing, 1: incoming
};

struct LIBJAMI_PUBLIC DataTransferInfo
{
    std::string accountId; ///< Identifier of the emiter/receiver account
    DataTransferEventCode lastEvent {
        DataTransferEventCode::invalid}; ///< Latest event code sent to the user
    uint32_t flags {0};                  ///< Transfer global information.
    int64_t totalSize {0};               ///< Total number of bytes to sent/receive, 0 if not known
    int64_t bytesProgress {0};           ///< Number of bytes sent/received
    std::string author;
    std::string peer; ///< Identifier of the remote peer (in the semantic of the associated account)
    std::string conversationId;
    std::string displayName; ///< Human oriented transfer name
    std::string path;        ///< associated local file path if supported (empty, if not)
    std::string mimetype;    ///< MimeType of transferred data
                             ///< (https://www.iana.org/assignments/media-types/media-types.xhtml)
};

/// Asynchronously send a file to a peer using given account connection.
///
/// If given account supports a file transfer protocol this function creates
/// an internal data transfer and return its identification.
/// This identity code is used by signals and APIs to follow the transfer progress.
///
/// Following the \a info structure fields usage:
///     - accountId [mandatory] existing account ID with file transfer support
///     - peer [mandatory] peer address suitable for the given account
///     - path [mandatory] pathname of file to transfer
///     - mimetype [optional] file type
///     - displayName [optional] textual representation given to the peer when the file is proposed
///
/// Other fields are not used, but you must keep the default assigned value for compatibility.
///
/// \param info a DataTransferInfo structure filled with information useful for a file transfer.
/// \param[out] id data transfer identifiant if function succeed, usable with other APIs. Undefined
/// value in case of error.
///
/// \return DataTransferError::success if file is accepted for transfer, any other value in case of
/// errors \note If the account is valid but doesn't support file transfer, or if the peer is
/// unjoignable, or at any further events during the transfer, the function returns a valid
/// DataTransferId as the processing is asynchronous. Application will be signaled throught
/// DataTransferEvent signal for such event. There is no reserved or special values on
/// DataTransferId type.
///

LIBJAMI_PUBLIC void sendFile(const std::string& accountId,
                             const std::string& conversationId,
                             const std::string& path,
                             const std::string& displayName,
                             const std::string& replyTo) noexcept;

/// Asks for retransferring a file. Generally this means that the file is missing
/// from the conversation
///
/// \param accountId
/// \param conversationId
/// \param fileId
/// \param path
///
LIBJAMI_PUBLIC bool downloadFile(const std::string& accountId,
                                 const std::string& conversationId,
                                 const std::string& interactionId,
                                 const std::string& fileId,
                                 const std::string& path) noexcept;

/// Refuse or abort an outgoing or an incoming file transfer.
///
/// Use this function when you receive an incoming or when you want to abort an outgoing
/// data transfer.
/// The cancellation will occurs asynchronously, a cancel event will be generated when its effective.
/// This function can be used only once per data transfer identifiant, when used more it's ignored.
///
/// \param id data transfer identification value as given by a DataTransferEvent signal.
///
/// \return DataTransferError::invalid_argument if id is unknown.
/// \note unknown \a id results to a no-op call.
///
DataTransferError cancelDataTransfer(const std::string& accountId,
                                     const std::string& conversationId,
                                     const std::string& fileId) noexcept LIBJAMI_PUBLIC;

/// Return the amount of sent/received bytes of an existing data transfer.
///
/// \param id data transfer identification value as given by a DataTransferEvent signal.
/// \param[out] total positive number of bytes to sent/received, or -1 if unknown.
/// \param[out] progress positive number of bytes already sent/received.
///
/// \return DataTransferError::success if \a total and \a progress is set with valid values.
/// DataTransferError::invalid_argument if the id is unknown.
///
LIBJAMI_PUBLIC DataTransferError fileTransferInfo(const std::string& accountId,
                                                  const std::string& conversationId,
                                                  const std::string& fileId,
                                                  std::string& path,
                                                  int64_t& total,
                                                  int64_t& progress) noexcept;

// Signals
struct LIBJAMI_PUBLIC DataTransferSignal
{
    struct LIBJAMI_PUBLIC DataTransferEvent
    {
        constexpr static const char* name = "DataTransferEvent";
        using cb_type = void(const std::string& accountId,
                             const std::string& conversationId,
                             const std::string& interactionId,
                             const std::string& fileId,
                             int eventCode);
    };
};

} // namespace libjami

#endif // LIBJAMI_DATATRANSFERI_H
