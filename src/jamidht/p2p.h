/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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
#include "channeled_transfers.h"

#include <string>
#include <memory>
#include <functional>

namespace jami {

class JamiAccount;
class PeerConnection;

class DhtPeerConnector {
public:
    DhtPeerConnector(JamiAccount& account);
    ~DhtPeerConnector();

    void onDhtConnected(const std::string& device_id);
    void requestConnection(const std::string& peer_id, const DRing::DataTransferId& tid,
                           const std::function<void(PeerConnection*)>& connect_cb,
                           const std::function<void(const std::shared_ptr<ChanneledOutgoingTransfer>&)>& channeledConnectedCb,
                           const std::function<void()>& onChanneledCancelled);
    void closeConnection(const std::string& peer_id, const DRing::DataTransferId& tid);
    bool onIncomingChannelRequest(const DRing::DataTransferId& tid);
    void onIncomingConnection(const std::string& peer_id, const DRing::DataTransferId& tid, const std::shared_ptr<ChannelSocket>& channel);
private:
    DhtPeerConnector() = delete;

    class Impl;
    std::shared_ptr<Impl> pimpl_;
};

}
