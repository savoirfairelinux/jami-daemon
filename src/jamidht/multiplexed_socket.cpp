/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "logger.h"
#include "manager.h"
#include "multiplexed_socket.h"
#include "peer_connection.h"
#include "ice_transport.h"
#include "security/certstore.h"

#include <deque>
#include <opendht/thread_pool.h>

namespace jami {

static constexpr std::size_t IO_BUFFER_SIZE {8192}; ///< Size of char buffer used by IO operations

using clock = std::chrono::steady_clock;
using time_point = clock::time_point;

struct ChannelInfo
{
    std::deque<uint8_t> buf {};
    std::mutex mutex {};
    std::condition_variable cv {};
};

class MultiplexedSocket::Impl
{
public:
    Impl(MultiplexedSocket& parent,
         const DeviceId& deviceId,
         std::unique_ptr<TlsSocketEndpoint> endpoint)
        : parent_(parent)
        , deviceId(deviceId)
        , endpoint(std::move(endpoint))
        , eventLoopThread_ {[this] {
            try {
                eventLoop();
            } catch (const std::exception& e) {
                JAMI_ERR() << "[CNX] peer connection event loop failure: " << e.what();
            }
        }}
    {}

    ~Impl()
    {
        if (!isShutdown_) {
            if (endpoint)
                endpoint->setOnStateChange({});
            shutdown();
        } else {
            clearSockets();
        }
        if (eventLoopThread_.joinable())
            eventLoopThread_.join();
    }

    void clearSockets()
    {
        decltype(sockets) socks;
        {
            std::lock_guard<std::mutex> lkSockets(socketsMutex);
            socks = std::move(sockets);
            for (auto& [key, channelData] : channelDatas_) {
                if (channelData)
                    channelData->cv.notify_all();
            }
        }
        for (auto& socket : socks) {
            // Just trigger onShutdown() to make client know
            // No need to write the EOF for the channel, the write will fail because endpoint is
            // already shutdown
            if (socket.second)
                socket.second->stop();
        }
    }

    void shutdown()
    {
        if (isShutdown_)
            return;
        stop.store(true);
        isShutdown_ = true;
        if (beaconTask_)
            beaconTask_->cancel();
        if (onShutdown_)
            onShutdown_();
        if (endpoint) {
            std::unique_lock<std::mutex> lk(writeMtx);
            endpoint->shutdown();
        }
        clearSockets();
    }

    /**
     * Handle packets on the TLS endpoint and parse RTP
     */
    void eventLoop();
    /**
     * Triggered when a new control packet is received
     */
    void handleControlPacket(std::vector<uint8_t>&& pkt);
    /**
     * Triggered when a new packet on a channel is received
     */
    void handleChannelPacket(uint16_t channel, std::vector<uint8_t>&& pkt);
    void onRequest(const std::string& name, uint16_t channel);
    void onAccept(const std::string& name, uint16_t channel);

    void setOnReady(OnConnectionReadyCb&& cb) { onChannelReady_ = std::move(cb); }
    void setOnRequest(OnConnectionRequestCb&& cb) { onRequest_ = std::move(cb); }

    // Beacon
    void sendBeacon(std::chrono::milliseconds timeout);
    void handleBeaconRequest();
    void handleBeaconResponse();
    std::atomic_int beaconCounter_ {0};

    msgpack::unpacker pac_ {};

    MultiplexedSocket& parent_;

    OnConnectionReadyCb onChannelReady_ {};
    OnConnectionRequestCb onRequest_ {};
    OnShutdownCb onShutdown_ {};

    DeviceId deviceId {};
    // Main socket
    std::unique_ptr<TlsSocketEndpoint> endpoint {};

    std::mutex socketsMutex {};
    std::map<uint16_t, std::shared_ptr<ChannelSocket>> sockets {};
    // Contains callback triggered when a channel is ready
    std::mutex channelCbsMutex {};
    std::map<uint16_t, onChannelReadyCb> channelCbs {};

    // Main loop to parse incoming packets
    std::atomic_bool stop {false};
    std::thread eventLoopThread_ {};

    // Multiplexed available datas
    std::map<uint16_t, std::unique_ptr<ChannelInfo>> channelDatas_ {};
    std::mutex channelCbsMtx_ {};
    std::map<uint16_t, GenericSocket<uint8_t>::RecvCb> channelCbs_ {};
    std::atomic_bool isShutdown_ {false};

    std::mutex writeMtx {};

    time_point start_ {clock::now()};
    std::shared_ptr<Task> beaconTask_ {};
};

void
MultiplexedSocket::Impl::eventLoop()
{
    endpoint->setOnStateChange([this](tls::TlsSessionState state) {
        if (state == tls::TlsSessionState::SHUTDOWN && !isShutdown_) {
            JAMI_INFO("Tls endpoint is down, shutdown multiplexed socket");
            shutdown();
            return false;
        }
        return true;
    });
    std::error_code ec;
    while (!stop) {
        if (!endpoint) {
            shutdown();
            return;
        }
        pac_.reserve_buffer(IO_BUFFER_SIZE);
        int size = endpoint->read(reinterpret_cast<uint8_t*>(&pac_.buffer()[0]), IO_BUFFER_SIZE, ec);
        if (size < 0) {
            if (ec)
                JAMI_ERR("Read error detected: %s", ec.message().c_str());
            break;
        }
        if (size == 0) {
            // We can close the socket
            shutdown();
            break;
        }

        pac_.buffer_consumed(size);
        msgpack::object_handle oh;
        while (pac_.next(oh) && !stop) {
            try {
                auto msg = oh.get().as<ChanneledMessage>();
                if (msg.channel == 0)
                    handleControlPacket(std::move(msg.data));
                else
                    handleChannelPacket(msg.channel, std::move(msg.data));
            } catch (...) {
                try {
                    auto msg = oh.get().as<BeaconMsg>();
                    if (msg.isRequest)
                        handleBeaconRequest();
                    else
                        handleBeaconResponse();
                } catch (const std::exception& e) {
                    JAMI_WARN("Error when decoding msgpack message: %s", e.what());
                }
            }
        }
    }
}

void
MultiplexedSocket::Impl::onAccept(const std::string& name, uint16_t channel)
{
    std::lock_guard<std::mutex> lkSockets(socketsMutex);
    auto& channelData = channelDatas_[channel];
    if (not channelData)
        channelData = std::make_unique<ChannelInfo>();
    auto& channelSocket = sockets[channel];
    if (not channelSocket)
        channelSocket = std::make_shared<ChannelSocket>(parent_.weak(), name, channel);
    onChannelReady_(deviceId, channelSocket);
    std::lock_guard<std::mutex> lk(channelCbsMutex);
    auto channelCbsIt = channelCbs.find(channel);
    if (channelCbsIt != channelCbs.end()) {
        (channelCbsIt->second)();
    }
}

void
MultiplexedSocket::Impl::sendBeacon(std::chrono::milliseconds timeout)
{
    // TODO only do this if peer supports it.
    beaconCounter_++;
    JAMI_DBG("Send beacon to peer %s", deviceId.to_c_str());
    BeaconMsg val;
    val.isRequest = true;
    // TODO pack bool
    msgpack::sbuffer buffer(512);
    msgpack::pack(buffer, val);
    std::error_code ec;
    int wr = endpoint->write((const unsigned char*) buffer.data(), buffer.size(), ec);
    if (wr < 0) {
        if (ec)
            JAMI_ERR("Error when writing on socket: %s", ec.message().c_str());
        shutdown();
        return;
    }
    beaconTask_ = Manager::instance().scheduleTaskIn(
        [w = parent_.weak()]() {
            if (auto shared = w.lock()) {
                if (shared->pimpl_->beaconCounter_ != 0) {
                    JAMI_ERR() << "Beacon doesn't get any response. Stopping socket";
                    shared->shutdown();
                }
            }
        },
        timeout);
}

void
MultiplexedSocket::Impl::handleBeaconRequest()
{
    // Run this on dedicated thread because some callbacks can take time
    dht::ThreadPool::io().run([w = parent_.weak()]() {
        if (auto shared = w.lock()) {
            BeaconMsg val;
            val.isRequest = false;
            // TODO pack bool
            msgpack::sbuffer buffer(512);
            msgpack::pack(buffer, val);
            std::error_code ec;
            JAMI_DBG("Send beacon response to peer %s", shared->deviceId().to_c_str());
            int wr = shared->pimpl_->endpoint->write((const unsigned char*) buffer.data(),
                                                     buffer.size(),
                                                     ec);
            if (wr < 0) {
                if (ec)
                    JAMI_ERR("Error when writing on socket: %s", ec.message().c_str());
                shared->shutdown();
                return;
            }
        }
    });
}

void
MultiplexedSocket::Impl::handleBeaconResponse()
{
    JAMI_DBG("Get beacon response from peer %s", deviceId.to_c_str());
    beaconCounter_--;
}

void
MultiplexedSocket::Impl::onRequest(const std::string& name, uint16_t channel)
{
    auto accept = onRequest_(deviceId, channel, name);
    std::shared_ptr<ChannelSocket> channelSocket;
    if (accept) {
        channelSocket = std::make_shared<ChannelSocket>(parent_.weak(), name, channel);
        {
            std::lock_guard<std::mutex> lkSockets(socketsMutex);
            auto sockIt = sockets.find(channel);
            if (sockIt != sockets.end()) {
                JAMI_WARN("A channel is already present on that socket, accepting "
                          "the request will close the previous one");
                sockets.erase(sockIt);
            }
            channelDatas_.emplace(channel, std::make_unique<ChannelInfo>());
            sockets.emplace(channel, channelSocket);
        }
    }

    // Answer to ChannelRequest if accepted
    ChannelRequest val;
    val.channel = channel;
    val.name = name;
    val.state = accept ? ChannelRequestState::ACCEPT : ChannelRequestState::DECLINE;
    msgpack::sbuffer buffer(512);
    msgpack::pack(buffer, val);
    std::error_code ec;
    int wr = parent_.write(CONTROL_CHANNEL,
                           reinterpret_cast<const uint8_t*>(buffer.data()),
                           buffer.size(),
                           ec);
    if (wr < 0) {
        if (ec)
            JAMI_ERR("The write operation failed with error: %s", ec.message().c_str());
        stop.store(true);
        return;
    }

    if (accept) {
        onChannelReady_(deviceId, channelSocket);
        std::lock_guard<std::mutex> lk(channelCbsMutex);
        auto channelCbsIt = channelCbs.find(channel);
        if (channelCbsIt != channelCbs.end()) {
            channelCbsIt->second();
        }
    }
}

void
MultiplexedSocket::Impl::handleControlPacket(std::vector<uint8_t>&& pkt)
{
    // Run this on dedicated thread because some callbacks can take time
    dht::ThreadPool::io().run([w = parent_.weak(), pkt = std::move(pkt)]() {
        try {
            size_t off = 0;
            while (off != pkt.size()) {
                msgpack::unpacked result;
                msgpack::unpack(result, (const char*) pkt.data(), pkt.size(), off);
                auto req = result.get().as<ChannelRequest>();
                if (auto shared = w.lock()) {
                    if (req.state == ChannelRequestState::ACCEPT) {
                        shared->pimpl_->onAccept(req.name, req.channel);
                    } else if (req.state == ChannelRequestState::DECLINE) {
                        std::lock_guard<std::mutex> lkSockets(shared->pimpl_->socketsMutex);
                        shared->pimpl_->channelDatas_.erase(req.channel);
                        shared->pimpl_->sockets.erase(req.channel);
                    } else if (shared->pimpl_->onRequest_) {
                        shared->pimpl_->onRequest(req.name, req.channel);
                    }
                }
            }
        } catch (const std::exception& e) {
            JAMI_ERR("Error on the control channel: %s", e.what());
            if (auto shared = w.lock())
                shared->pimpl_->stop.store(true);
        }
    });
}

void
MultiplexedSocket::Impl::handleChannelPacket(uint16_t channel, std::vector<uint8_t>&& pkt)
{
    std::lock_guard<std::mutex> lkSockets(socketsMutex);
    auto sockIt = sockets.find(channel);
    auto dataIt = channelDatas_.find(channel);
    if (channel > 0 && sockIt->second && dataIt->second) {
        if (pkt.size() == 0) {
            sockIt->second->shutdown();
            dataIt->second->cv.notify_all();
            channelDatas_.erase(dataIt);
            sockets.erase(sockIt);
        } else {
            GenericSocket<uint8_t>::RecvCb cb;
            {
                std::lock_guard<std::mutex> lk(channelCbsMtx_);
                auto cbIt = channelCbs_.find(channel);
                if (cbIt != channelCbs_.end()) {
                    cb = cbIt->second;
                }
            }
            if (cb) {
                cb(&pkt[0], pkt.size());
                return;
            }
            {
                std::lock_guard<std::mutex> lkSockets(dataIt->second->mutex);
                dataIt->second->buf.insert(dataIt->second->buf.end(),
                                           std::make_move_iterator(pkt.begin()),
                                           std::make_move_iterator(pkt.end()));
                dataIt->second->cv.notify_all();
            }
        }
    } else if (pkt.size() != 0) {
        std::string p = std::string(pkt.begin(), pkt.end());
        JAMI_WARN("Non existing channel: %u - %.*s", channel, (int) pkt.size(), p.c_str());
    }
}

MultiplexedSocket::MultiplexedSocket(const DeviceId& deviceId,
                                     std::unique_ptr<TlsSocketEndpoint> endpoint)
    : pimpl_(std::make_unique<Impl>(*this, deviceId, std::move(endpoint)))
{}

MultiplexedSocket::~MultiplexedSocket() {}

std::shared_ptr<ChannelSocket>
MultiplexedSocket::addChannel(const std::string& name)
{
    // Note: because both sides can request the same channel number at the same time
    // it's better to use a random channel number instead of just incrementing the request.
    thread_local dht::crypto::random_device rd;
    std::uniform_int_distribution<uint16_t> dist;
    auto offset = dist(rd);
    std::lock_guard<std::mutex> lk(pimpl_->socketsMutex);
    for (int i = 1; i < UINT16_MAX; ++i) {
        auto c = (offset + i) % UINT16_MAX;
        auto& socket = pimpl_->sockets[c];
        if (!socket) {
            auto& channel = pimpl_->channelDatas_[c];
            if (!channel)
                channel = std::make_unique<ChannelInfo>();
            socket = std::make_shared<ChannelSocket>(weak(), name, c);
            return socket;
        }
    }
    return {};
}

DeviceId
MultiplexedSocket::deviceId() const
{
    return pimpl_->deviceId;
}

void
MultiplexedSocket::setOnReady(OnConnectionReadyCb&& cb)
{
    pimpl_->onChannelReady_ = std::move(cb);
}

void
MultiplexedSocket::setOnRequest(OnConnectionRequestCb&& cb)
{
    pimpl_->onRequest_ = std::move(cb);
}

void
MultiplexedSocket::setOnChannelReady(uint16_t channel, onChannelReadyCb&& cb)
{
    pimpl_->channelCbs[channel] = std::move(cb);
}

bool
MultiplexedSocket::isReliable() const
{
    return true;
}

bool
MultiplexedSocket::isInitiator() const
{
    if (!pimpl_->endpoint) {
        JAMI_WARN("No endpoint found for socket");
        return false;
    }
    return pimpl_->endpoint->isInitiator();
}

int
MultiplexedSocket::maxPayload() const
{
    if (!pimpl_->endpoint) {
        JAMI_WARN("No endpoint found for socket");
        return 0;
    }
    return pimpl_->endpoint->maxPayload();
}

std::size_t
MultiplexedSocket::read(const uint16_t& channel, uint8_t* buf, std::size_t len, std::error_code& ec)
{
    if (pimpl_->isShutdown_) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    std::lock_guard<std::mutex> lkSockets(pimpl_->socketsMutex);
    auto dataIt = pimpl_->channelDatas_.find(channel);
    if (dataIt == pimpl_->channelDatas_.end() || !dataIt->second) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    std::size_t size;
    {
        std::lock_guard<std::mutex> lkSockets(dataIt->second->mutex);
        auto& chanBuf = dataIt->second->buf;
        size = std::min(len, chanBuf.size());

        for (std::size_t i = 0; i < size; ++i)
            buf[i] = chanBuf[i];

        chanBuf.erase(chanBuf.begin(), chanBuf.begin() + size);
    }

    return size;
}

std::size_t
MultiplexedSocket::write(const uint16_t& channel,
                         const uint8_t* buf,
                         std::size_t len,
                         std::error_code& ec)
{
    if (pimpl_->isShutdown_) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    if (len > UINT16_MAX) {
        ec = std::make_error_code(std::errc::message_size);
        return -1;
    }
    bool oneShot = len < 8192;
    msgpack::sbuffer buffer(oneShot ? 16 + len : 16);
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack_array(2);
    pk.pack(channel);
    pk.pack_bin(len);
    if (oneShot)
        pk.pack_bin_body((const char*) buf, len);

    std::unique_lock<std::mutex> lk(pimpl_->writeMtx);
    if (!pimpl_->endpoint) {
        JAMI_WARN("No endpoint found for socket");
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    int res = pimpl_->endpoint->write((const unsigned char*) buffer.data(), buffer.size(), ec);
    if (not oneShot and res >= 0)
        res = pimpl_->endpoint->write(buf, len, ec);
    lk.unlock();
    if (res < 0) {
        if (ec)
            JAMI_ERR("Error when writing on socket: %s", ec.message().c_str());
        shutdown();
    }
    return res;
}

int
MultiplexedSocket::waitForData(const uint16_t& channel,
                               std::chrono::milliseconds timeout,
                               std::error_code& ec) const
{
    if (pimpl_->isShutdown_) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    auto dataIt = pimpl_->channelDatas_.find(channel);
    if (dataIt == pimpl_->channelDatas_.end()) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    auto& channelData = dataIt->second;
    if (!channelData) {
        return -1;
    }
    std::unique_lock<std::mutex> lk {channelData->mutex};
    channelData->cv.wait_for(lk, timeout, [&] {
        return !channelData->buf.empty() or pimpl_->isShutdown_;
    });
    return channelData->buf.size();
}

void
MultiplexedSocket::setOnRecv(const uint16_t& channel, GenericSocket<uint8_t>::RecvCb&& cb)
{
    std::deque<uint8_t> recv;
    {
        std::lock_guard<std::mutex> lk(pimpl_->channelCbsMtx_);
        pimpl_->channelCbs_[channel] = cb;

        auto dataIt = pimpl_->channelDatas_.find(channel);
        if (dataIt != pimpl_->channelDatas_.end() && dataIt->second) {
            auto& channelData = dataIt->second;
            recv = std::move(channelData->buf);
        }
    }
    if (!recv.empty() && cb) {
        cb(&recv[0], recv.size());
    }
}

void
MultiplexedSocket::shutdown()
{
    pimpl_->shutdown();
}

void
MultiplexedSocket::onShutdown(OnShutdownCb&& cb)
{
    pimpl_->onShutdown_ = std::move(cb);
    if (pimpl_->isShutdown_) {
        pimpl_->onShutdown_();
    }
}

std::shared_ptr<IceTransport>
MultiplexedSocket::underlyingICE() const
{
    return pimpl_->endpoint->underlyingICE();
}

void
MultiplexedSocket::monitor() const
{
    auto cert = tls::CertificateStore::instance().getCertificate(deviceId().toString());
    if (!cert)
        return;
    auto userUri = cert->getIssuerUID();
    JAMI_DBG("- Socket with device: %s - account: %s", deviceId().to_c_str(), userUri.c_str());
    auto now = clock::now();
    JAMI_DBG("- Duration: %lu",
             std::chrono::duration_cast<std::chrono::milliseconds>(now - pimpl_->start_).count());
    const auto& ice = underlyingICE();
    if (ice)
        JAMI_DBG("\t- Ice connection: %s", ice->link().c_str());
    std::lock_guard<std::mutex> lk(pimpl_->socketsMutex);
    for (const auto& [_, channel] : pimpl_->sockets) {
        if (channel)
            JAMI_DBG("\t\t- Channel with name %s", channel->name().c_str());
    }
}

void
MultiplexedSocket::sendBeacon(std::chrono::milliseconds timeout)
{
    pimpl_->sendBeacon(timeout);
}

////////////////////////////////////////////////////////////////

class ChannelSocket::Impl
{
public:
    Impl(std::weak_ptr<MultiplexedSocket> endpoint, const std::string& name, const uint16_t& channel)
        : name(name)
        , channel(channel)
        , endpoint(std::move(endpoint))
    {}

    ~Impl() {}

    OnShutdownCb shutdownCb_ {};
    std::atomic_bool isShutdown_ {false};
    std::string name {};
    uint16_t channel {};
    std::weak_ptr<MultiplexedSocket> endpoint {};
};

ChannelSocket::ChannelSocket(std::weak_ptr<MultiplexedSocket> endpoint,
                             const std::string& name,
                             const uint16_t& channel)
    : pimpl_ {std::make_unique<Impl>(endpoint, name, channel)}
{}

ChannelSocket::~ChannelSocket() {}

DeviceId
ChannelSocket::deviceId() const
{
    if (auto ep = pimpl_->endpoint.lock()) {
        return ep->deviceId();
    }
    return {};
}

std::string
ChannelSocket::name() const
{
    return pimpl_->name;
}

uint16_t
ChannelSocket::channel() const
{
    return pimpl_->channel;
}

bool
ChannelSocket::isReliable() const
{
    if (auto ep = pimpl_->endpoint.lock()) {
        return ep->isReliable();
    }
    return false;
}

bool
ChannelSocket::isInitiator() const
{
    if (auto ep = pimpl_->endpoint.lock()) {
        return ep->isInitiator();
    }
    return false;
}

int
ChannelSocket::maxPayload() const
{
    if (auto ep = pimpl_->endpoint.lock()) {
        return ep->maxPayload();
    }
    return -1;
}

void
ChannelSocket::setOnRecv(RecvCb&& cb)
{
    if (auto ep = pimpl_->endpoint.lock())
        ep->setOnRecv(pimpl_->channel, std::move(cb));
}

std::shared_ptr<IceTransport>
ChannelSocket::underlyingICE() const
{
    if (auto mtx = pimpl_->endpoint.lock())
        return mtx->underlyingICE();
    return {};
}

void
ChannelSocket::stop()
{
    if (pimpl_->isShutdown_)
        return;
    pimpl_->isShutdown_ = true;
    if (pimpl_->shutdownCb_)
        pimpl_->shutdownCb_();
}

void
ChannelSocket::shutdown()
{
    if (pimpl_->isShutdown_)
        return;
    stop();
    if (auto ep = pimpl_->endpoint.lock()) {
        std::error_code ec;
        ep->write(pimpl_->channel, nullptr, 0, ec);
    }
}

std::size_t
ChannelSocket::read(ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (auto ep = pimpl_->endpoint.lock()) {
        int res = ep->read(pimpl_->channel, buf, len, ec);
        if (ec)
            JAMI_ERR("Error when reading on channel: %s", ec.message().c_str());
        return res;
    }
    ec = std::make_error_code(std::errc::broken_pipe);
    return -1;
}

std::size_t
ChannelSocket::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (auto ep = pimpl_->endpoint.lock()) {
        std::size_t sent = 0;
        do {
            std::size_t toSend = std::min(static_cast<std::size_t>(UINT16_MAX), len - sent);
            auto res = ep->write(pimpl_->channel, buf + sent, toSend, ec);
            if (ec) {
                JAMI_ERR("Error when writing on channel: %s", ec.message().c_str());
                return res;
            }
            sent += toSend;
        } while (sent < len);
        return sent;
    }
    ec = std::make_error_code(std::errc::broken_pipe);
    return -1;
}

int
ChannelSocket::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    if (auto ep = pimpl_->endpoint.lock()) {
        auto res = ep->waitForData(pimpl_->channel, timeout, ec);
        if (ec)
            JAMI_ERR("Error when waiting on channel: %s", ec.message().c_str());
        return res;
    }
    ec = std::make_error_code(std::errc::broken_pipe);
    return -1;
}

void
ChannelSocket::onShutdown(OnShutdownCb&& cb)
{
    pimpl_->shutdownCb_ = std::move(cb);
    if (pimpl_->isShutdown_) {
        pimpl_->shutdownCb_();
    }
}

void
ChannelSocket::sendBeacon(std::chrono::milliseconds timeout)
{
    if (auto ep = pimpl_->endpoint.lock()) {
        ep->sendBeacon(timeout);
    } else {
        shutdown();
    }
}

} // namespace jami
