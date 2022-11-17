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
#include "connectivity/security/certstore.h"

#include <opendht/thread_pool.h>
#include <asio/io_context.hpp>
#include <deque>

static constexpr std::size_t IO_BUFFER_SIZE {8192}; ///< Size of char buffer used by IO operations
static constexpr int MULTIPLEXED_SOCKET_VERSION {1};

struct ChanneledMessage
{
    uint16_t channel;
    std::vector<uint8_t> data;
    MSGPACK_DEFINE(channel, data)
};

struct BeaconMsg
{
    bool p;
    MSGPACK_DEFINE_MAP(p)
};

struct VersionMsg
{
    int v;
    MSGPACK_DEFINE_MAP(v)
};

namespace jami {

using clock = std::chrono::steady_clock;
using time_point = clock::time_point;

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
                shutdown();
            }
        }}
    {}

    ~Impl() {}

    void join()
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

    std::shared_ptr<ChannelSocket> makeSocket(const std::string& name,
                                              uint16_t channel,
                                              bool isInitiator = false)
    {
        auto& channelSocket = sockets[channel];
        if (not channelSocket)
            channelSocket = std::make_shared<ChannelSocket>(parent_.weak(),
                                                            name,
                                                            channel,
                                                            isInitiator,
                                                            [w = parent_.weak(), channel]() {
                                                                // Remove socket in another thread to avoid any lock
                                                                dht::ThreadPool::io().run([w, channel]() {
                                                                    if (auto shared = w.lock()) {
                                                                        shared->eraseChannel(channel);
                                                                    }
                                                                });

                                                            });
        else {
            JAMI_WARN("A channel is already present on that socket, accepting "
                      "the request will close the previous one %s",
                      name.c_str());
        }
        return channelSocket;
    }

    /**
     * Handle packets on the TLS endpoint and parse RTP
     */
    void eventLoop();
    /**
     * Triggered when a new control packet is received
     */
    void handleControlPacket(std::vector<uint8_t>&& pkt);
    void handleProtocolPacket(std::vector<uint8_t>&& pkt);
    bool handleProtocolMsg(const msgpack::object& o);
    /**
     * Triggered when a new packet on a channel is received
     */
    void handleChannelPacket(uint16_t channel, std::vector<uint8_t>&& pkt);
    void onRequest(const std::string& name, uint16_t channel);
    void onAccept(const std::string& name, uint16_t channel);

    void setOnReady(OnConnectionReadyCb&& cb) { onChannelReady_ = std::move(cb); }
    void setOnRequest(OnConnectionRequestCb&& cb) { onRequest_ = std::move(cb); }

    // Beacon
    void sendBeacon(const std::chrono::milliseconds& timeout);
    void handleBeaconRequest();
    void handleBeaconResponse();
    std::atomic_int beaconCounter_ {0};

    bool writeProtocolMessage(const msgpack::sbuffer& buffer);

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

    // Main loop to parse incoming packets
    std::atomic_bool stop {false};
    std::thread eventLoopThread_ {};

    std::atomic_bool isShutdown_ {false};

    std::mutex writeMtx {};

    time_point start_ {clock::now()};
    std::shared_ptr<Task> beaconTask_ {};

    // version related stuff
    void sendVersion();
    void onVersion(int version);
    std::atomic_bool canSendBeacon_ {false};
    std::atomic_bool answerBeacon_ {true};
    int version_ {MULTIPLEXED_SOCKET_VERSION};
    std::function<void(bool)> onBeaconCb_ {};
    std::function<void(int)> onVersionCb_ {};
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
    sendVersion();
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
                if (msg.channel == CONTROL_CHANNEL)
                    handleControlPacket(std::move(msg.data));
                else if (msg.channel == PROTOCOL_CHANNEL)
                    handleProtocolPacket(std::move(msg.data));
                else
                    handleChannelPacket(msg.channel, std::move(msg.data));
            } catch (const std::exception& E) {
                JAMI_WARN("Failed to unpacked message of %d bytes: %s", size, E.what());
            } catch (...) {
                JAMI_ERR("Unknown exception catched while unpacking message of %d bytes", size);
            }
        }
    }
}

void
MultiplexedSocket::Impl::onAccept(const std::string& name, uint16_t channel)
{
    std::lock_guard<std::mutex> lkSockets(socketsMutex);
    auto& socket = sockets[channel];
    if (!socket) {
        JAMI_ERR() << "Receiving an answer for a non existing channel. This is a bug.";
        return;
    }

    onChannelReady_(deviceId, socket);
    socket->ready();
    // Due to the callbacks that can take some time, onAccept can arrive after
    // receiving all the data. In this case, the socket should be removed here
    // as handle by onChannelReady_
    if (socket->isRemovable())
        sockets.erase(channel);
    else
        socket->answered();
}

void
MultiplexedSocket::Impl::sendBeacon(const std::chrono::milliseconds& timeout)
{
    if (!canSendBeacon_)
        return;
    beaconCounter_++;
    JAMI_DBG("Send beacon to peer %s", deviceId.to_c_str());

    msgpack::sbuffer buffer(8);
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack(BeaconMsg {true});
    if (!writeProtocolMessage(buffer))
        return;
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
    if (!answerBeacon_)
        return;
    // Run this on dedicated thread because some callbacks can take time
    dht::ThreadPool::io().run([w = parent_.weak()]() {
        if (auto shared = w.lock()) {
            msgpack::sbuffer buffer(8);
            msgpack::packer<msgpack::sbuffer> pk(&buffer);
            pk.pack(BeaconMsg {false});
            JAMI_DBG("Send beacon response to peer %s", shared->deviceId().to_c_str());
            shared->pimpl_->writeProtocolMessage(buffer);
        }
    });
}

void
MultiplexedSocket::Impl::handleBeaconResponse()
{
    JAMI_DBG("Get beacon response from peer %s", deviceId.to_c_str());
    beaconCounter_--;
}

bool
MultiplexedSocket::Impl::writeProtocolMessage(const msgpack::sbuffer& buffer)
{
    std::error_code ec;
    int wr = parent_.write(PROTOCOL_CHANNEL,
                           (const unsigned char*) buffer.data(),
                           buffer.size(),
                           ec);
    return wr > 0;
}

void
MultiplexedSocket::Impl::sendVersion()
{
    dht::ThreadPool::io().run([w = parent_.weak()]() {
        if (auto shared = w.lock()) {
            auto version = shared->pimpl_->version_;
            msgpack::sbuffer buffer(8);
            msgpack::packer<msgpack::sbuffer> pk(&buffer);
            pk.pack(VersionMsg {version});
            shared->pimpl_->writeProtocolMessage(buffer);
        }
    });
}

void
MultiplexedSocket::Impl::onVersion(int version)
{
    // Check if version > 1
    if (version >= 1) {
        JAMI_INFO() << "Enable beacon support for " << deviceId;
        canSendBeacon_ = true;
    } else {
        JAMI_WARN("Peer %s uses an old version which doesn't support all the features (%d)",
                  deviceId.to_c_str(),
                  version);
        canSendBeacon_ = false;
    }
}

void
MultiplexedSocket::Impl::onRequest(const std::string& name, uint16_t channel)
{
    auto accept = onRequest_(endpoint->peerCertificate(), channel, name);
    std::shared_ptr<ChannelSocket> channelSocket;
    if (accept) {
        std::lock_guard<std::mutex> lkSockets(socketsMutex);
        channelSocket = makeSocket(name, channel);
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
        channelSocket->ready();
    }
}

void
MultiplexedSocket::Impl::handleControlPacket(std::vector<uint8_t>&& pkt)
{
    // Run this on dedicated thread because some callbacks can take time
    dht::ThreadPool::io().run([w = parent_.weak(), pkt = std::move(pkt)]() {
        auto shared = w.lock();
        if (!shared)
            return;
        try {
            size_t off = 0;
            while (off != pkt.size()) {
                msgpack::unpacked result;
                msgpack::unpack(result, (const char*) pkt.data(), pkt.size(), off);
                auto object = result.get();
                auto& pimpl = *shared->pimpl_;
                if (pimpl.handleProtocolMsg(object))
                    continue;
                auto req = object.as<ChannelRequest>();
                if (req.state == ChannelRequestState::ACCEPT) {
                    pimpl.onAccept(req.name, req.channel);
                } else if (req.state == ChannelRequestState::DECLINE) {
                    std::lock_guard<std::mutex> lkSockets(pimpl.socketsMutex);
                    auto channel = pimpl.sockets.find(req.channel);
                    if (channel != pimpl.sockets.end()) {
                        channel->second->stop();
                        pimpl.sockets.erase(channel);
                    }
                } else if (pimpl.onRequest_) {
                    pimpl.onRequest(req.name, req.channel);
                }
            }
        } catch (const std::exception& e) {
            JAMI_ERR("Error on the control channel: %s", e.what());
        }
    });
}

void
MultiplexedSocket::Impl::handleChannelPacket(uint16_t channel, std::vector<uint8_t>&& pkt)
{
    std::lock_guard<std::mutex> lkSockets(socketsMutex);
    auto sockIt = sockets.find(channel);
    if (channel > 0 && sockIt != sockets.end() && sockIt->second) {
        if (pkt.size() == 0) {
            sockIt->second->stop();
            if (sockIt->second->isAnswered())
                sockets.erase(sockIt);
            else
                sockIt->second->removable(); // This means that onAccept didn't happen yet, will be
                                             // removed later.
        } else {
            sockIt->second->onRecv(std::move(pkt));
        }
    } else if (pkt.size() != 0) {
        JAMI_WARN("Non existing channel: %u", channel);
    }
}

bool
MultiplexedSocket::Impl::handleProtocolMsg(const msgpack::object& o)
{
    try {
        if (o.type == msgpack::type::MAP && o.via.map.size > 0) {
            auto key = o.via.map.ptr[0].key.as<std::string_view>();
            if (key == "p") {
                auto msg = o.as<BeaconMsg>();
                if (msg.p)
                    handleBeaconRequest();
                else
                    handleBeaconResponse();
                if (onBeaconCb_)
                    onBeaconCb_(msg.p);
                return true;
            } else if (key == "v") {
                auto msg = o.as<VersionMsg>();
                onVersion(msg.v);
                if (onVersionCb_)
                    onVersionCb_(msg.v);
                return true;
            } else {
                JAMI_WARN("Unknown message type");
            }
        }
    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
    }
    return false;
}

void
MultiplexedSocket::Impl::handleProtocolPacket(std::vector<uint8_t>&& pkt)
{
    // Run this on dedicated thread because some callbacks can take time
    dht::ThreadPool::io().run([w = parent_.weak(), pkt = std::move(pkt)]() {
        auto shared = w.lock();
        if (!shared)
            return;
        try {
            size_t off = 0;
            while (off != pkt.size()) {
                msgpack::unpacked result;
                msgpack::unpack(result, (const char*) pkt.data(), pkt.size(), off);
                auto object = result.get();
                if (shared->pimpl_->handleProtocolMsg(object))
                    return;
            }
        } catch (const std::exception& e) {
            JAMI_ERR("Error on the protocol channel: %s", e.what());
        }
    });
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
        if (c == CONTROL_CHANNEL || c == PROTOCOL_CHANNEL
            || pimpl_->sockets.find(c) != pimpl_->sockets.end())
            continue;
        auto channel = pimpl_->makeSocket(name, c, true);
        return channel;
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
MultiplexedSocket::write(const uint16_t& channel,
                         const uint8_t* buf,
                         std::size_t len,
                         std::error_code& ec)
{
    assert(nullptr != buf);

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

void
MultiplexedSocket::shutdown()
{
    pimpl_->shutdown();
}

void
MultiplexedSocket::join()
{
    pimpl_->join();
}

void
MultiplexedSocket::onShutdown(OnShutdownCb&& cb)
{
    pimpl_->onShutdown_ = std::move(cb);
    if (pimpl_->isShutdown_)
        pimpl_->onShutdown_();
}

void
MultiplexedSocket::monitor() const
{
    auto cert = peerCertificate();
    if (!cert || !cert->issuer)
        return;
    auto userUri = cert->issuer->getId().toString();
    JAMI_DEBUG("- Socket with device: {:s} - account: {:s}", deviceId().to_c_str(), userUri);
    auto now = clock::now();
    JAMI_DEBUG("- Duration: {}",
               std::chrono::duration_cast<std::chrono::milliseconds>(now - pimpl_->start_));
    pimpl_->endpoint->monitor();
    std::lock_guard<std::mutex> lk(pimpl_->socketsMutex);
    for (const auto& [_, channel] : pimpl_->sockets) {
        if (channel)
            JAMI_DEBUG("\t\t- Channel {} (count: {}) with name {:s} Initiator: {}", fmt::ptr(channel.get()), channel.use_count(), channel->name(), channel->isInitiator());
    }
}

void
MultiplexedSocket::sendBeacon(const std::chrono::milliseconds& timeout)
{
    pimpl_->sendBeacon(timeout);
}

std::shared_ptr<dht::crypto::Certificate>
MultiplexedSocket::peerCertificate() const
{
    return pimpl_->endpoint->peerCertificate();
}

#ifdef LIBJAMI_TESTABLE
bool
MultiplexedSocket::canSendBeacon() const
{
    return pimpl_->canSendBeacon_;
}

void
MultiplexedSocket::answerToBeacon(bool value)
{
    pimpl_->answerBeacon_ = value;
}

void
MultiplexedSocket::setVersion(int version)
{
    pimpl_->version_ = version;
}

void
MultiplexedSocket::setOnBeaconCb(const std::function<void(bool)>& cb)
{
    pimpl_->onBeaconCb_ = cb;
}

void
MultiplexedSocket::setOnVersionCb(const std::function<void(int)>& cb)
{
    pimpl_->onVersionCb_ = cb;
}

void
MultiplexedSocket::sendVersion()
{
    pimpl_->sendVersion();
}

IpAddr
MultiplexedSocket::getLocalAddress() const
{
    return pimpl_->endpoint->getLocalAddress();
}

IpAddr
MultiplexedSocket::getRemoteAddress() const
{
    return pimpl_->endpoint->getRemoteAddress();
}

#endif

void
MultiplexedSocket::eraseChannel(uint16_t channel)
{
    std::lock_guard<std::mutex> lkSockets(pimpl_->socketsMutex);
    auto itSocket = pimpl_->sockets.find(channel);
    if (pimpl_->sockets.find(channel) != pimpl_->sockets.end())
        pimpl_->sockets.erase(itSocket);
}

////////////////////////////////////////////////////////////////

class ChannelSocket::Impl
{
public:
    Impl(std::weak_ptr<MultiplexedSocket> endpoint,
         const std::string& name,
         const uint16_t& channel,
         bool isInitiator,
         std::function<void()> rmFromMxSockCb)
        : name(name)
        , channel(channel)
        , endpoint(std::move(endpoint))
        , isInitiator_(isInitiator)
        , rmFromMxSockCb_(std::move(rmFromMxSockCb))
    {}

    ~Impl() {}

    ChannelReadyCb readyCb_ {};
    OnShutdownCb shutdownCb_ {};
    std::atomic_bool isShutdown_ {false};
    std::string name {};
    uint16_t channel {};
    std::weak_ptr<MultiplexedSocket> endpoint {};
    bool isInitiator_ {false};
    std::function<void()> rmFromMxSockCb_;

    bool isAnswered_ {false};
    bool isRemovable_ {false};

    std::vector<uint8_t> buf {};
    std::mutex mutex {};
    std::condition_variable cv {};
    GenericSocket<uint8_t>::RecvCb cb {};
};

ChannelSocketTest::ChannelSocketTest(const DeviceId& deviceId,
                                     const std::string& name,
                                     const uint16_t& channel)
    : pimpl_deviceId(deviceId)
    , pimpl_name(name)
    , pimpl_channel(channel)
    , ioCtx_(*Manager::instance().ioContext())
/*, eventLoopThread_ {[this] {
    try {
        eventLoop();
    } catch (const std::exception& e) {
        JAMI_ERR() << "[CNX] peer connection event loop failure: " << e.what();
        shutdown();
    }
}}*/
{}

ChannelSocketTest::~ChannelSocketTest()
{
    // eventLoopThread_.join();
}

void
ChannelSocketTest::link(const std::shared_ptr<ChannelSocketTest>& socket1,
                        const std::shared_ptr<ChannelSocketTest>& socket2)
{
    socket1->remote = socket2;
    socket2->remote = socket1;
}

DeviceId
ChannelSocketTest::deviceId() const
{
    return pimpl_deviceId;
}

std::string
ChannelSocketTest::name() const
{
    return pimpl_name;
}

uint16_t
ChannelSocketTest::channel() const
{
    return pimpl_channel;
}

void
ChannelSocketTest::shutdown()
{
    {
        std::unique_lock<std::mutex> lk {mutex};
        if (!isShutdown_.exchange(true)) {
            lk.unlock();
            shutdownCb_();
        }
        cv.notify_all();
    }

    if (auto peer = remote.lock()) {
        if (!peer->isShutdown_.exchange(true)) {
            peer->shutdownCb_();
        }
        peer->cv.notify_all();
    }
}

std::size_t
ChannelSocketTest::read(ValueType* buf, std::size_t len, std::error_code& ec)
{
    std::size_t size = std::min(len, this->rx_buf.size());

    for (std::size_t i = 0; i < size; ++i)
        buf[i] = this->rx_buf[i];

    if (size == this->rx_buf.size()) {
        this->rx_buf.clear();
    } else
        this->rx_buf.erase(this->rx_buf.begin(), this->rx_buf.begin() + size);
    return size;
}

std::size_t
ChannelSocketTest::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (isShutdown_) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    ec = {};
    dht::ThreadPool::computation().run(
        [r = remote, data = std::vector<uint8_t>(buf, buf + len)]() mutable {
            if (auto peer = r.lock())
                peer->onRecv(std::move(data));
        });
    return len;
}

int
ChannelSocketTest::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    std::unique_lock<std::mutex> lk {mutex};
    cv.wait_for(lk, timeout, [&] { return !rx_buf.empty() or isShutdown_; });
    return rx_buf.size();
}

void
ChannelSocketTest::setOnRecv(RecvCb&& cb)
{
    std::lock_guard<std::mutex> lkSockets(mutex);
    this->cb = std::move(cb);
    if (!rx_buf.empty() && this->cb) {
        this->cb(rx_buf.data(), rx_buf.size());
        rx_buf.clear();
    }
}

void
ChannelSocketTest::onRecv(std::vector<uint8_t>&& pkt)
{
    std::lock_guard<std::mutex> lkSockets(mutex);
    if (cb) {
        cb(pkt.data(), pkt.size());
        return;
    }
    // JAMI_WARNING("{} Buffering {} bytes", fmt::ptr(this), pkt.size());
    rx_buf.insert(rx_buf.end(),
                  std::make_move_iterator(pkt.begin()),
                  std::make_move_iterator(pkt.end()));
    cv.notify_all();
}

void
ChannelSocketTest::onReady(ChannelReadyCb&& cb)
{}

void
ChannelSocketTest::onShutdown(OnShutdownCb&& cb)
{
    std::unique_lock<std::mutex> lk {mutex};
    shutdownCb_ = std::move(cb);

    // JAMI_ERROR("ONSHUTDOWNCALLBACK FOR DEVICE: {}", deviceId().toString());
    if (isShutdown_) {
        lk.unlock();
        shutdownCb_();
    }
}

ChannelSocket::ChannelSocket(std::weak_ptr<MultiplexedSocket> endpoint,
                             const std::string& name,
                             const uint16_t& channel,
                             bool isInitiator,
                             std::function<void()> rmFromMxSockCb)
    : pimpl_ {std::make_unique<Impl>(endpoint, name, channel, isInitiator, std::move(rmFromMxSockCb))}
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
    // Note. Is initiator here as not the same meaning of MultiplexedSocket.
    // because a multiplexed socket can have sockets from accepted requests
    // or made via connectDevice(). Here, isInitiator_ return if the socket
    // is from connectDevice.
    return pimpl_->isInitiator_;
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
    std::lock_guard<std::mutex> lkSockets(pimpl_->mutex);
    pimpl_->cb = std::move(cb);
    if (!pimpl_->buf.empty() && pimpl_->cb) {
        pimpl_->cb(pimpl_->buf.data(), pimpl_->buf.size());
        pimpl_->buf.clear();
    }
}

void
ChannelSocket::onRecv(std::vector<uint8_t>&& pkt)
{
    std::lock_guard<std::mutex> lkSockets(pimpl_->mutex);
    if (pimpl_->cb) {
        pimpl_->cb(&pkt[0], pkt.size());
        return;
    }
    pimpl_->buf.insert(pimpl_->buf.end(),
                       std::make_move_iterator(pkt.begin()),
                       std::make_move_iterator(pkt.end()));
    pimpl_->cv.notify_all();
}

#ifdef LIBJAMI_TESTABLE
std::shared_ptr<MultiplexedSocket>
ChannelSocket::underlyingSocket() const
{
    if (auto mtx = pimpl_->endpoint.lock())
        return mtx;
    return {};
}
#endif

void
ChannelSocket::answered()
{
    pimpl_->isAnswered_ = true;
}

void
ChannelSocket::removable()
{
    pimpl_->isRemovable_ = true;
}

bool
ChannelSocket::isRemovable() const
{
    return pimpl_->isRemovable_;
}

bool
ChannelSocket::isAnswered() const
{
    return pimpl_->isAnswered_;
}

void
ChannelSocket::ready()
{
    if (pimpl_->readyCb_)
        pimpl_->readyCb_();
}

void
ChannelSocket::stop()
{
    if (pimpl_->isShutdown_)
        return;
    pimpl_->isShutdown_ = true;
    if (pimpl_->shutdownCb_)
        pimpl_->shutdownCb_();
    pimpl_->cv.notify_all();
    // stop() can be called by ChannelSocket::shutdown()
    // In this case, the eventLoop is not used, but MxSock
    // must remove the channel from its list (so that the
    // channel can be destroyed and its shared_ptr invalidated).
    if (pimpl_->rmFromMxSockCb_)
        pimpl_->rmFromMxSockCb_();
}

void
ChannelSocket::shutdown()
{
    if (pimpl_->isShutdown_)
        return;
    stop();
    if (auto ep = pimpl_->endpoint.lock()) {
        std::error_code ec;
        const uint8_t dummy = '\0';
        ep->write(pimpl_->channel, &dummy, 0, ec);
    }
}

std::size_t
ChannelSocket::read(ValueType* outBuf, std::size_t len, std::error_code& ec)
{
    std::lock_guard<std::mutex> lkSockets(pimpl_->mutex);
    std::size_t size = std::min(len, pimpl_->buf.size());

    for (std::size_t i = 0; i < size; ++i)
        outBuf[i] = pimpl_->buf[i];

    pimpl_->buf.erase(pimpl_->buf.begin(), pimpl_->buf.begin() + size);
    return size;
}

std::size_t
ChannelSocket::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (pimpl_->isShutdown_) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
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
    std::unique_lock<std::mutex> lk {pimpl_->mutex};
    pimpl_->cv.wait_for(lk, timeout, [&] { return !pimpl_->buf.empty() or pimpl_->isShutdown_; });
    return pimpl_->buf.size();
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
ChannelSocket::onReady(ChannelReadyCb&& cb)
{
    pimpl_->readyCb_ = std::move(cb);
}

void
ChannelSocket::sendBeacon(const std::chrono::milliseconds& timeout)
{
    if (auto ep = pimpl_->endpoint.lock()) {
        ep->sendBeacon(timeout);
    } else {
        shutdown();
    }
}

std::shared_ptr<dht::crypto::Certificate>
ChannelSocket::peerCertificate() const
{
    if (auto ep = pimpl_->endpoint.lock())
        return ep->peerCertificate();
    return {};
}

IpAddr
ChannelSocket::getLocalAddress() const
{
    if (auto ep = pimpl_->endpoint.lock())
        return ep->getLocalAddress();
    return {};
}

IpAddr
ChannelSocket::getRemoteAddress() const
{
    if (auto ep = pimpl_->endpoint.lock())
        return ep->getRemoteAddress();
    return {};
}

} // namespace jami
