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

#include "datatransfer_interface.h"

#include "manager.h"
#include "data_transfer.h"
#include "client/ring_signal.h"
#include "jamidht/jamiaccount.h"

namespace libjami {

void
registerDataXferHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    registerSignalHandlers(handlers);
}

void
sendFile(const std::string& accountId,
         const std::string& conversationId,
         const std::string& path,
         const std::string& displayName,
         const std::string& replyTo) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        acc->sendFile(conversationId, path, displayName, replyTo);
    }
}

bool
downloadFile(const std::string& accountId,
             const std::string& conversationId,
             const std::string& interactionId,
             const std::string& fileId,
             const std::string& path) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->downloadFile(conversationId, interactionId, fileId, path);
    return {};
}

DataTransferError
cancelDataTransfer(const std::string& accountId,
                   const std::string& conversationId,
                   const std::string& fileId) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        if (auto dt = acc->dataTransfer(conversationId))
            return dt->cancel(fileId) ? libjami::DataTransferError::success
                                      : libjami::DataTransferError::invalid_argument;
    }
    return libjami::DataTransferError::invalid_argument;
}

DataTransferError
fileTransferInfo(const std::string& accountId,
                 const std::string& conversationId,
                 const std::string& fileId,
                 std::string& path,
                 int64_t& total,
                 int64_t& progress) noexcept
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        if (auto dt = acc->dataTransfer(conversationId))
            return dt->info(fileId, path, total, progress)
                       ? libjami::DataTransferError::success
                       : libjami::DataTransferError::invalid_argument;
    }
    return libjami::DataTransferError::invalid_argument;
}

} // namespace libjami
