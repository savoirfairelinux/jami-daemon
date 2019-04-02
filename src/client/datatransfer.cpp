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

#include "datatransfer_interface.h"

#include "manager.h"
#include "data_transfer.h"
#include "client/ring_signal.h"

namespace DRing {

void
registerDataXferHandlers(const std::map<std::string,
    std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    registerSignalHandlers(handlers);
}

std::vector<DataTransferId>
dataTransferList() noexcept
{
    return jami::Manager::instance().dataTransfers->list();
}

DataTransferError
sendFile(const DataTransferInfo& info, DataTransferId& id) noexcept
{
    return jami::Manager::instance().dataTransfers->sendFile(info, id);
}

DataTransferError
acceptFileTransfer(const DataTransferId& id, const std::string& file_path, int64_t offset) noexcept
{
    return jami::Manager::instance().dataTransfers->acceptAsFile(id, file_path, offset);
}

DataTransferError
cancelDataTransfer(const DataTransferId& id) noexcept
{
    return jami::Manager::instance().dataTransfers->cancel(id);
}

DataTransferError
dataTransferBytesProgress(const DataTransferId& id, int64_t& total, int64_t& progress) noexcept
{
    return jami::Manager::instance().dataTransfers->bytesProgress(id, total, progress);
}

DataTransferError
dataTransferInfo(const DataTransferId& id, DataTransferInfo& info) noexcept
{
    return jami::Manager::instance().dataTransfers->info(id, info);
}

} // namespace DRing
