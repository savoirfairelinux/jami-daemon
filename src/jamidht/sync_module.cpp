/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "sync_module.h"

#include "jamidht/conversation_module.h"
#include "jamidht/archive_account_manager.h"
#include <dhtnet/multiplexed_socket.h>

namespace jami {

class SyncModule::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::weak_ptr<JamiAccount>&& account);

    std::weak_ptr<JamiAccount> account_;

    // Sync connections
    std::recursive_mutex syncConnectionsMtx_;
    std::map<DeviceId /* deviceId */, std::vector<std::shared_ptr<dhtnet::ChannelSocket>>>
        syncConnections_;

    /**
     * Build SyncMsg and send it on socket
     * @param socket
     */
    void syncInfos(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                   const std::shared_ptr<SyncMsg>& syncMsg);
    void onChannelShutdown(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                           const DeviceId& device);
};

SyncModule::Impl::Impl(std::weak_ptr<JamiAccount>&& account)
    : account_(account)
{}

void
SyncModule::Impl::syncInfos(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                            const std::shared_ptr<SyncMsg>& syncMsg)
{
    auto acc = account_.lock();
    if (!acc)
        return;
    msgpack::sbuffer buffer(UINT16_MAX); // Use max pkt size
    std::error_code ec;
    if (!syncMsg) {
        // Send contacts infos
        // This message can be big. TODO rewrite to only take UINT16_MAX bytes max or split it multiple
        // messages. For now, write 3 messages (UINT16_MAX*3 should be enough for all information).
        if (auto info = acc->accountManager()->getInfo()) {
            if (info->contacts) {
                SyncMsg msg;
                msg.ds = info->contacts->getSyncData();
                msgpack::pack(buffer, msg);
                socket->write(reinterpret_cast<const unsigned char*>(buffer.data()),
                              buffer.size(),
                              ec);
                if (ec) {
                    JAMI_ERROR("{:s}", ec.message());
                    return;
                }
            }
        }
        buffer.clear();
        // Sync conversations
        auto c = ConversationModule::convInfos(acc->getAccountID());
        if (!c.empty()) {
            SyncMsg msg;
            msg.c = std::move(c);
            msgpack::pack(buffer, msg);
            socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
            if (ec) {
                JAMI_ERROR("{:s}", ec.message());
                return;
            }
        }
        buffer.clear();
        // Sync requests
        auto cr = ConversationModule::convRequests(acc->getAccountID());
        if (!cr.empty()) {
            SyncMsg msg;
            msg.cr = std::move(cr);
            msgpack::pack(buffer, msg);
            socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
            if (ec) {
                JAMI_ERROR("{:s}", ec.message());
                return;
            }
        }

        auto convModule = acc->convModule(true);
        if (!convModule)
            return;
        // Sync conversation's preferences
        auto p = convModule->convPreferences();
        if (!p.empty()) {
            SyncMsg msg;
            msg.p = std::move(p);
            msgpack::pack(buffer, msg);
            socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
            if (ec) {
                JAMI_ERROR("{:s}", ec.message());
                return;
            }
        }
        buffer.clear();
        // Sync read's status
        auto ms = convModule->convMessageStatus();
        if (!ms.empty()) {
            SyncMsg msg;
            msg.ms = std::move(ms);
            msgpack::pack(buffer, msg);
            socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
            if (ec) {
                JAMI_ERROR("{:s}", ec.message());
                return;
            }
        }
        buffer.clear();

    } else {
        msgpack::pack(buffer, *syncMsg);
        socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
        if (ec)
            JAMI_ERROR("{:s}", ec.message());
    }
}

////////////////////////////////////////////////////////////////

SyncModule::SyncModule(std::weak_ptr<JamiAccount>&& account)
    : pimpl_ {std::make_shared<Impl>(std::move(account))}
{}

void
SyncModule::Impl::onChannelShutdown(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                                    const DeviceId& device)
{
    std::lock_guard lk(syncConnectionsMtx_);
    auto connectionsIt = syncConnections_.find(device);
    if (connectionsIt == syncConnections_.end())
        return;
    auto& connections = connectionsIt->second;
    auto conn = std::find(connections.begin(), connections.end(), socket);
    if (conn != connections.end())
        connections.erase(conn);
    if (connections.empty())
        syncConnections_.erase(connectionsIt);
}

void
SyncModule::cacheSyncConnection(std::shared_ptr<dhtnet::ChannelSocket>&& socket,
                                const std::string& peerId,
                                const DeviceId& device)
{
    std::lock_guard lk(pimpl_->syncConnectionsMtx_);
    pimpl_->syncConnections_[device].emplace_back(socket);

    socket->onShutdown([w = pimpl_->weak_from_this(), device, s = std::weak_ptr(socket)]() {
        if (auto shared = w.lock())
            shared->onChannelShutdown(s.lock(), device);
    });

    struct DecodingContext
    {
        msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) { return true; },
                               nullptr,
                               512};
    };

    socket->setOnRecv([acc = pimpl_->account_.lock(),
                       device,
                       peerId,
                       ctx = std::make_shared<DecodingContext>()](const uint8_t* buf, size_t len) {
        if (!buf || !acc)
            return len;

        ctx->pac.reserve_buffer(len);
        std::copy_n(buf, len, ctx->pac.buffer());
        ctx->pac.buffer_consumed(len);

        msgpack::object_handle oh;
        try {
            while (ctx->pac.next(oh)) {
                SyncMsg msg;
                oh.get().convert(msg);
                if (auto manager = acc->accountManager())
                    manager->onSyncData(std::move(msg.ds), false);

                if (!msg.c.empty() || !msg.cr.empty() || !msg.p.empty() || !msg.ld.empty()
                    || !msg.ms.empty())
                    if (auto cm = acc->convModule(true))
                        cm->onSyncData(msg, peerId, device.toString());
            }
        } catch (const std::exception& e) {
            JAMI_WARNING("[convInfo] error on sync: {:s}", e.what());
        }

        return len;
    });

    pimpl_->syncInfos(socket, nullptr);
}

bool
SyncModule::isConnected(const DeviceId& deviceId) const
{
    std::lock_guard lk(pimpl_->syncConnectionsMtx_);
    auto it = pimpl_->syncConnections_.find(deviceId);
    if (it == pimpl_->syncConnections_.end())
        return false;
    return !it->second.empty();
}

void
SyncModule::syncWithConnected(const std::shared_ptr<SyncMsg>& syncMsg, const DeviceId& deviceId)
{
    std::lock_guard lk(pimpl_->syncConnectionsMtx_);
    for (auto& [did, sockets] : pimpl_->syncConnections_) {
        if (not sockets.empty()) {
            if (!deviceId || deviceId == did) {
                pimpl_->syncInfos(sockets[0], syncMsg);
            }
        }
    }
}
} // namespace jami
