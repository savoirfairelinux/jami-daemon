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

#include <deque>
#include <opendht/thread_pool.h>

namespace jami
{

static constexpr std::size_t IO_BUFFER_SIZE {8192}; ///< Size of char buffer used by IO operations

struct ChannelInfo {
    std::deque<uint8_t> buf {};
    std::mutex mutex {};
    std::condition_variable cv {};
};

class MultiplexedSocket::Impl
{
public:
    Impl(MultiplexedSocket& parent, const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint)
    : parent_(parent), deviceId(deviceId), endpoint(std::move(endpoint))
    , eventLoopFut_ { std::async(std::launch::async, [this] {
        try {
            stop.store(false);
            eventLoop();
        } catch (const std::exception& e) {
            JAMI_ERR() << "[CNX] peer connection event loop failure: " << e.what();
        }
    }) } { }

    ~Impl() {
        if (!isShutdown_) {
            shutdown();
        } else {
            std::lock_guard<std::mutex> lkSockets(socketsMutex);
            for (auto& socket : sockets) {
                if (socket.second) socket.second->stop();
            }
            sockets.clear();
        }
    }

    void shutdown() {
        if (isShutdown_) return;
        stop.store(true);
        isShutdown_ = true;
        if (onShutdown_) onShutdown_();
        endpoint->setOnStateChange({});
        endpoint->shutdown();
        {
            std::lock_guard<std::mutex> lkSockets(socketsMutex);
            for (auto& socket : sockets) {
                // Just trigger onShutdown() to make client know
                // No need to write the EOF for the channel, the write will fail because endpoint is already shutdown
                if (socket.second) socket.second->stop();
            }
            sockets.clear();
        }
    }

    /**
     * Handle packets on the TLS endpoint and parse RTP
     */
    void eventLoop();
    /**
     * Triggered when a new control packet is received
     */
    void handleControlPacket(const std::vector<uint8_t>&& pkt);
    /**
     * Triggered when a new packet on a channel is received
     */
    void handleChannelPacket(uint16_t channel, const std::vector<uint8_t>&& pkt);

    void setOnReady(OnConnectionReadyCb&& cb) { onChannelReady_ = std::move(cb); }
    void setOnRequest(OnConnectionRequestCb&& cb) { onRequest_ = std::move(cb); }

    msgpack::unpacker pac_;

    MultiplexedSocket& parent_;

    OnConnectionReadyCb onChannelReady_;
    OnConnectionRequestCb onRequest_;
    OnShutdownCb onShutdown_;

    std::string deviceId;
    // Main socket
    std::unique_ptr<TlsSocketEndpoint> endpoint;

    std::mutex socketsMutex;
    std::map<uint16_t, std::shared_ptr<ChannelSocket>> sockets;
    // Contains callback triggered when a channel is ready
    std::mutex channelCbsMutex;
    std::map<uint16_t, onChannelReadyCb> channelCbs;

    // Main loop to parse incoming packets
    std::future<void> eventLoopFut_;
    std::atomic_bool stop;

    // Multiplexed available datas
    std::map<uint16_t, std::unique_ptr<ChannelInfo>> channelDatas_ {};
    std::mutex channelCbsMtx_ {};
    std::map<uint16_t, GenericSocket<uint8_t>::RecvCb> channelCbs_ {};
    std::atomic_bool isShutdown_ {false};
};

void
MultiplexedSocket::Impl::eventLoop()
{
    endpoint->setOnStateChange([this](tls::TlsSessionState state) {
        if (state == tls::TlsSessionState::SHUTDOWN && !isShutdown_) {
            JAMI_INFO("Tls endpoint is down, shutdown multiplexed socket");
            shutdown();
        }
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
            if (ec) JAMI_ERR("Read error detected: %s", ec.message().c_str());
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

                if (msg.channel == 0) {
                    handleControlPacket(std::move(msg.data));
                } else {
                    handleChannelPacket(msg.channel, std::move(msg.data));
                }
            } catch (const msgpack::unpack_error &e) {
                JAMI_WARN("Error when decoding msgpack message: %s", e.what());
            }
        }
    }
}


void
MultiplexedSocket::Impl::handleControlPacket(const std::vector<uint8_t>&& pkt)
{
    // Run this on dedicated thread because some callbacks can take time
    dht::ThreadPool::io().run([this, pkt = std::move(pkt)]() {
        msgpack::unpacker pac;
        pac.reserve_buffer(pkt.size());
        memcpy(pac.buffer(), pkt.data(), pkt.size());
        pac.buffer_consumed(pkt.size());
        msgpack::object_handle oh;

        while (pac.next(oh)) {
            try {
                auto req = oh.get().as<ChannelRequest>();
                if (req.state == ChannelRequestState::ACCEPT) {
                    std::lock_guard<std::mutex> lkSockets(socketsMutex);
                    auto& channel = channelDatas_[req.channel];
                    if (not channel)
                        channel = std::make_unique<ChannelInfo>();
                    auto& channelSocket = sockets[req.channel];
                    if (not channelSocket)
                        channelSocket = std::make_shared<ChannelSocket>(parent_.weak(), req.name, req.channel);
                    onChannelReady_(deviceId, channelSocket);
                    std::lock_guard<std::mutex> lk(channelCbsMutex);
                    auto channelCbsIt = channelCbs.find(req.channel);
                    if (channelCbsIt != channelCbs.end()) {
                        (channelCbsIt->second)();
                    }
                } else if (req.state == ChannelRequestState::DECLINE) {
                    std::lock_guard<std::mutex> lkSockets(socketsMutex);
                    channelDatas_.erase(req.channel);
                    sockets.erase(req.channel);
                } else if (onRequest_) {
                    auto accept = onRequest_(deviceId, req.channel, req.name);
                    std::shared_ptr<ChannelSocket> channelSocket;
                    if (accept) {
                        channelSocket = std::make_shared<ChannelSocket>(parent_.weak(), req.name, req.channel);
                        {
                            std::lock_guard<std::mutex> lkSockets(socketsMutex);
                            auto sockIt = sockets.find(req.channel);
                            if (sockIt != sockets.end()) {
                                JAMI_WARN("A channel is already present on that socket, accepting the request will close the previous one");
                                sockets.erase(sockIt);
                            }
                            channelDatas_.emplace(req.channel, std::make_unique<ChannelInfo>());
                            sockets.emplace(req.channel, channelSocket);
                        }
                    }

                    // Answer to ChannelRequest if accepted
                    ChannelRequest val;
                    val.channel = req.channel;
                    val.name = req.name;
                    val.state = accept ? ChannelRequestState::ACCEPT : ChannelRequestState::DECLINE;
                    std::stringstream ss;
                    msgpack::pack(ss, val);
                    std::error_code ec;
                    auto toSend = ss.str();
                    int wr = parent_.write(CONTROL_CHANNEL, reinterpret_cast<const uint8_t*>(&toSend[0]), toSend.size(), ec);
                    if (wr < 0) {
                        if (ec)
                            JAMI_ERR("The write operation failed with error: %s", ec.message().c_str());
                        stop.store(true);
                        return;
                    }

                    if (accept) {
                        onChannelReady_(deviceId, channelSocket);
                        std::lock_guard<std::mutex> lk(channelCbsMutex);
                        auto channelCbsIt = channelCbs.find(req.channel);
                        if (channelCbsIt != channelCbs.end()) {
                            (channelCbsIt->second)();
                        }
                    }
                }
            } catch (const std::exception& e) {
                JAMI_ERR("Error on the control channel: %s", e.what());
                stop.store(true);
            }
        }
    });
}

void
MultiplexedSocket::Impl::handleChannelPacket(uint16_t channel, const std::vector<uint8_t>&& pkt)
{
    auto sockIt = sockets.find(channel);
    auto dataIt = channelDatas_.find(channel);
    if (channel > 0 && sockIt->second && dataIt->second) {
        if (pkt.size() == 0) {
            sockIt->second->shutdown();
            dataIt->second->cv.notify_all();
            channelDatas_.erase(dataIt);
            std::lock_guard<std::mutex> lkSockets(socketsMutex);
            sockets.erase(sockIt);
        } else {
            std::unique_lock<std::mutex> lk(channelCbsMtx_);
            auto cb = channelCbs_.find(channel);
            if (cb != channelCbs_.end()) {
                lk.unlock();
                cb->second(&pkt[0], pkt.size());
                return;
            }
            lk.unlock();
            dataIt->second->buf.insert(
                dataIt->second->buf.end(),
                std::make_move_iterator(pkt.begin()),
                std::make_move_iterator(pkt.end()));
            dataIt->second->cv.notify_all();
        }
    } else if (pkt.size() != 0) {
        std::string p = std::string(pkt.begin(), pkt.end());
        JAMI_WARN("Non existing channel: %u - %s - %u", channel, p.c_str(), pkt.size());
    }
}


MultiplexedSocket::MultiplexedSocket(const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint)
: pimpl_(std::make_unique<Impl>(*this, deviceId, std::move(endpoint)))
{
}

MultiplexedSocket::~MultiplexedSocket()
{ }

std::shared_ptr<ChannelSocket>
MultiplexedSocket::addChannel(const std::string& name)
{
    std::lock_guard<std::mutex> lk(pimpl_->socketsMutex);
    for (int i = 1; i < UINT16_MAX; ++i) {
        auto& socket = pimpl_->sockets[i];
        if (!socket) {
            auto& channel = pimpl_->channelDatas_[i];
            if (!channel)
                channel = std::make_unique<ChannelInfo>();
            socket = std::make_shared<ChannelSocket>(weak(), name, i);
            return socket;
        }
    }
    return {};
}

std::string
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
    auto dataIt = pimpl_->channelDatas_.find(channel);
    if (dataIt == pimpl_->channelDatas_.end()
        || !dataIt->second) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    auto& chanBuf = dataIt->second->buf;
    auto size = std::min(len, chanBuf.size());

    for (std::size_t i = 0; i < size; ++i) {
        buf[i] = chanBuf[i];
    }

    chanBuf.erase(chanBuf.begin(), chanBuf.begin() + size);

    return size;
}

std::size_t
MultiplexedSocket::write(const uint16_t& channel, const uint8_t* buf, std::size_t len, std::error_code& ec)
{
    if (pimpl_->isShutdown_) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    if (len > UINT16_MAX) {
        ec = std::make_error_code(std::errc::message_size);
        return -1;
    }
    if (!pimpl_->endpoint) {
        JAMI_WARN("No endpoint found for socket");
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack_array(2);
    pk.pack(channel);
    pk.pack_bin(len);
    pk.pack_bin_body((const char*)buf, len);

    int res = pimpl_->endpoint->write((const unsigned char*)buffer.data(), buffer.size(), ec);
    if (res < 0) {
        if (ec)
            JAMI_ERR("Error when writing on socket: %s", ec.message().c_str());
        shutdown();
    }
    return res;
}

int
MultiplexedSocket::waitForData(const uint16_t& channel, std::chrono::milliseconds timeout, std::error_code& ec) const
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
    channelData->cv.wait_for(lk, timeout, [&]{
        return !channelData->buf.empty();
    });
    return channelData->buf.size();
}

void
MultiplexedSocket::setOnRecv(const uint16_t& channel, GenericSocket<uint8_t>::RecvCb&& cb)
{
    std::lock_guard<std::mutex> lk(pimpl_->channelCbsMtx_);
    pimpl_->channelCbs_[channel] = cb;
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

////////////////////////////////////////////////////////////////

class ChannelSocket::Impl
{
public:
    Impl(std::weak_ptr<MultiplexedSocket> endpoint, const std::string& name, const uint16_t& channel)
    : name(name), channel(channel), endpoint(std::move(endpoint)) {}

    ~Impl() {}

    OnShutdownCb shutdownCb_;
    std::atomic_bool isShutdown_ {false};
    std::string name;
    uint16_t channel;
    std::weak_ptr<MultiplexedSocket> endpoint;
};

ChannelSocket::ChannelSocket(std::weak_ptr<MultiplexedSocket> endpoint, const std::string& name, const uint16_t& channel)
: pimpl_ { std::make_unique<Impl>(endpoint, name, channel) }
{ }


ChannelSocket::~ChannelSocket()
{ }

std::string
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
    pimpl_->isShutdown_ = true;
    if (pimpl_->shutdownCb_) pimpl_->shutdownCb_();
}

void
ChannelSocket::shutdown()
{
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
        if (res < 0) {
            if (ec)
                JAMI_ERR("Error when reading on channel: %s", ec.message().c_str());
            shutdown();
        }
        return res;
    }
    ec = std::make_error_code(std::errc::broken_pipe);
    return -1;
}

std::size_t
ChannelSocket::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (auto ep = pimpl_->endpoint.lock()) {
        int res = ep->write(pimpl_->channel, buf, len, ec);
        if (res < 0) {
            if (ec)
                JAMI_ERR("Error when writing on channel: %s", ec.message().c_str());
            shutdown();
        }
        return res;
    }
    ec = std::make_error_code(std::errc::broken_pipe);
    return -1;
}

int
ChannelSocket::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    if (auto ep = pimpl_->endpoint.lock()) {
        auto res = ep->waitForData(pimpl_->channel, timeout, ec);
        if (res < 0) {
            if (ec)
                JAMI_ERR("Error when waiting on channel: %s", ec.message().c_str());
        }
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


}
