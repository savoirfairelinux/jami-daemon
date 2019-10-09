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

#include "multiplexed_socket.h"
#include "logger.h"
#include "peer_connection.h"

namespace jami
{

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

    std::vector<uint8_t> rtp_pkt {};
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
            buf.resize(3000 /* TODO */);
            wanted_size = -1;
            wanted_channel = -1;
            rtp_pkt.clear();
            auto pkt_len = endpoint->read(&buf[0], 3000, ec);
            if (pkt_len < 0) {
                JAMI_ERR("Read error detected: %i", ec);
                break;
            }

            if (wanted_channel == -1 && wanted_size == -1 && rtp_pkt.empty()) {
                // TODO size
                wanted_channel = static_cast<uint16_t>((buf[0] << 8) + buf[1]);
                wanted_size = static_cast<uint16_t>((buf[2] << 8) + buf[3]);
                if (wanted_size == 0) {
                    continue;
                }
                rtp_pkt.reserve(wanted_size);
                // TODO better way
                for (int i = 0; i < wanted_size; ++i) {
                    rtp_pkt.push_back(buf[4+i]);
                }
            } else {
                JAMI_WARN("TODO INCOMPLETE");
                break;
            }

            // TODO move
            if (wanted_channel == 0) {
                msgpack::unpacker pac;
                pac.reserve_buffer(rtp_pkt.size());
                memcpy(pac.buffer(), rtp_pkt.data(), rtp_pkt.size());
                pac.buffer_consumed(rtp_pkt.size());
                msgpack::object_handle oh;

                while (auto result = pac.next(oh) && !stop) {
                    try {
                        auto req = oh.get().as<ChannelRequest>();
                        if (req.isAnswer) {
                            std::shared_ptr<ChannelSocket> channelSocket = nullptr;
                            if (sockets.find(req.channel) == sockets.end()) {
                                channelSocket = std::make_shared<ChannelSocket>(parent_, req.name, req.channel);
                                sockets.insert({req.channel, channelSocket});
                            } else {
                                channelSocket = sockets.at(req.channel);
                            }
                            onChannelReady_(deviceId, channelSocket);
                            if (channelCbs.find(req.channel) != channelCbs.end()) {
                                channelCbs.at(req.channel)();
                            }
                        } else {
                            if (onRequest_ && onRequest_(deviceId, req.channel, req.name)) {
                                if (sockets.find(req.channel) != sockets.end()) {
                                    JAMI_WARN("A channel is already present on that socket, accepting the request will close the previous one");
                                    sockets.erase(req.channel);
                                }
                                auto channelSocket = std::make_shared<ChannelSocket>(parent_, req.name, req.channel);
                                sockets.insert({req.channel, channelSocket});

                                ChannelRequest val;
                                val.channel = req.channel;
                                val.name = req.name;
                                val.isAnswer = true;
                                std::stringstream ss;
                                msgpack::pack(ss, val);
                                std::error_code ec;
                                auto toSend = ss.str();
                                parent_.write(0, reinterpret_cast<const uint8_t*>(&toSend[0]), toSend.size(), ec);

                                onChannelReady_(deviceId, channelSocket);
                                if (channelCbs.find(req.channel) != channelCbs.end()) {
                                    channelCbs.at(req.channel)();
                                }
                            }
                        }
                    } catch (...) {
                        //JAMI_ERR("ERR!");
                    }
                }
            } else {
                JAMI_WARN("DATA ON CHANNEL %i", wanted_channel);
            }
        }
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
// TODO move
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
