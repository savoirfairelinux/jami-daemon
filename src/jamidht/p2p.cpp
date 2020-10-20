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

#include "p2p.h"

#include "account_schema.h"
#include "jamiaccount.h"
#include "channel.h"
#include "ice_transport.h"
#include "ftp_server.h"
#include "manager.h"
#include "peer_connection.h"
#include "turn_transport.h"
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

    ~Impl()
    {
        std::lock_guard<std::mutex> lk(waitForReadyMtx_);
        waitForReadyEndpoints_.clear();
    }

    std::weak_ptr<JamiAccount> account;

    bool hasPublicIp(const ICESDP& sdp)
    {
        for (const auto& cand : sdp.rem_candidates)
            if (cand.type == PJ_ICE_CAND_TYPE_SRFLX)
                return true;
        return false;
    }

    std::map<std::pair<dht::InfoHash, IpAddr>, std::unique_ptr<TlsSocketEndpoint>>
        waitForReadyEndpoints_;
    std::mutex waitForReadyMtx_ {};

    // key: Stored certificate PublicKey id (normaly it's the DeviceId)
    // value: pair of shared_ptr<Certificate> and associated RingId
    std::map<dht::InfoHash, std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::InfoHash>>
        certMap_;
    std::map<IpAddr, dht::InfoHash> connectedPeers_;

    bool validatePeerCertificate(const dht::crypto::Certificate&, dht::InfoHash&);
    void closeConnection(const DRing::DataTransferId& tid);
    void stateChanged(const DRing::DataTransferId& tid, const DRing::DataTransferEventCode& code);

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

    // For Channeled transports
    std::mutex channeledIncomingMtx_;
    std::map<DRing::DataTransferId, std::unique_ptr<ChanneledIncomingTransfer>> channeledIncoming_;
    std::mutex channeledOutgoingMtx_;
    std::map<DRing::DataTransferId, std::vector<std::shared_ptr<ChanneledOutgoingTransfer>>>
        channeledOutgoing_;
    std::mutex incomingTransfersMtx_;
    std::set<DRing::DataTransferId> incomingTransfers_;
};
//==============================================================================

/// Find who is connected by using connection certificate
bool
DhtPeerConnector::Impl::validatePeerCertificate(const dht::crypto::Certificate& cert,
                                                dht::InfoHash& peer_h)
{
    const auto& iter = certMap_.find(cert.getId());
    if (iter != std::cend(certMap_)) {
        if (iter->second.first->getPacked() == cert.getPacked()) {
            peer_h = iter->second.second;
            return true;
        }
    }
    return false;
}

void
DhtPeerConnector::Impl::stateChanged(const DRing::DataTransferId& tid,
                                     const DRing::DataTransferEventCode& code)
{
    if (code == DRing::DataTransferEventCode::finished
        or code == DRing::DataTransferEventCode::closed_by_peer
        or code == DRing::DataTransferEventCode::timeout_expired)
        closeConnection(tid);
}

void
DhtPeerConnector::Impl::closeConnection(const DRing::DataTransferId& tid)
{
    dht::ThreadPool::io().run([w = weak(), tid] {
        auto shared = w.lock();
        if (!shared)
            return;
        // Cancel outgoing files
        {
            std::lock_guard<std::mutex> lk(shared->channeledIncomingMtx_);
            auto it = shared->channeledIncoming_.erase(tid);
        }
        {
            std::lock_guard<std::mutex> lk(shared->channeledOutgoingMtx_);
            shared->channeledOutgoing_.erase(tid);
        }
    });
}

//==============================================================================

DhtPeerConnector::DhtPeerConnector(JamiAccount& account)
    : pimpl_ {std::make_shared<Impl>(account.weak())}
{}

DhtPeerConnector::~DhtPeerConnector() = default;

void
DhtPeerConnector::requestConnection(
    const std::string& peer_id,
    const DRing::DataTransferId& tid,
    bool isVCard,
    const std::function<void(const std::shared_ptr<ChanneledOutgoingTransfer>&)>&
        channeledConnectedCb,
    const std::function<void()>& onChanneledCancelled)
{
    auto acc = pimpl_->account.lock();
    if (!acc)
        return;

    const auto peer_h = dht::InfoHash(peer_id);

    auto channelReadyCb = [this, tid, peer_id, channeledConnectedCb, onChanneledCancelled](
                              const std::shared_ptr<ChannelSocket>& channel) {
        auto shared = pimpl_->account.lock();
        if (!channel) {
            onChanneledCancelled();
            return;
        }
        if (!shared)
            return;
        JAMI_INFO("New file channel for outgoing transfer with id(%lu)", tid);

        auto outgoingFile = std::make_shared<ChanneledOutgoingTransfer>(
            channel,
            [this](const DRing::DataTransferId& id, const DRing::DataTransferEventCode& code) {
                pimpl_->stateChanged(id, code);
            });
        if (!outgoingFile)
            return;
        {
            std::lock_guard<std::mutex> lk(pimpl_->channeledOutgoingMtx_);
            pimpl_->channeledOutgoing_[tid].emplace_back(outgoingFile);
        }

        channel->onShutdown([this, tid, onChanneledCancelled, peer = outgoingFile->peer()]() {
            JAMI_INFO("Channel down for outgoing transfer with id(%lu)", tid);
            onChanneledCancelled();
            dht::ThreadPool::io().run([w = pimpl_->weak(), tid, peer] {
                auto shared = w.lock();
                if (!shared)
                    return;
                // Cancel outgoing files
                {
                    std::lock_guard<std::mutex> lk(shared->channeledOutgoingMtx_);
                    auto outgoingTransfers = shared->channeledOutgoing_.find(tid);
                    if (outgoingTransfers != shared->channeledOutgoing_.end()) {
                        auto& currentTransfers = outgoingTransfers->second;
                        auto it = currentTransfers.begin();
                        while (it != currentTransfers.end()) {
                            auto& transfer = *it;
                            if (transfer && transfer->peer() == peer)
                                it = currentTransfers.erase(it);
                            else
                                ++it;
                        }
                        if (currentTransfers.empty())
                            shared->channeledOutgoing_.erase(outgoingTransfers);
                    }
                }
                Manager::instance().dataTransfers->close(tid);
            });
        });
        channeledConnectedCb(outgoingFile);
    };

    if (isVCard) {
        acc->connectionManager().connectDevice(peer_id,
                                               "vcard://" + std::to_string(tid),
                                               channelReadyCb);
        return;
    }

    // Notes for reader:
    // 1) dht.getPublicAddress() suffers of a non-usability into forEachDevice() callbacks.
    //    If you call it in forEachDevice callbacks, it'll never not return...
    //    Seems that getPublicAddress() and forEachDevice() need to process into the same thread
    //    (here the one where dht_ loop runs).
    // 2) anyway its good to keep this processing here in case of multiple device
    //    as the result is the same for each device.
    auto addresses = acc->publicAddresses();

    acc->forEachDevice(
        peer_h,
        [this, addresses, tid, channelReadyCb = std::move(channelReadyCb)](
            const dht::InfoHash& dev_h) {
            auto acc = pimpl_->account.lock();
            if (!acc)
                return;
            if (dev_h == acc->dht()->getId()) {
                JAMI_ERR() << acc->getAccountID() << "[CNX] no connection to yourself, bad person!";
                return;
            }

            acc->connectionManager().connectDevice(dev_h.toString(),
                                                   "file://" + std::to_string(tid),
                                                   channelReadyCb);
        },

        [peer_h, onChanneledCancelled, accId = acc->getAccountID()](bool found) {
            if (!found) {
                JAMI_WARN() << accId << "[CNX] aborted, no devices for " << peer_h;
                onChanneledCancelled();
            }
        });
}

void
DhtPeerConnector::closeConnection(const DRing::DataTransferId& tid)
{
    pimpl_->closeConnection(tid);
}

bool
DhtPeerConnector::onIncomingChannelRequest(const DRing::DataTransferId& tid)
{
    std::lock_guard<std::mutex> lk(pimpl_->incomingTransfersMtx_);
    if (pimpl_->incomingTransfers_.find(tid) != pimpl_->incomingTransfers_.end()) {
        JAMI_INFO("Incoming transfer request with id(%lu) is already treated via DHT", tid);
        return false;
    }
    pimpl_->incomingTransfers_.emplace(tid);
    JAMI_INFO("Incoming transfer request with id(%lu)", tid);
    return true;
}

void
DhtPeerConnector::onIncomingConnection(const std::string& peer_id,
                                       const DRing::DataTransferId& tid,
                                       const std::shared_ptr<ChannelSocket>& channel,
                                       InternalCompletionCb&& cb)
{
    if (!channel)
        return;
    auto acc = pimpl_->account.lock();
    if (!acc)
        return;
    auto incomingFile = std::make_unique<ChanneledIncomingTransfer>(
        channel,
        std::make_shared<FtpServer>(acc->getAccountID(), peer_id, tid, std::move(cb)),
        [this](const DRing::DataTransferId& id, const DRing::DataTransferEventCode& code) {
            pimpl_->stateChanged(id, code);
        });
    {
        std::lock_guard<std::mutex> lk(pimpl_->channeledIncomingMtx_);
        pimpl_->channeledIncoming_.emplace(tid, std::move(incomingFile));
    }
    channel->onShutdown([this, tid]() {
        JAMI_INFO("Channel down for incoming transfer with id(%lu)", tid);
        dht::ThreadPool::io().run([w = pimpl_->weak(), tid] {
            auto shared = w.lock();
            if (!shared)
                return;
            // Cancel incoming files
            // Note: erasing the channeled transfer will close the file via ftp_->close()
            std::lock_guard<std::mutex> lk(shared->channeledIncomingMtx_);
            shared->channeledIncoming_.erase(tid);
        });
    });
}

} // namespace jami
