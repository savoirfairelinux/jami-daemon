/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "peer_connection.h"

#include "account.h"
#include "string_utils.h"
#include "channel.h"
#include "turn_transport.h"

#include <algorithm>
#include <future>
#include <map>
#include <atomic>
#include <stdexcept>
#include <istream>
#include <ostream>
#include <unistd.h>

namespace ring {

/**
 * SendRequest --(on response)--> connect relayAddr --(connected)-----> waitForStreamEvent
 * ---(on stream offert)--> send meta --(on start)--------------------------------------^
 * +--(on stream started)--> send data -------------------------------------------------^
 * +--(on rx stream meta)--> signal client --(on client accept)--> consumer/tx 'start'--^
 * +--(on rx stream data)--> write data into consumer ----------------------------------^
 */

using lock = std::lock_guard<std::mutex>;

enum class ConnectionState : int
{
    CREATED, // start point
    CONNECTED,
    CLOSED, // end point
    FAILED, // end point
};

//==============================================================================

TcpTurnEndpoint::TcpTurnEndpoint(TurnTransport& turn, const IpAddr& peer)
    : peer_ {peer}, turn_ {turn}
{}

TcpTurnEndpoint::~TcpTurnEndpoint() = default;

std::vector<char>
TcpTurnEndpoint::read()
{
    std::map<IpAddr, std::vector<char>> map;
    while (true) {
        turn_.recvfrom(map);
        if (map.find(peer_) != std::end(map))
            return map.at(peer_);
    }
}

void
TcpTurnEndpoint::write(const std::vector<char>& buffer)
{
    turn_.sendto(peer_, buffer);
}

//==============================================================================

TcpSocketEndpoint::TcpSocketEndpoint(const IpAddr& addr)
    : addr_ {addr}
    , sock_ {::socket(addr.getFamily(), SOCK_STREAM, 0)}
{
    if (sock_ < 0)
        std::system_error(errno, std::system_category());
    auto bound = ip_utils::getAnyHostAddr(addr.getFamily());
    if (::bind(sock_, bound, bound.getLength()) < 0)
        std::system_error(errno, std::system_category());
}

TcpSocketEndpoint::~TcpSocketEndpoint()
{
    close();
}

void
TcpSocketEndpoint::connect()
{
    if (::connect(sock_, addr_, addr_.getLength()) < 0)
        throw std::system_error(errno, std::system_category());
}

void
TcpSocketEndpoint::close() noexcept
{
    ::close(sock_);
}

std::vector<char>
TcpSocketEndpoint::read()
{
    std::vector<char> result(buffer_size);
    while (true) {
        auto res = ::recv(sock_, result.data(), result.size(), 0);
        if (res < 0)
            std::system_error(errno, std::system_category());
        if (res > 0) {
            result.resize(res);
            return result;
        }
    }
}

void
TcpSocketEndpoint::write(const std::vector<char>& buffer)
{
    if (::write(sock_, buffer.data(), buffer.size()) < 0)
        std::system_error(errno, std::system_category());
}

//==============================================================================

enum class CtrlMsgType
{
    DUMMY,
    NEW_INPUT,
    NEW_OUTPUT,
    REFUSE,
    ABORT,
};

struct CtrlMsg
{
    virtual CtrlMsgType type() const { return CtrlMsgType::DUMMY; }
    virtual ~CtrlMsg() = default;
};

struct StreamCtrlMsg : public CtrlMsg
{
    explicit StreamCtrlMsg(const std::string& id) : id {id} {}
    const std::string id;
};

struct NewInputCtrlMsg : public StreamCtrlMsg
{
    NewInputCtrlMsg(const std::string& id, std::unique_ptr<std::istream> stream)
        : StreamCtrlMsg {id}
        , stream {std::move(stream)} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::NEW_INPUT; }
    std::unique_ptr<std::istream> stream;
};

struct NewOutputCtrlMsg : public StreamCtrlMsg
{
    NewOutputCtrlMsg(const std::string& id, std::unique_ptr<std::ostream> stream)
        : StreamCtrlMsg {id}
        , stream {std::move(stream)} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::NEW_OUTPUT; }
    std::unique_ptr<std::ostream> stream;
};

struct RefuseCtrlMsg : public StreamCtrlMsg
{
    explicit RefuseCtrlMsg(const std::string& id) : StreamCtrlMsg {id} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::REFUSE; }
};

struct AbortCtrlMsg : public StreamCtrlMsg
{
    explicit AbortCtrlMsg(const std::string& id) : StreamCtrlMsg {id} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::ABORT; }
};

class PeerConnection::PeerConnectionImpl
{
public:
    static constexpr std::size_t MAX_PACKET_SIZE {3000};

    PeerConnectionImpl(Account& account, const std::string& peer_uri,
                       std::unique_ptr<ConnectionEndpoint> endpoint)
        : account {account}
        , peer_uri {peer_uri}
        , endpoint {std::move(endpoint)} {
            RING_DBG() << "Peer connection to " << peer_uri << " ready";
        }

    const Account& account;
    const std::string peer_uri;
    std::unique_ptr<ConnectionEndpoint> endpoint;

    std::atomic<ConnectionState> state {ConnectionState::CREATED};

    Channel<CtrlMsg> ctrlChannel;
    std::atomic<unsigned long> streamCounter {0}; ///< stream uid generator
    std::map<std::string, std::unique_ptr<std::istream>> inputs;
    std::map<std::string, std::unique_ptr<std::ostream>> outputs;

    std::future<void> eventLoopFut;

    void changeState(ConnectionState);

    void eventLoop();

    void sendClose(const std::string&) {}

    void sendData(const std::vector<char>& data) {
        endpoint->write(data);
    }
};

void
PeerConnection::PeerConnectionImpl::changeState(ConnectionState new_state)
{
    ConnectionState old_state;
    do {
        old_state = state.load();
        if (old_state == new_state || old_state == ConnectionState::FAILED || old_state == ConnectionState::CLOSED)
            return;
    } while (!state.compare_exchange_weak(old_state, new_state));

    if (new_state == ConnectionState::CONNECTED)
        eventLoopFut = std::async([this]{ eventLoop(); });
}

void
PeerConnection::PeerConnectionImpl::eventLoop()
{
    while (true) {
        // Process ctrl orders
        while (!ctrlChannel.empty()) {
            CtrlMsg msg;
            ctrlChannel >> msg;
            switch (msg.type()) {
                case CtrlMsgType::NEW_INPUT:
                {
                    auto& input_msg = static_cast<NewInputCtrlMsg&>(msg);
                    inputs.emplace(input_msg.id, std::move(input_msg.stream));
                }
                break;

                case CtrlMsgType::NEW_OUTPUT:
                {
                    auto& output_msg = static_cast<NewOutputCtrlMsg&>(msg);
                    outputs.emplace(output_msg.id, std::move(output_msg.stream));
                }
                break;

                case CtrlMsgType::REFUSE:
                {
                    //auto& refuse_msg = static_cast<RefuseCtrlMsg&>(msg);
                    // TODO
                }
                break;

                case CtrlMsgType::ABORT:
                {
                    //auto& abort_msg = static_cast<AbortCtrlMsg&>(msg);
                    // TODO
                }
                break;

                default: break;
            }
        }

        // Process streams, remove closed ones
        auto item = std::begin(inputs);
        while (item != std::end(inputs)) {
            const auto& id = item->first;
            auto& stream = *item->second;
            std::vector<char> buf(MAX_PACKET_SIZE);
            if (stream.read(&buf[0], buf.size())) {
                sendData(buf);
            }

            if (stream.eof()) {
                sendClose(id);
                inputs.erase(item++);
                continue;
            }

            ++item;
        }
    }
}

//==============================================================================

PeerConnection::PeerConnection(Account& account, const std::string& peer_uri,
                               std::unique_ptr<ConnectionEndpoint> endpoint)
    : pimpl_(new PeerConnectionImpl{account, peer_uri, std::move(endpoint)})
{}

PeerConnection::~PeerConnection()
{
    close();
    if (pimpl_->eventLoopFut.valid())
        pimpl_->eventLoopFut.wait();
}

void
PeerConnection::close()
{
    pimpl_->endpoint.reset();
    pimpl_->changeState(ConnectionState::CLOSED);
}

std::string
PeerConnection::newInputStream(std::unique_ptr<std::istream> stream)
{
    auto id = to_string(pimpl_->streamCounter++);
    pimpl_->ctrlChannel << NewInputCtrlMsg {id, std::move(stream)};
    return id;
}

void
PeerConnection::newOutputStream(std::unique_ptr<std::ostream> stream,
                                const std::string& id)
{
    pimpl_->ctrlChannel << NewOutputCtrlMsg {id, std::move(stream)};
}

void
PeerConnection::refuseStream(const std::string& id)
{
    pimpl_->ctrlChannel << RefuseCtrlMsg {id};
}

void
PeerConnection::abortStream(const std::string& id)
{
    pimpl_->ctrlChannel << AbortCtrlMsg {id};
}

} // namespace ring
