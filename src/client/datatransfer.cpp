/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "jamidht/jamiaccount.h"

namespace DRing {

void
registerDataXferHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    registerSignalHandlers(handlers);
}

DataTransferError
sendFile(const DataTransferInfo& info, DataTransferId& id) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(info.accountId)) {
        auto to = info.conversationId;
        if (to.empty())
            to = info.peer;
        id = acc->sendFile(to, info.path);
        if (id != 0)
            return DRing::DataTransferError::success;
    }
    return DRing::DataTransferError::invalid_argument;
}

DataTransferError
acceptFileTransfer(const std::string& accountId,
                   const std::string& conversationId,
                   const DataTransferId& id,
                   const std::string& file_path,
                   int64_t offset) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        return acc->acceptFile(conversationId, id, file_path, offset)
                   ? DRing::DataTransferError::success
                   : DRing::DataTransferError::invalid_argument;
    }
    return DRing::DataTransferError::invalid_argument;
}

DataTransferError
cancelDataTransfer(const std::string& accountId,
                   const std::string& conversationId,
                   const DataTransferId& id) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        return acc->cancel(conversationId, id) ? DRing::DataTransferError::success
                                               : DRing::DataTransferError::invalid_argument;
    }
    return DRing::DataTransferError::invalid_argument;
}

DataTransferError
dataTransferBytesProgress(const std::string& accountId,
                          const std::string& conversationId,
                          const DataTransferId& id,
                          int64_t& total,
                          int64_t& progress) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        return acc->bytesProgress(conversationId, id, total, progress)
                   ? DRing::DataTransferError::success
                   : DRing::DataTransferError::invalid_argument;
    }
    return DRing::DataTransferError::invalid_argument;
}

DataTransferError
dataTransferInfo(const std::string& accountId,
                 const std::string& conversationId,
                 const DataTransferId& id,
                 DataTransferInfo& info) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        return acc->info(conversationId, id, info) ? DRing::DataTransferError::success
                                                   : DRing::DataTransferError::invalid_argument;
    }
    return DRing::DataTransferError::invalid_argument;
}

} // namespace DRing
