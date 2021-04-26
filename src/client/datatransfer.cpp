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

uint64_t
downloadFile(const std::string& accountId,
             const std::string& conversationUri,
             const std::string& interactionId,
             const std::string& path) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->downloadFile(conversationUri, interactionId, path);
    return {};
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

} // namespace DRing
