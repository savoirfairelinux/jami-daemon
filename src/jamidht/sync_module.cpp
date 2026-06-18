/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
#include "fileutils.h"

#include <dhtnet/multiplexed_socket.h>
#include <dhtnet/channel_utils.h>
#include <opendht/thread_pool.h>

#include <fstream>

namespace jami {

class SyncModule::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(const std::shared_ptr<JamiAccount>& account);

    std::weak_ptr<JamiAccount> account_;
    const std::string accountId_;

    // Sync connections
    std::recursive_mutex syncConnectionsMtx_;
    std::map<DeviceId /* deviceId */, std::vector<std::shared_ptr<dhtnet::ChannelSocket>>> syncConnections_;

    // Local sync-version tracking (never transmitted, see header). Used to
    // decide whether a sync connection must be (re)established with a device.
    std::filesystem::path versionPath_;
    mutable std::mutex versionMtx_;
    uint64_t localVersion_ {0};
    std::map<DeviceId, uint64_t> lastSynced_;

    void loadVersions();
    void saveVersions(); // versionMtx_ must be held
    uint64_t bumpVersion();
    uint64_t currentVersion() const;
    bool needsSync(const DeviceId& deviceId) const;
    void markSynced(const DeviceId& deviceId, uint64_t version);

    /**
     * Build SyncMsg and send it on socket
     * @param socket
     * @return true if the whole state was written without error
     */
    bool syncInfos(const std::shared_ptr<dhtnet::ChannelSocket>& socket, const std::shared_ptr<SyncMsg>& syncMsg);
    void onChannelShutdown(const std::shared_ptr<dhtnet::ChannelSocket>& socket, const DeviceId& device);
};

namespace {
// On-disk representation of the local sync-version state.
struct SyncVersionData
{
    uint64_t version {0};
    std::map<DeviceId, uint64_t> synced;
    MSGPACK_DEFINE_MAP(version, synced)
};
} // namespace

SyncModule::Impl::Impl(const std::shared_ptr<JamiAccount>& account)
    : account_(account)
    , accountId_ {account->getAccountID()}
{
    versionPath_ = account->getPath() / "syncVersions";
    loadVersions();
}

void
SyncModule::Impl::loadVersions()
{
    try {
        auto file = fileutils::loadFile(versionPath_);
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
        SyncVersionData data;
        oh.get().convert(data);
        std::lock_guard lk(versionMtx_);
        localVersion_ = data.version;
        lastSynced_ = std::move(data.synced);
    } catch (const std::exception&) {
        // No (or unreadable) file yet: start fresh. Every known device will be
        // considered out-of-date and synced once on first contact.
    }
}

void
SyncModule::Impl::saveVersions()
{
    // versionMtx_ must be held
    try {
        std::ofstream file(versionPath_, std::ios::trunc | std::ios::binary);
        SyncVersionData data;
        data.version = localVersion_;
        data.synced = lastSynced_;
        msgpack::pack(file, data);
    } catch (const std::exception& e) {
        JAMI_WARNING("[Account {}] Unable to save sync versions: {:s}", accountId_, e.what());
    }
}

uint64_t
SyncModule::Impl::bumpVersion()
{
    std::lock_guard lk(versionMtx_);
    ++localVersion_;
    saveVersions();
    return localVersion_;
}

uint64_t
SyncModule::Impl::currentVersion() const
{
    std::lock_guard lk(versionMtx_);
    return localVersion_;
}

bool
SyncModule::Impl::needsSync(const DeviceId& deviceId) const
{
    std::lock_guard lk(versionMtx_);
    auto it = lastSynced_.find(deviceId);
    // Never synced, or synced at an older version than the current one.
    return it == lastSynced_.end() || it->second < localVersion_;
}

void
SyncModule::Impl::markSynced(const DeviceId& deviceId, uint64_t version)
{
    std::lock_guard lk(versionMtx_);
    auto& synced = lastSynced_[deviceId];
    if (synced < version) {
        synced = version;
        saveVersions();
    }
}

bool
SyncModule::Impl::syncInfos(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                            const std::shared_ptr<SyncMsg>& syncMsg)
{
    auto acc = account_.lock();
    if (!acc)
        return false;
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
                socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
                if (ec) {
                    JAMI_ERROR("[Account {}] [device {}] {:s}", accountId_, socket->deviceId(), ec.message());
                    return false;
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
                JAMI_ERROR("[Account {}] [device {}] {:s}", accountId_, socket->deviceId(), ec.message());
                return false;
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
                JAMI_ERROR("[Account {}] [device {}] {:s}", accountId_, socket->deviceId(), ec.message());
                return false;
            }
        }
        buffer.clear();
        auto convModule = acc->convModule(true);
        if (!convModule)
            return false;
        // Sync conversation's preferences
        auto p = convModule->convPreferences();
        if (!p.empty()) {
            SyncMsg msg;
            msg.p = std::move(p);
            msgpack::pack(buffer, msg);
            socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
            if (ec) {
                JAMI_ERROR("[Account {}] [device {}] {:s}", accountId_, socket->deviceId(), ec.message());
                return false;
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
                JAMI_ERROR("[Account {}] [device {}] {:s}", accountId_, socket->deviceId(), ec.message());
                return false;
            }
        }
        buffer.clear();

    } else {
        msgpack::pack(buffer, *syncMsg);
        socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
        if (ec) {
            JAMI_ERROR("[Account {}] [device {}] {:s}", accountId_, socket->deviceId(), ec.message());
            return false;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////

SyncModule::SyncModule(const std::shared_ptr<JamiAccount>& account)
    : pimpl_ {std::make_shared<Impl>(account)}
{}

void
SyncModule::Impl::onChannelShutdown(const std::shared_ptr<dhtnet::ChannelSocket>& socket, const DeviceId& device)
{
    std::lock_guard lk(syncConnectionsMtx_);
    auto connectionsIt = syncConnections_.find(device);
    if (connectionsIt == syncConnections_.end()) {
        JAMI_WARNING("[Account {}] [device {}] onChannelShutdown: no connection found.", accountId_, device.to_view());
        return;
    }
    auto& connections = connectionsIt->second;
    auto conn = std::find(connections.begin(), connections.end(), socket);
    if (conn != connections.end())
        connections.erase(conn);
    JAMI_LOG("[Account {}] [device {}] removed connection, remaining: {:d}",
             accountId_,
             device.to_view(),
             connections.size());
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

    socket->setOnRecv(dhtnet::buildMsgpackReader<SyncMsg>([acc = pimpl_->account_, device, peerId](SyncMsg&& msg) {
        auto account = acc.lock();
        if (!account)
            return std::make_error_code(std::errc::operation_canceled);

        try {
            if (auto manager = account->accountManager())
                manager->onSyncData(std::move(msg.ds), false);

            if (!msg.c.empty() || !msg.cr.empty() || !msg.p.empty() || !msg.ld.empty() || !msg.ms.empty())
                if (auto cm = account->convModule(true))
                    cm->onSyncData(msg, peerId, device.toString());
        } catch (const std::exception& e) {
            JAMI_WARNING("[Account {}] [device {}] [convInfo] error on sync: {:s}",
                         account->getAccountID(),
                         device.to_view(),
                         e.what());
        }
        return std::error_code();
    }));
    socket->onShutdown([w = pimpl_->weak_from_this(), device, s = std::weak_ptr(socket)](const std::error_code&) {
        if (auto shared = w.lock())
            shared->onChannelShutdown(s.lock(), device);
    });

    // Capture the version we are about to deliver before sending the full
    // state. On success, record that this device is synced up to that version
    // so we don't reconnect to it until something changes again. Captured
    // before the send so a concurrent change is never considered delivered.
    auto version = pimpl_->currentVersion();
    dht::ThreadPool::io().run([w = pimpl_->weak_from_this(), socket = std::move(socket), device, version]() {
        if (auto s = w.lock()) {
            if (s->syncInfos(socket, nullptr))
                s->markSynced(device, version);
        }
    });
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
    size_t count = 0;
    for (const auto& [did, sockets] : pimpl_->syncConnections_) {
        if (not sockets.empty() and (!deviceId || deviceId == did)) {
            count++;
            dht::ThreadPool::io().run([w = pimpl_->weak_from_this(), s = sockets.back(), syncMsg] {
                if (auto sthis = w.lock())
                    sthis->syncInfos(s, syncMsg);
            });
        }
    }
    if (count == 0) {
        JAMI_WARNING("[Account {}] [device {}] no sync connection.", pimpl_->accountId_, deviceId.toString());
    } else {
        JAMI_DEBUG("[Account {}] [device {}] syncing with {:d} devices", pimpl_->accountId_, deviceId.to_view(), count);
    }
}

uint64_t
SyncModule::bumpVersion()
{
    return pimpl_->bumpVersion();
}

uint64_t
SyncModule::currentVersion() const
{
    return pimpl_->currentVersion();
}

bool
SyncModule::needsSync(const DeviceId& deviceId) const
{
    return pimpl_->needsSync(deviceId);
}

void
SyncModule::markSynced(const DeviceId& deviceId, uint64_t version)
{
    pimpl_->markSynced(deviceId, version);
}

} // namespace jami
