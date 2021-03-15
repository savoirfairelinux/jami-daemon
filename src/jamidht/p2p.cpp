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

#include "p2p.h"

#include "account_schema.h"
#include "jamiaccount.h"
#include "ice_transport.h"
#include "ftp_server.h"
#include "manager.h"
#include "peer_connection.h"
#include "account_manager.h"
#include "multiplexed_socket.h"
#include "connectionmanager.h"
#include "fileutils.h"

#include <opendht/default_types.h>
#include <opendht/rng.h>
#include <opendht/thread_pool.h>

#include <memory>
#include <map>
#include <vector>
#include <chrono>
#include <array>
#include <future>
#include <algorithm>
#include <type_traits>

namespace jami {

static constexpr std::chrono::seconds DHT_MSG_TIMEOUT {30};
static constexpr std::chrono::seconds NET_CONNECTION_TIMEOUT {10};
static constexpr std::chrono::seconds SOCK_TIMEOUT {10};
static constexpr std::chrono::seconds ICE_READY_TIMEOUT {10};
static constexpr std::chrono::seconds ICE_INIT_TIMEOUT {10};
static constexpr std::chrono::seconds ICE_NEGOTIATION_TIMEOUT {10};

using Clock = std::chrono::system_clock;
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;

//==============================================================================

class DhtPeerConnector::Impl : public std::enable_shared_from_this<DhtPeerConnector::Impl>
{
public:
    class ClientConnector;

    explicit Impl(const std::weak_ptr<JamiAccount>& account)
        : account {account}
    {}

    std::weak_ptr<JamiAccount> account;

    void closeConnection(const DRing::DataTransferId& tid, const std::string& peer = "");
    void stateChanged(const DRing::DataTransferId& tid,
                      const DRing::DataTransferEventCode& code,
                      const std::string& peer);

    std::shared_ptr<DhtPeerConnector::Impl> shared()
    {
        return std::static_pointer_cast<DhtPeerConnector::Impl>(shared_from_this());
    }
    std::shared_ptr<DhtPeerConnector::Impl const> shared() const
    {
        return std::static_pointer_cast<DhtPeerConnector::Impl const>(shared_from_this());
    }
    std::weak_ptr<DhtPeerConnector::Impl> weak()
    {
        return std::static_pointer_cast<DhtPeerConnector::Impl>(shared_from_this());
    }
    std::weak_ptr<DhtPeerConnector::Impl const> weak() const
    {
        return std::static_pointer_cast<DhtPeerConnector::Impl const>(shared_from_this());
    }

    void removeIncoming(const DRing::DataTransferId& tid, const std::string& peer)
    {
        std::vector<std::unique_ptr<ChanneledIncomingTransfer>> ifiles;
        {
            std::lock_guard<std::mutex> lk(channeledIncomingMtx_);
            auto it = channeledIncoming_.find(tid);
            if (it != channeledIncoming_.end()) {
                for (auto chanIt = it->second.begin(); chanIt != it->second.end();) {
                    if ((*chanIt)->peer() == peer) {
                        ifiles.emplace_back(std::move(*chanIt));
                        chanIt = it->second.erase(chanIt);
                    } else {
                        ++chanIt;
                    }
                }
                if (it->second.empty())
                    channeledIncoming_.erase(it);
            }
        }
    }

    void removeOutgoing(const DRing::DataTransferId& tid, const std::string& peer)
    {
        std::vector<std::shared_ptr<ChanneledOutgoingTransfer>> ofiles;
        {
            std::lock_guard<std::mutex> lk(channeledOutgoingMtx_);
            auto it = channeledOutgoing_.find(tid);
            if (it != channeledOutgoing_.end()) {
                for (auto chanIt = it->second.begin(); chanIt != it->second.end();) {
                    if ((*chanIt)->peer() == peer) {
                        ofiles.emplace_back(std::move(*chanIt));
                        chanIt = it->second.erase(chanIt);
                    } else {
                        ++chanIt;
                    }
                }
                if (it->second.empty())
                    channeledOutgoing_.erase(it);
            }
        }
    }

    // For Channeled transports
    std::mutex channeledIncomingMtx_;
    std::map<DRing::DataTransferId, std::vector<std::unique_ptr<ChanneledIncomingTransfer>>>
        channeledIncoming_;
    std::mutex channeledOutgoingMtx_;
    // TODO change <<id, peer>, Channeled>
    std::map<DRing::DataTransferId, std::vector<std::shared_ptr<ChanneledOutgoingTransfer>>>
        channeledOutgoing_;
};
//==============================================================================

void
DhtPeerConnector::Impl::stateChanged(const DRing::DataTransferId& tid,
                                     const DRing::DataTransferEventCode& code,
                                     const std::string& peer)
{
    if (code == DRing::DataTransferEventCode::finished
        or code == DRing::DataTransferEventCode::closed_by_peer
        or code == DRing::DataTransferEventCode::timeout_expired)
        closeConnection(tid, peer);
}

void
DhtPeerConnector::Impl::closeConnection(const DRing::DataTransferId& tid, const std::string& peer)
{
    dht::ThreadPool::io().run([w = weak(), tid, peer] {
        auto shared = w.lock();
        if (!shared)
            return;
        shared->removeIncoming(tid, peer);
        shared->removeOutgoing(tid, peer);
    });
}

//==============================================================================

DhtPeerConnector::DhtPeerConnector(JamiAccount& account)
    : pimpl_ {std::make_shared<Impl>(account.weak())}
{}

void
DhtPeerConnector::requestConnection(
    const DRing::DataTransferInfo& info,
    const DRing::DataTransferId& tid,
    bool isVCard,
    const std::function<void(const std::shared_ptr<ChanneledOutgoingTransfer>&)>&
        channeledConnectedCb,
    const std::function<void(const std::string&)>& onChanneledCancelled)
{
    auto acc = pimpl_->account.lock();
    if (!acc)
        return;

    auto channelReadyCb = [this,
                           tid,
                           channeledConnectedCb,
                           onChanneledCancelled](const std::shared_ptr<ChannelSocket>& channel,
                                                 const DeviceId& deviceId) {
        auto shared = pimpl_->account.lock();
        if (!channel) {
            onChanneledCancelled(deviceId.toString());
            return;
        }
        if (!shared)
            return;
        JAMI_INFO("New file channel for outgoing transfer with id(%lu)", tid);

        auto outgoingFile = std::make_shared<ChanneledOutgoingTransfer>(
            channel,
            [this, deviceId](const DRing::DataTransferId& id,
                             const DRing::DataTransferEventCode& code) {
                pimpl_->stateChanged(id, code, deviceId.toString());
            });
        if (!outgoingFile)
            return;
        {
            std::lock_guard<std::mutex> lk(pimpl_->channeledOutgoingMtx_);
            pimpl_->channeledOutgoing_[tid].emplace_back(outgoingFile);
        }

        channel->onShutdown([this, tid, onChanneledCancelled, peer = outgoingFile->peer()]() {
            JAMI_INFO("Channel down for outgoing transfer with id(%lu)", tid);
            onChanneledCancelled(peer);
            dht::ThreadPool::io().run([w = pimpl_->weak(), tid, peer] {
                if (auto shared = w.lock())
                    shared->removeOutgoing(tid, peer);
            });
        });
        channeledConnectedCb(outgoingFile);
    };

    if (isVCard) {
        acc->connectionManager().connectDevice(DeviceId(info.peer),
                                               "vcard://" + std::to_string(tid),
                                               channelReadyCb);
        return;
    }

    std::string channelName = "file://" + std::to_string(tid);
    std::vector<DeviceId> devices;
    if (!info.conversationId.empty()) {
        // TODO remove preSwarmCompat
        // In a one_to_one conv with an old version, the contact here can be in an invited
        // state and will not support data-transfer. So if one_to_oe with non accepted, just
        // force to file:// for now.
        auto members = acc->getConversationMembers(info.conversationId);
        auto preSwarmCompat = members.size() == 2 && members[1]["role"] == "invited";
        if (preSwarmCompat) {
            auto infos = acc->conversationInfos(info.conversationId);
            preSwarmCompat = infos["mode"] == "0";
        }
        for (const auto& member : members) {
            devices.emplace_back(DeviceId(member.at("uri")));
        }
        if (!preSwarmCompat)
            channelName = "data-transfer://" + info.conversationId + "/" + acc->currentDeviceId()
                          + "/" + std::to_string(tid);
        // If peer is not empty this means that we want to send to one device only
        if (!info.peer.empty()) {
            acc->connectionManager().connectDevice(DeviceId(info.peer), channelName, channelReadyCb);
            return;
        }
    } else {
        devices.emplace_back(DeviceId(info.peer));
    }

    for (const auto& peer_h : devices) {
        acc->forEachDevice(
            peer_h,
            [this, channelName, tid, channelReadyCb = std::move(channelReadyCb)](
                const dht::InfoHash& dev_h) {
                auto acc = pimpl_->account.lock();
                if (!acc)
                    return;
                if (dev_h == acc->dht()->getId()) {
                    // No connection to same device
                    return;
                }

                acc->connectionManager().connectDevice(dev_h, channelName, channelReadyCb);
            },

            [peer_h, onChanneledCancelled, accId = acc->getAccountID()](bool found) {
                if (!found) {
                    JAMI_WARN() << accId << "[CNX] aborted, no devices for " << peer_h;
                    onChanneledCancelled(peer_h.toString());
                }
            });
    }
}

void
DhtPeerConnector::closeConnection(const DRing::DataTransferId& tid)
{
    pimpl_->closeConnection(tid);
}

void
DhtPeerConnector::onIncomingConnection(const DRing::DataTransferInfo& info,
                                       const DRing::DataTransferId& id,
                                       const std::shared_ptr<ChannelSocket>& channel,
                                       const InternalCompletionCb& cb)
{
    if (!channel)
        return;
    auto peer_id = info.peer;
    auto incomingFile = std::make_unique<ChanneledIncomingTransfer>(
        channel,
        std::make_shared<FtpServer>(info, id, std::move(cb)),
        [this, peer_id](const DRing::DataTransferId& id, const DRing::DataTransferEventCode& code) {
            pimpl_->stateChanged(id, code, peer_id);
        });
    {
        std::lock_guard<std::mutex> lk(pimpl_->channeledIncomingMtx_);
        pimpl_->channeledIncoming_[id].emplace_back(std::move(incomingFile));
    }
    channel->onShutdown([this, id, peer_id]() {
        JAMI_INFO("Channel down for incoming transfer with id(%lu)", id);
        dht::ThreadPool::io().run([w = pimpl_->weak(), id, peer_id] {
            if (auto shared = w.lock())
                shared->removeIncoming(id, peer_id);
        });
    });
}

} // namespace jami
