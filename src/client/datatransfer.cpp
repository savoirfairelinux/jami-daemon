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

#include "datatransfer_interface.h"

#include "manager.h"
#include "data_transfer.h"
#include "client/ring_signal.h"

namespace DRing {

void
registerDataXferHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    auto& handlers_ = ring::getSignalHandlers();
    for (const auto& item : handlers) {
        auto iter = handlers_.find(item.first);
        if (iter == handlers_.end()) {
            RING_ERR("Signal %s not supported", item.first.c_str());
            continue;
        }

        iter->second = std::move(item.second);
    }
}

DataTransferId
sendFile(const std::string& account_id,
         const std::string& peer_uri,
         const std::string& file_path,
         const std::string& display_name)
{
    return ring::Manager::instance().dataTransfers->sendFile(
        account_id, peer_uri, file_path, display_name.empty() ? display_name : file_path);
}

void
acceptFileTransfer(const DataTransferId& id,
                   const std::string& file_path,
                   std::size_t offset)
{
    ring::Manager::instance().dataTransfers->acceptAsFile(id, file_path, offset);
}

void
cancelDataTransfer(const DataTransferId& id)
{
    ring::Manager::instance().dataTransfers->cancel(id);
}

std::streamsize
dataTransferBytesSent(const DataTransferId& id)
{
    return ring::Manager::instance().dataTransfers->bytesSent(id);
}

DataTransferInfo
dataTransferInfo(const DataTransferId& id)
{
    return ring::Manager::instance().dataTransfers->info(id);
}

} // namespace DRing
