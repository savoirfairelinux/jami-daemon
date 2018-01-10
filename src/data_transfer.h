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

#pragma once

#include "dring/datatransfer_interface.h"

#include <memory>
#include <string>

namespace ring {

/// Front-end to data transfer service
class DataTransferFacade
{
public:
    DataTransferFacade();
    ~DataTransferFacade();

    /// Return all known transfer id
    std::vector<DRing::DataTransferId> list() const;

    /// Send a file to a peer.
    /// Open a file and send its contents over a reliable connection
    /// to given peer using the protocol from given account.
    /// This method fails immediately if the file cannot be open in binary read mode,
    /// if the account doesn't exist or if it doesn't support data transfer.
    /// Remaining actions are operated asynchronously, so events are given by signals.
    /// \return a unique data transfer identifier.
    /// \except std::invalid_argument account doesn't exist or don't support data transfer.
    /// \except std::ios_base::failure in case of open file errors.
    DRing::DataTransferId sendFile(const std::string& account_id,
                                   const std::string& peer_uri,
                                   const std::string& file_path,
                                   const std::string& display_name);

    /// Accept an incoming transfer and send data into given file.
    void acceptAsFile(const DRing::DataTransferId& id,
                      const std::string& file_path,
                      std::size_t offset);

    /// Abort a transfer.
    /// The transfer id is abort and removed. The id is not longer valid after the call.
    void cancel(const DRing::DataTransferId& id);

    /// \return a copy of all information about a data transfer
    DRing::DataTransferInfo info(const DRing::DataTransferId& id) const;

    /// \return number of bytes sent by a data transfer
    /// \note this method is fatest than info()
    std::streamsize bytesSent(const DRing::DataTransferId& id) const;

    /// Create an IncomingFileTransfer object.
    /// \return a filename to open where incoming data will be written or an empty string
    ///         in case of refusal.
    std::string onIncomingFileRequest(const std::string& display_name, std::size_t total_size, std::size_t offset);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace ring
