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

#include "data_transfer.h"
#include "manager.h"
#include "client/ring_signal.h"

#include <random>

namespace ring {

template<typename... Args>
inline void
emitDataXferStatus(Args... args)
{
    emitSignal<DRing::DataTransferSignal::DataTransferStatus>(args...);
}

static DRing::DataTransferId
get_unique_id()
{
    static std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream accId;
    accId << std::hex << dist(Manager::instance().getRandomEngine());
    return accId.str();
}

//==================================================================================================

DataTransfer::DataTransfer(const DRing::DataConnectionId& connectionId, const std::string& name)
    : id_(get_unique_id())
    , info_()
{
    info_.connectionId = connectionId;
    info_.name = name;
    info_.size = -1;
    info_.code = DRing::DataTransferCode::CODE_UNKNOWN;
}

DataTransfer::~DataTransfer()
{
    if (info_.code > 0 and ((info_.code / 100) < 2 or info_.code == DRing::DataTransferCode::CODE_ACCEPTED))
        emitDataXferStatus(id_, DRing::DataTransferCode::CODE_SERVICE_UNAVAILABLE);
}

void
DataTransfer::setStatus(DRing::DataTransferCode code)
{
    {
        std::lock_guard<std::mutex> lk(infoMutex_);
        info_.code = code;
    }
    emitDataXferStatus(id_, code);
}

//==================================================================================================

FileSender::FileSender(const DRing::DataConnectionId& connectionId, const std::string& name,
                       std::ifstream&& stream)
    : DataTransfer(connectionId, name)
    , stream_(std::move(stream))
{}

} // namespace ring
