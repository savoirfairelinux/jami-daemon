/*
 *  Copyright (C) 2022-2023 Savoir-faire Linux Inc.
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

#include "connectivity/turn_cache.h"

#include "logger.h"
#include "fileutils.h"
#include "manager.h"
#include "opendht/thread_pool.h" // TODO remove asio
#include "turn_cache.h"

namespace jami {

TurnCache::TurnCache(const std::string& accountId,
                     const std::string& cachePath,
                     const TurnTransportParams& params,
                     bool enabled)
    : accountId_(accountId)
    , cachePath_(cachePath)
    , io_context(Manager::instance().ioContext())
{
    refreshTimer_ = std::make_unique<asio::steady_timer>(*io_context,
                                                         std::chrono::steady_clock::now());
    onConnectedTimer_ = std::make_unique<asio::steady_timer>(*io_context,
                                                         std::chrono::steady_clock::now());
}

TurnCache::~TurnCache() {
    {
        std::lock_guard<std::mutex> lock(shutdownMtx_);
        if (refreshTimer_) {
            refreshTimer_->cancel();
            refreshTimer_.reset();
        }
        if (onConnectedTimer_) {
            onConnectedTimer_->cancel();
            onConnectedTimer_.reset();
        }
    }
    {
        std::lock_guard<std::mutex> lock(cachedTurnMutex_);
        testTurnV4_.reset();
        testTurnV6_.reset();
        cacheTurnV4_.reset();
        cacheTurnV6_.reset();
    }
}

std::optional<IpAddr>
TurnCache::getResolvedTurn(uint16_t family) const
{
    if (family == AF_INET && cacheTurnV4_) {
        return *cacheTurnV4_;
    } else if (family == AF_INET6 && cacheTurnV6_) {
        return *cacheTurnV6_;
    }
    return std::nullopt;
}

void
TurnCache::reconfigure(const TurnTransportParams& params, bool enabled)
{
    params_ = params;
    enabled_ = enabled;
    {
        std::lock_guard<std::mutex> lk(cachedTurnMutex_);
        // Force re-resolution
        isRefreshing_ = false;
        cacheTurnV4_.reset();
        cacheTurnV6_.reset();
        testTurnV4_.reset();
        testTurnV6_.reset();
    }
    std::lock_guard<std::mutex> lock(shutdownMtx_);
    if (refreshTimer_) {
        refreshTimer_->expires_at(std::chrono::steady_clock::now());
        refreshTimer_->async_wait(std::bind(&TurnCache::refresh, shared_from_this(), std::placeholders::_1));
    }
}

void
TurnCache::refresh(const asio::error_code& ec)
{
    if (ec == asio::error::operation_aborted)
        return;
    // The resolution of the TURN server can take quite some time (if timeout).
    // So, run this in its own io thread to avoid to block the main thread.
    // Avoid multiple refresh
    if (isRefreshing_.exchange(true))
        return;
    if (!enabled_) {
        // In this case, we do not use any TURN server
        std::lock_guard<std::mutex> lk(cachedTurnMutex_);
        cacheTurnV4_.reset();
        cacheTurnV6_.reset();
        isRefreshing_ = false;
        return;
    }
    JAMI_INFO("[Account %s] Refresh cache for TURN server resolution", accountId_.c_str());
    // Retrieve old cached value if available.
    // This means that we directly get the correct value when launching the application on the
    // same network
    // No need to resolve, it's already a valid address
    auto server = params_.domain;
    if (IpAddr::isValid(server, AF_INET)) {
        testTurn(IpAddr(server, AF_INET));
        return;
    } else if (IpAddr::isValid(server, AF_INET6)) {
        testTurn(IpAddr(server, AF_INET6));
        return;
    }
    // Else cache resolution result
    fileutils::recursive_mkdir(cachePath_ + DIR_SEPARATOR_STR + "domains", 0700);
    auto pathV4 = cachePath_ + DIR_SEPARATOR_STR + "domains" + DIR_SEPARATOR_STR + "v4." + server;
    IpAddr testV4, testV6;
    if (auto turnV4File = std::ifstream(pathV4)) {
        std::string content((std::istreambuf_iterator<char>(turnV4File)),
                            std::istreambuf_iterator<char>());
        testV4 = IpAddr(content, AF_INET);
    }
    auto pathV6 = cachePath_ + DIR_SEPARATOR_STR + "domains" + DIR_SEPARATOR_STR + "v6." + server;
    if (auto turnV6File = std::ifstream(pathV6)) {
        std::string content((std::istreambuf_iterator<char>(turnV6File)),
                            std::istreambuf_iterator<char>());
        testV6 = IpAddr(content, AF_INET6);
    }
    // Resolve just in case. The user can have a different connectivity
    auto turnV4 = IpAddr {server, AF_INET};
    {
        if (turnV4) {
            // Cache value to avoid a delay when starting up Jami
            std::ofstream turnV4File(pathV4);
            turnV4File << turnV4.toString();
        } else
            fileutils::remove(pathV4, true);
        // Update TURN
        testV4 = IpAddr(std::move(turnV4));
    }
    auto turnV6 = IpAddr {server, AF_INET6};
    {
        if (turnV6) {
            // Cache value to avoid a delay when starting up Jami
            std::ofstream turnV6File(pathV6);
            turnV6File << turnV6.toString();
        } else
            fileutils::remove(pathV6, true);
        // Update TURN
        testV6 = IpAddr(std::move(turnV6));
    }
    if (testV4)
        testTurn(testV4);
    if (testV6)
        testTurn(testV6);

    refreshTurnDelay(!testV4 && !testV6);
}

void
TurnCache::testTurn(IpAddr server)
{
    TurnTransportParams params = params_;
    params.server = server;
    std::lock_guard<std::mutex> lk(cachedTurnMutex_);
    auto& turn = server.isIpv4() ? testTurnV4_ : testTurnV6_;
    turn.reset(); // Stop previous TURN
    try {
        turn = std::make_unique<TurnTransport>(
            params, [this, server](bool ok) {
                // Stop server in an async job, because this callback can be called
                // immediately and cachedTurnMutex_ must not be locked.
                std::lock_guard<std::mutex> lock(shutdownMtx_);
                if (onConnectedTimer_) {
                    onConnectedTimer_->expires_at(std::chrono::steady_clock::now());
                    onConnectedTimer_->async_wait(std::bind(&TurnCache::onConnected, shared_from_this(), std::placeholders::_1, ok, server));
                }
            });
    } catch (const std::exception& e) {
        JAMI_ERROR("TurnTransport creation error: {}", e.what());
    }
}

void
TurnCache::onConnected(const asio::error_code& ec, bool ok, IpAddr server)
{
    if (ec == asio::error::operation_aborted)
        return;

    std::lock_guard<std::mutex> lk(cachedTurnMutex_);
    auto& cacheTurn = server.isIpv4() ? cacheTurnV4_ : cacheTurnV6_;
    if (!ok) {
        JAMI_ERROR("Connection to {:s} failed - reset", server.toString());
        cacheTurn.reset();
    } else {
        JAMI_DEBUG("Connection to {:s} ready", server.toString());
        cacheTurn = std::make_unique<IpAddr>(server);
    }
    refreshTurnDelay(!cacheTurnV6_ && !cacheTurnV4_);
    if (auto& turn = server.isIpv4() ? testTurnV4_ : testTurnV6_)
        turn->shutdown();
}


void
TurnCache::refreshTurnDelay(bool scheduleNext)
{
    isRefreshing_ = false;
    if (scheduleNext) {
        std::lock_guard<std::mutex> lock(shutdownMtx_);
        JAMI_WARNING("[Account {:s}] Cache for TURN resolution failed.", accountId_);
        if (refreshTimer_) {
            refreshTimer_->expires_at(std::chrono::steady_clock::now() + turnRefreshDelay_);
            refreshTimer_->async_wait(std::bind(&TurnCache::refresh, shared_from_this(), std::placeholders::_1));
        }
        if (turnRefreshDelay_ < std::chrono::minutes(30))
            turnRefreshDelay_ *= 2;
    } else {
        JAMI_DEBUG("[Account {:s}] Cache refreshed for TURN resolution", accountId_);
        turnRefreshDelay_ = std::chrono::seconds(10);
    }
}

} // namespace jami
