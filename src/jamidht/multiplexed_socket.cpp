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

// TODO move socket code
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

    void eventLoop();
    void handleControlPacket(const std::vector<uint8_t>& pkt);
    void handleChannelPacket(uint16_t& channel, const std::vector<uint8_t>& pkt);

    void setOnReady(onConnectionReadyCb&& cb) { onChannelReady_ = std::move(cb); }
    void setOnRequest(onConnectionRequestCb&& cb) { onRequest_ = std::move(cb); }

    MultiplexedSocket& parent_;

    std::string deviceId;
    std::unique_ptr<TlsSocketEndpoint> endpoint;
    std::mutex socketsMutex;
    std::map<uint16_t, std::shared_ptr<ChannelSocket>> sockets;
    // TODO mutex
    std::map<uint16_t, onChannelReadyCb> channelCbs;
    
    // TODO only receive from accepted
    std::map<uint16_t, std::vector<uint8_t>> socketsData;
    std::future<void> eventLoopFut_;
    std::atomic_bool stop {false};

    std::deque<uint8_t> rtp_pkt {};
    int wanted_size {-1};
    int wanted_channel {-1};

    onConnectionReadyCb onChannelReady_;
    onConnectionRequestCb onRequest_;
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
                wanted_size = -1; wanted_channel = -1;
                rtp_pkt.clear();
                break;
            }

            // A packet has the following format:
            //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
            // | DATA_LEN MSB  | DATA_LEN LSB  | CHANNEL MSB   |  CHANNEL LSB  |         DATA
            uint16_t parsed = 0, current_size = 0, current_channel = 0; // Already parsed idx
            std::deque<uint8_t> packet;
            do {
    			uint16_t pkt_len = size - parsed;
                bool store_remaining = true;

                // Get channel
                if (wanted_channel == -1 && pkt_len + rtp_pkt.size() > 2) {
                    if (rtp_pkt.size() >= 2) {
                        wanted_channel = static_cast<uint16_t>((rtp_pkt[0] << 8) + rtp_pkt[0]);
                        rtp_pkt.pop_front();
                        rtp_pkt.pop_front();
                    } else if (rtp_pkt.size() == 1) {
                        wanted_channel = static_cast<uint16_t>((rtp_pkt[0] << 8) + buf[parsed]);
                        rtp_pkt.pop_front();
                        parsed += 1;
                    } else {
                        wanted_channel = static_cast<uint16_t>((buf[parsed] << 8) + buf[parsed + 1]);
                        parsed += 2;
                    }
                    pkt_len = size - parsed;
                }

                // Get size
                if (wanted_size == -1 && pkt_len + rtp_pkt.size() > 2) {
                    if (rtp_pkt.size() >= 2) {
                        wanted_size = static_cast<uint16_t>((rtp_pkt[0] << 8) + rtp_pkt[0]);
                        rtp_pkt.pop_front();
                        rtp_pkt.pop_front();
                    } else if (rtp_pkt.size() == 1) {
                        wanted_size = static_cast<uint16_t>((rtp_pkt[0] << 8) + buf[parsed]);
                        rtp_pkt.pop_front();
                        parsed += 1;
                    } else {
                        wanted_size = static_cast<uint16_t>((buf[parsed] << 8) + buf[parsed + 1]);
                        parsed += 2;
                    }
                    pkt_len = size - parsed;
                }

                // Get data
                if (pkt_len + rtp_pkt.size() >= wanted_size) {
                    // We have enough infos to handle the packet
                    store_remaining = false;
                    current_channel = wanted_channel;
                    current_size = wanted_size;
                    auto offset_size = rtp_pkt.size();
                    auto parsed_size = current_size - offset_size;
                    packet.swap(rtp_pkt);
                    // TODO better method
                    for (int i = 0 ; i < parsed_size ; i++) {
                        packet.emplace_back(buf[parsed + i]);
                    }
                    parsed += parsed_size;
                    pkt_len = size - parsed;
                    // Reset loop values
                    wanted_size = -1; wanted_channel = -1;
                    rtp_pkt.clear();
                }

                // Uncomplete packet
                if (store_remaining) {
                    // TODO better method
                    for (int i = 0; i < pkt_len; ++i) {
                        rtp_pkt.emplace_back(buf[parsed + i]);
                    }
                    parsed += pkt_len;
                }

                // Parse data
                if (!packet.empty()) {
                    if (current_channel == 0) {
                        handleControlPacket({packet.begin(), packet.end()});
                    } else {
                        handleChannelPacket(current_channel, {packet.begin(), packet.end()});
                    }
                    packet.clear();
                }
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
                    std::shared_ptr<ChannelSocket> channelSocket = nullptr;
                    if (this->sockets.find(req.channel) == this->sockets.end()) {
                        channelSocket = std::make_shared<ChannelSocket>(this->parent_, req.name, req.channel);
                        this->sockets.insert({req.channel, channelSocket});
                    } else {
                        channelSocket = this->sockets.at(req.channel);
                    }
                    this->onChannelReady_(this->deviceId, channelSocket);
                    if (this->channelCbs.find(req.channel) != this->channelCbs.end()) {
                        this->channelCbs.at(req.channel)();
                    }
                } else if (this->onRequest_ && this->onRequest_(this->deviceId, req.channel, req.name)) {
                    if (this->sockets.find(req.channel) != this->sockets.end()) {
                        JAMI_WARN("A channel is already present on that socket, accepting the request will close the previous one");
                        this->sockets.erase(req.channel);
                    }
                    auto channelSocket = std::make_shared<ChannelSocket>(this->parent_, req.name, req.channel);
                    this->sockets.insert({req.channel, channelSocket});

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
MultiplexedSocket::Impl::handleChannelPacket(uint16_t& channel, const std::vector<uint8_t>& pkt)
{
    JAMI_ERR("DATA");
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
    // TODO improve this
    std::lock_guard<std::mutex> lk(pimpl_->socketsMutex);
    for (int i = 1; i < 65536; ++i) {
        if (pimpl_->sockets.find(i) == pimpl_->sockets.end()) {
            pimpl_->sockets[i] = std::make_shared<ChannelSocket>(*this, name, i);
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
    // TODO CHANNEL
    if (!pimpl_->endpoint) {
        JAMI_WARN("No endpoint found for socket");
        return 0;
    }
    return pimpl_->endpoint->read(buf, len, ec);
}

std::size_t
MultiplexedSocket::write(const uint16_t& channel, const uint8_t* buf, std::size_t len, std::error_code& ec)
{
    // TODO len more than 65536
    if (!pimpl_->endpoint) {
        JAMI_WARN("No endpoint found for socket");
        return 0;
    }
    std::vector<uint8_t> toSend;
    toSend.reserve(len + 4);
    toSend[0] = static_cast<uint8_t>(channel >> 8);
    toSend[1] = static_cast<uint8_t>(channel);
    toSend[2] = static_cast<uint8_t>(len >> 8);
    toSend[3] = static_cast<uint8_t>(len);
    // TODO better way
    for (int i = 0; i < len; ++i) {
        toSend[4+i] = buf[i];
    }

    return pimpl_->endpoint->write(&toSend[0], len + 4, ec);
}

int
MultiplexedSocket::waitForData(const uint16_t& channel, std::chrono::milliseconds timeout, std::error_code& ec) const
{
    // TODO CHANNEL
    if (!pimpl_->endpoint) {
        JAMI_WARN("No endpoint found for socket");
        return 0;
    }
    return pimpl_->endpoint->waitForData(timeout, ec);
}

////////////////////////////////////////////////////////////////

class ChannelSocket::Impl
{
public:
    Impl(MultiplexedSocket& endpoint, const std::string& name, const uint16_t& channel)
    : endpoint(endpoint), name(name), channel(channel) {}

    ~Impl() {}

    MultiplexedSocket& endpoint;
    std::string name;
    uint16_t channel;
};

ChannelSocket::ChannelSocket(MultiplexedSocket& endpoint, const std::string& name, const uint16_t& channel)
: pimpl_ { std::make_unique<Impl>(endpoint, name, channel) }
{ }


ChannelSocket::~ChannelSocket()
{ }

std::string
ChannelSocket::deviceId() const
{
    return pimpl_->endpoint.deviceId();
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
    return pimpl_->endpoint.isReliable();
}

bool
ChannelSocket::isInitiator() const
{
    return pimpl_->endpoint.isInitiator();
}

int
ChannelSocket::maxPayload() const
{
    return pimpl_->endpoint.maxPayload();
}

std::size_t
ChannelSocket::read(ValueType* buf, std::size_t len, std::error_code& ec)
{
    return pimpl_->endpoint.read(pimpl_->channel, buf, len, ec);
}

std::size_t
ChannelSocket::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    return pimpl_->endpoint.write(pimpl_->channel, buf, len, ec);
}

int
ChannelSocket::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    return pimpl_->endpoint.waitForData(pimpl_->channel, timeout, ec);
}

}
