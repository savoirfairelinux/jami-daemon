/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
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
    std::map<DeviceId /* deviceId */, std::vector<std::shared_ptr<dhtnet::ChannelSocket>>> syncConnections_;

    std::weak_ptr<Impl> weak() { return std::static_pointer_cast<Impl>(shared_from_this()); }

    /**
     * Build SyncMsg and send it on socket
     * @param socket
     */
    void syncInfos(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                   const std::shared_ptr<SyncMsg>& syncMsg);
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
    Json::Value syncValue;
    msgpack::sbuffer buffer(UINT16_MAX); // Use max pkt size
    std::error_code ec;
    if (!syncMsg) {
        // Send contacts infos
        // This message can be big. TODO rewrite to only take UINT16_MAX bytes max or split it multiple
        // messages. For now, write 3 messages (UINT16_MAX*3 should be enough for all informations).
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

        auto convModule = acc->convModule();
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
        auto ld = convModule->convDisplayed();
        if (!ld.empty()) {
            SyncMsg msg;
            msg.ld = std::move(ld);
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
SyncModule::cacheSyncConnection(std::shared_ptr<dhtnet::ChannelSocket>&& socket,
                                const std::string& peerId,
                                const DeviceId& device)
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->syncConnectionsMtx_);
    pimpl_->syncConnections_[device].emplace_back(socket);

    socket->onShutdown([w = pimpl_->weak(), peerId, device, socket]() {
        auto shared = w.lock();
        if (!shared)
            return;
        std::lock_guard<std::recursive_mutex> lk(shared->syncConnectionsMtx_);
        auto& connections = shared->syncConnections_[device];
        auto conn = connections.begin();
        while (conn != connections.end()) {
            if (*conn == socket)
                conn = connections.erase(conn);
            else
                conn++;
        }
    });

    socket->setOnRecv([acc = pimpl_->account_.lock(), device, peerId](const uint8_t* buf,
                                                                      size_t len) {
        if (!buf || !acc)
            return len;

        SyncMsg msg;
        try {
            msgpack::unpacker unpacker;
            unpacker.reserve_buffer(len);
            memcpy(unpacker.buffer(), reinterpret_cast<const char*>(buf), len);
            unpacker.buffer_consumed(len);
            msgpack::object_handle oh;
            if (unpacker.next(oh))
                oh.get().convert(msg);
        } catch (const std::exception& e) {
            JAMI_WARNING("[convInfo] error on sync: {:s}", e.what());
            return len;
        }

        if (auto manager = acc->accountManager())
            manager->onSyncData(std::move(msg.ds), false);

        if (!msg.c.empty() || !msg.cr.empty() || !msg.p.empty() || !msg.ld.empty())
            acc->convModule()->onSyncData(msg, peerId, device.toString());
        return len;
    });
}

void
SyncModule::syncWith(const DeviceId& deviceId,
                     const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                     const std::shared_ptr<SyncMsg>& syncMsg)
{
    if (!socket)
        return;
    {
        std::lock_guard<std::recursive_mutex> lk(pimpl_->syncConnectionsMtx_);
        socket->onShutdown([w = pimpl_->weak(), socket, deviceId]() {
            // When sock is shutdown update syncConnections_ to be able to resync asap
            auto shared = w.lock();
            if (!shared)
                return;
            std::lock_guard<std::recursive_mutex> lk(shared->syncConnectionsMtx_);
            auto& connections = shared->syncConnections_[deviceId];
            auto conn = connections.begin();
            while (conn != connections.end()) {
                if (*conn == socket)
                    conn = connections.erase(conn);
                else
                    conn++;
            }
            if (connections.empty()) {
                shared->syncConnections_.erase(deviceId);
            }
        });
        pimpl_->syncConnections_[deviceId].emplace_back(socket);
    }
    pimpl_->syncInfos(socket, syncMsg);
}

void
SyncModule::syncWithConnected(const std::shared_ptr<SyncMsg>& syncMsg, const DeviceId& deviceId)
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->syncConnectionsMtx_);
    for (auto& [did, sockets] : pimpl_->syncConnections_) {
        if (not sockets.empty()) {
            if (!deviceId || deviceId == did) {
                pimpl_->syncInfos(sockets[0], syncMsg);
            }
        }
    }
}
} // namespace jami