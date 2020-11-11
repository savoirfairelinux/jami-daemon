/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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
#include "data_transfer.h"
#include "channeled_transfers.h"

#include <string>
#include <memory>
#include <functional>

namespace jami {

class JamiAccount;

class DhtPeerConnector
{
public:
    DhtPeerConnector(JamiAccount& account);

    void requestConnection(
        const DRing::DataTransferInfo& info,
        const DRing::DataTransferId& tid,
        bool isVCard,
        const std::function<void(const std::shared_ptr<ChanneledOutgoingTransfer>&)>&
            channeledConnectedCb,
        const std::function<void(const std::string&)>& onChanneledCancelled);
    void closeConnection(const DRing::DataTransferId& tid);
    void onIncomingConnection(const DRing::DataTransferInfo& info,
                              const DRing::DataTransferId& id,
                              const std::shared_ptr<ChannelSocket>& channel,
                              const InternalCompletionCb& cb = {});

private:
    DhtPeerConnector() = delete;

    class Impl;
    std::shared_ptr<Impl> pimpl_;
};

} // namespace jami
