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

#pragma once

#include "datatransfer_interface.h"
#include "noncopyable.h"

#include <string>
#include <fstream>
#include <mutex>

namespace ring {

class DataTransfer
{
public:
    DataTransfer(const DRing::DataConnectionId& connectionId, const std::string& name);
    ~DataTransfer();

    DataTransfer(DataTransfer&& o)
        : id_(std::move(o.id_))
        , info_(std::move(o.info_)) {}

    DRing::DataTransferId getId() const noexcept { return id_; }

    void setStatus(DRing::DataTransferCode code);

private:
    NON_COPYABLE(DataTransfer);

    DRing::DataTransferId id_ {}; // unique (in the process scope) data transfer identifier

    std::mutex infoMutex_ {};
    DRing::DataTransferInfo info_ {};
};

class FileSender : public DataTransfer
{
public:
    FileSender(const DRing::DataConnectionId& connectionId, const std::string& name,
               std::ifstream&& stream);

    FileSender(FileSender&& o)
        : DataTransfer(std::move(o))
        , stream_(std::move(o.stream_)) {}

private:
    NON_COPYABLE(FileSender);
    std::ifstream stream_ {};
};

} // namespace ring
