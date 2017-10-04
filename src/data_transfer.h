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

namespace ring {

class DataTransferFacade
{
public:
    DataTransferFacade();
    ~DataTransferFacade();

    DRing::DataTransferId sendFile(const std::string& accountId, const std::string& peerUri,
                                   const std::string& pathname, const std::string& displayName={});

    void acceptAsFile(const DRing::DataTransferId& id, const std::string& pathname);

    void cancel(const DRing::DataTransferId& id);

    std::streamsize bytesSent(const DRing::DataTransferId& id) const;

    DRing::DataTransferInfo info(const DRing::DataTransferId& id) const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace ring
