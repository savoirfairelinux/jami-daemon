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
            eventLoop();
        } catch (const std::exception& e) {
            JAMI_ERR() << "[CNX] peer connection event loop failure: " << e.what();
        }
    }) } { }

    ~Impl() {
        stop.store(true);
    }

    void shutdown() {
        if (isShutdown_) return;
        stop.store(true);
        isShutdown_ = true;
        if (onShutdown_) onShutdown_();
        for (auto& socket : sockets) {
            // Just trigger onShutdown() to make client know
            // No need to write the EOF for the channel, the write will fail because endpoint is already shutdown
            if (socket.second) socket.second->stop();
        }
        sockets.clear();
    }

    /**
     * Handle packets on the TLS endpoint and parse RTP
     */
    void eventLoop();
    /**
     * Triggered when a new control packet is received
     */
    void handleControlPacket(const std::vector<uint8_t>& pkt);
    /**
     * Triggered when a new packet on a channel is received
     */
    void handleChannelPacket(uint16_t channel, const std::vector<uint8_t>& pkt);

    void setOnReady(onConnectionReadyCb&& cb) { onChannelReady_ = std::move(cb); }
    void setOnRequest(onConnectionRequestCb&& cb) { onRequest_ = std::move(cb); }

    MultiplexedSocket& parent_;

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
    std::atomic_bool stop {false};

    std::deque<uint8_t> RtpPkt_ {};
    int wantedSize_ {-1};
    int wantedChan_ {-1};

    // Multiplexed available datas
    std::map<uint16_t, std::unique_ptr<ChannelInfo>> channelDatas_ {};

    onConnectionReadyCb onChannelReady_;
    onConnectionRequestCb onRequest_;
    onShutdownCb onShutdown_;
    std::atomic_bool isShutdown_ {false};
};

void
MultiplexedSocket::Impl::eventLoop()
{
    std::error_code ec;
    while (!stop) {
        std::vector<uint8_t> buf;
        auto data_len = endpoint->waitForData(std::chrono::milliseconds(0), ec);
        if (data_len > 0) {
            buf.resize(IO_BUFFER_SIZE);
            auto size = endpoint->read(&buf[0], 3000, ec);
            if (size < 0) {
                JAMI_ERR("Read error detected: %i", ec);
                wantedSize_ = -1; wantedChan_ = -1;
                RtpPkt_.clear();
                break;
            }
            if (size == 0) {
                // We can close the socket
                shutdown();
                break;
            }

            // A packet has the following format:
            //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
            // | DATA_LEN MSB  | DATA_LEN LSB  | CHANNEL MSB   |  CHANNEL LSB  |         DATA
            // NOTE: if DATA_LEN == 0, it's a EOF
            uint16_t parsed = 0, current_size = 0, current_channel = 0; // Already parsed idx
            std::deque<uint8_t> packet;
            do {
    			uint16_t pkt_len = size - parsed;
                bool store_remaining = true;

                // Get channel
                if (wantedChan_ == -1 && pkt_len + RtpPkt_.size() >= 2) {
                    if (RtpPkt_.size() >= 2) {
                        wantedChan_ = static_cast<uint16_t>((RtpPkt_[0] << 8) + RtpPkt_[0]);
                        RtpPkt_.pop_front();
                        RtpPkt_.pop_front();
                    } else if (RtpPkt_.size() == 1) {
                        wantedChan_ = static_cast<uint16_t>((RtpPkt_[0] << 8) + buf[parsed]);
                        RtpPkt_.pop_front();
                        parsed += 1;
                    } else {
                        wantedChan_ = static_cast<uint16_t>((buf[parsed] << 8) + buf[parsed + 1]);
                        parsed += 2;
                    }
                    pkt_len = size - parsed;
                }

                // Get size
                if (wantedSize_ == -1 && pkt_len + RtpPkt_.size() >= 2) {
                    if (RtpPkt_.size() >= 2) {
                        wantedSize_ = static_cast<uint16_t>((RtpPkt_[0] << 8) + RtpPkt_[0]);
                        RtpPkt_.pop_front();
                        RtpPkt_.pop_front();
                    } else if (RtpPkt_.size() == 1) {
                        wantedSize_ = static_cast<uint16_t>((RtpPkt_[0] << 8) + buf[parsed]);
                        RtpPkt_.pop_front();
                        parsed += 1;
                    } else {
                        wantedSize_ = static_cast<uint16_t>((buf[parsed] << 8) + buf[parsed + 1]);
                        parsed += 2;
                    }
                    pkt_len = size - parsed;
                }

                // Get data
                if (pkt_len + RtpPkt_.size() >= wantedSize_) {
                    // We have enough infos to handle the packet
                    store_remaining = false;
                    current_channel = wantedChan_;
                    current_size = wantedSize_;
                    auto offset_size = RtpPkt_.size();
                    auto parsed_size = current_size - offset_size;
                    packet.swap(RtpPkt_);
                    packet.insert(packet.end(),
                        std::make_move_iterator(buf.begin() + parsed),
                        std::make_move_iterator(buf.begin() + parsed + parsed_size));
                    parsed += parsed_size;
                    pkt_len = size - parsed;
                    // Reset loop values
                    wantedSize_ = -1; wantedChan_ = -1;
                    RtpPkt_.clear();
                }

                // Uncomplete packet
                if (store_remaining) {
                    RtpPkt_.insert(RtpPkt_.end(),
                        std::make_move_iterator(buf.begin() + parsed),
                        std::make_move_iterator(buf.begin() + parsed + pkt_len));
                    parsed += pkt_len;
                }

                // Parse data
                if (current_channel == 0) {
                    handleControlPacket({packet.begin(), packet.end()});
                } else {
                    handleChannelPacket(current_channel, {packet.begin(), packet.end()});
                }
                packet.clear();
            } while (parsed < size && !stop);
        }
    }
}


void
MultiplexedSocket::Impl::handleControlPacket(const std::vector<uint8_t>& pkt)
{
    // Run this on dedicated thread because some callbacks can take time
    dht::ThreadPool::io().run([this, pkt = std::move(pkt)]() {
        msgpack::unpacker pac;
        std::vector<uint8_t> data = {pkt.begin(), pkt.end()};
        pac.reserve_buffer(data.size());
        memcpy(pac.buffer(), data.data(), data.size());
        pac.buffer_consumed(data.size());
        msgpack::object_handle oh;

        while (auto result = pac.next(oh)) {
            try {
                auto req = oh.get().as<ChannelRequest>();
                if (req.isAnswer) {
                    std::lock_guard<std::mutex> lkSockets(socketsMutex);
                    std::shared_ptr<ChannelSocket> channelSocket = nullptr;
                    if (channelDatas_.find(req.channel) == channelDatas_.end()) {
                        channelDatas_.insert({req.channel, std::make_unique<ChannelInfo>()});
                    }
                    if (this->sockets.find(req.channel) == this->sockets.end()) {
                        channelSocket = std::make_shared<ChannelSocket>(this->parent_.weak(), req.name, req.channel);
                        this->sockets.insert({req.channel, channelSocket});
                    } else {
                        channelSocket = this->sockets.at(req.channel);
                    }
                    this->onChannelReady_(this->deviceId, channelSocket);
                    std::lock_guard<std::mutex> lk(channelCbsMutex);
                    if (this->channelCbs.find(req.channel) != this->channelCbs.end()) {
                        this->channelCbs.at(req.channel)();
                    }
                } else if (this->onRequest_ && this->onRequest_(this->deviceId, req.channel, req.name)) {
                    auto channelSocket = std::make_shared<ChannelSocket>(this->parent_.weak(), req.name, req.channel);
                    {
                        std::lock_guard<std::mutex> lkSockets(socketsMutex);
                        if (this->sockets.find(req.channel) != this->sockets.end()) {
                            JAMI_WARN("A channel is already present on that socket, accepting the request will close the previous one");
                            this->sockets.erase(req.channel);
                        }
                        channelDatas_.insert({req.channel, std::make_unique<ChannelInfo>()});
                        this->sockets.insert({req.channel, channelSocket});
                    }

                    // Answer to ChannelRequest if accepted
                    ChannelRequest val;
                    val.channel = req.channel;
                    val.name = req.name;
                    val.isAnswer = true;
                    std::stringstream ss;
                    msgpack::pack(ss, val);
                    std::error_code ec;
                    auto toSend = ss.str();
                    this->parent_.write(CONTROL_CHANNEL, reinterpret_cast<const uint8_t*>(&toSend[0]), toSend.size(), ec);

                    this->onChannelReady_(this->deviceId, channelSocket);
                    std::lock_guard<std::mutex> lk(channelCbsMutex);
                    if (this->channelCbs.find(req.channel) != this->channelCbs.end()) {
                        this->channelCbs.at(req.channel)();
                    }
                }
            } catch (const std::exception& e) {
                JAMI_ERR("Error on the control channel: %s", e.what());
            }
        }
    });
}

void
MultiplexedSocket::Impl::handleChannelPacket(uint16_t channel, const std::vector<uint8_t>& pkt)
{
    if (channel > 0 && sockets[channel] && channelDatas_[channel]) {
        if (pkt.size() == 0) {
            sockets[channel]->shutdown();
            channelDatas_[channel]->cv.notify_all();
            channelDatas_.erase(channel);
            sockets.erase(channel);
        } else {
            channelDatas_[channel]->buf.insert(
                channelDatas_[channel]->buf.end(),
                std::make_move_iterator(pkt.begin()),
                std::make_move_iterator(pkt.end()));
            channelDatas_[channel]->cv.notify_all();
        }
    } else {
        JAMI_WARN("Non existing channel: %u", channel);
    }
}


MultiplexedSocket::MultiplexedSocket(const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint)
: pimpl_(std::make_unique<Impl>(*this, deviceId, std::move(endpoint)))
{
}

MultiplexedSocket::~MultiplexedSocket()
{ }

std::shared_ptr<ChannelSocket>
MultiplexedSocket::addChannel(const std::string name)
{
    std::lock_guard<std::mutex> lk(pimpl_->socketsMutex);
    for (int i = 1; i < UINT8_MAX; ++i) {
        if (!pimpl_->sockets[i]) {
            if (pimpl_->channelDatas_.find(i) == pimpl_->channelDatas_.end())
                pimpl_->channelDatas_.insert({i, std::make_unique<ChannelInfo>()});
            pimpl_->sockets[i] = std::make_shared<ChannelSocket>(weak(), name, i);
            return pimpl_->sockets[i];
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
MultiplexedSocket::setOnReady(onConnectionReadyCb&& cb)
{
    pimpl_->onChannelReady_ = std::move(cb);
}

void
MultiplexedSocket::setOnRequest(onConnectionRequestCb&& cb)
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
    if (pimpl_->channelDatas_.find(channel) == pimpl_->channelDatas_.end()
        || !pimpl_->channelDatas_[channel]) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    auto& chanBuf = pimpl_->channelDatas_[channel]->buf;
    auto size = std::min(len, chanBuf.size());

    for (int i = 0; i < size; ++i) {
        buf[i] = chanBuf[i];
    }

    chanBuf.erase(chanBuf.begin(), chanBuf.begin() + size);

    return size;
}

std::size_t
MultiplexedSocket::write(const uint16_t& channel, const uint8_t* buf, std::size_t len, std::error_code& ec)
{
    if (len > UINT8_MAX) {
        ec = std::make_error_code(std::errc::message_size);
        return -1;
    }
    if (!pimpl_->endpoint) {
        JAMI_WARN("No endpoint found for socket");
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    std::vector<uint8_t> toSend;
    toSend.reserve(len + 4);
    toSend.emplace_back(static_cast<uint8_t>(channel >> 8));
    toSend.emplace_back(static_cast<uint8_t>(channel));
    toSend.emplace_back(static_cast<uint8_t>(len >> 8));
    toSend.emplace_back(static_cast<uint8_t>(len));

    auto v = std::vector<uint8_t>(buf, buf + len);
    toSend.insert(toSend.end(), std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()));
    return pimpl_->endpoint->write(&toSend[0], len + 4, ec);
}

int
MultiplexedSocket::waitForData(const uint16_t& channel, std::chrono::milliseconds timeout, std::error_code& ec) const
{
    if (pimpl_->channelDatas_.find(channel) == pimpl_->channelDatas_.end()) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    auto& channelData = pimpl_->channelDatas_[channel];
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
MultiplexedSocket::shutdown()
{
    pimpl_->shutdown();
}

void
MultiplexedSocket::onShutdown(onShutdownCb&& cb)
{
    pimpl_->onShutdown_ = std::move(cb);
    if (pimpl_->isShutdown_) {
        pimpl_->onShutdown_();
    }
}


////////////////////////////////////////////////////////////////

class ChannelSocket::Impl
{
public:
    Impl(std::weak_ptr<MultiplexedSocket> endpoint, const std::string& name, const uint16_t& channel)
    : endpoint(endpoint), name(name), channel(channel) {}

    ~Impl() {}

    std::weak_ptr<MultiplexedSocket> endpoint;
    std::string name;
    uint16_t channel;
    onShutdownCb shutdownCb_;
    std::atomic_bool isShutdown_ {false};
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
        return ep->read(pimpl_->channel, buf, len, ec);
    }
    ec = std::make_error_code(std::errc::broken_pipe);
    return -1;
}

std::size_t
ChannelSocket::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (auto ep = pimpl_->endpoint.lock()) {
        return ep->write(pimpl_->channel, buf, len, ec);
    }
    ec = std::make_error_code(std::errc::broken_pipe);
    return -1;
}

int
ChannelSocket::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    if (auto ep = pimpl_->endpoint.lock()) {
        return ep->waitForData(pimpl_->channel, timeout, ec);
    }
    ec = std::make_error_code(std::errc::broken_pipe);
    return -1;
}

void
ChannelSocket::onShutdown(onShutdownCb&& cb)
{
    pimpl_->shutdownCb_ = std::move(cb);
    if (pimpl_->isShutdown_) {
        pimpl_->shutdownCb_();
    }
}


}
