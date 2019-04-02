/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

namespace jami {

class Stream;

/// Front-end to data transfer service
class DataTransferFacade
{
public:
    DataTransferFacade();
    ~DataTransferFacade();

    /// \see DRing::dataTransferList
    std::vector<DRing::DataTransferId> list() const noexcept;

    /// \see DRing::sendFile
    DRing::DataTransferError sendFile(const DRing::DataTransferInfo& info,
                                      DRing::DataTransferId& id) noexcept;

    /// \see DRing::acceptFileTransfer
    DRing::DataTransferError acceptAsFile(const DRing::DataTransferId& id,
                                          const std::string& file_path,
                                          int64_t offset) noexcept;

    /// \see DRing::cancelDataTransfer
    DRing::DataTransferError cancel(const DRing::DataTransferId& id) noexcept;

    /// \see DRing::dataTransferInfo
    DRing::DataTransferError info(const DRing::DataTransferId& id,
                                  DRing::DataTransferInfo& info) const noexcept;

    /// \see DRing::dataTransferBytesProgress
    DRing::DataTransferError bytesProgress(const DRing::DataTransferId& id, int64_t& total,
                                           int64_t& progress) const noexcept;

    /// Create an IncomingFileTransfer object.
    /// \return a shared pointer on created Stream object, or nullptr in case of error
    std::shared_ptr<Stream> onIncomingFileRequest(const DRing::DataTransferInfo& info);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
