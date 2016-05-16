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

#include "datatransfer_interface.h"
#include "client/ring_signal.h"
#include "manager.h"
#include "data_transfer.h"

#include "logger.h"

namespace DRing {

void
registerDataXferHandlers(const std::map<std::string,
                         std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    auto& handlers_ = ring::getSignalHandlers();
    for (auto& item : handlers) {
        auto iter = handlers_.find(item.first);
        if (iter == handlers_.end()) {
            RING_ERR("Signal %s not supported", item.first.c_str());
            continue;
        }

        iter->second = std::move(item.second);
    }
}

DataConnectionId
connectToPeer(const std::string&, const std::string&)
{
    return {};
}

bool
dataConnectionInfo(const DataConnectionId&, const DataConnectionInfo&)
{
    return false;
}

bool
closeDataConnection(const DataConnectionId&)
{
    return false;
}

DataTransferId
sendFile(const std::string& accountID,
         const std::string& peerUri,
         const std::string& pathname,
         const std::string& name)
{
    return ring::Manager::instance().sendFile(accountID, peerUri, pathname, name);
}

bool
dataTransferInfo(const DataTransferId&, DataTransferInfo&)
{
    //if (auto dt = ring::DataTransfer::getDataTransfer(tid)) {
    //    dt->getInfo(info);
    //    return true;
    //}
    return false;
}

std::streamsize
dataTransferSentBytes(const DataTransferId&)
{
    //if (auto dt = ring::DataTransfer::getDataTransfer(tid))
    //    return dt->getCount();
    return 0;
}

bool
cancelDataTransfer(const DataTransferId&)
{
    return false;
}

void
acceptFileTransfer(const DataTransferId&, const std::string&)
{
    //ring::acceptFileTransfer(id, pathname);
}

} // namespace DRing
