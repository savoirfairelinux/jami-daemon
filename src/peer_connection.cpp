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

#include "data_transfer.h"
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
#include <cstdio>

namespace ring {

using lock = std::lock_guard<std::mutex>;

static constexpr std::size_t IO_BUFFER_SIZE {3000}; ///< Size of char buffer used by IO operations

//==============================================================================

TcpTurnEndpoint::TcpTurnEndpoint(TurnTransport& turn, const IpAddr& peer)
    : peer_ {peer}, turn_ {turn}
{}

TcpTurnEndpoint::~TcpTurnEndpoint() = default;

void
TcpTurnEndpoint::read(std::vector<char>& buffer) const
{
    turn_.recvfrom(peer_, buffer);
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
        std::system_error(errno, std::generic_category());
    auto bound = ip_utils::getAnyHostAddr(addr.getFamily());
    if (::bind(sock_, bound, bound.getLength()) < 0)
        std::system_error(errno, std::generic_category());
}

TcpSocketEndpoint::~TcpSocketEndpoint()
{
    close();
}

void
TcpSocketEndpoint::connect()
{
    // Blocking method
    if (::connect(sock_, addr_, addr_.getLength()) < 0)
        throw std::system_error(errno, std::generic_category());
}

void
TcpSocketEndpoint::close() noexcept
{
    ::close(sock_);
}

void
TcpSocketEndpoint::read(std::vector<char>& buffer) const
{
    auto res = ::recv(sock_, buffer.data(), buffer.size(), 0);
    if (res < 0)
        throw std::system_error(errno, std::generic_category());
    buffer.resize(res);
}

char
TcpSocketEndpoint::readchar() const
{
    char c = EOF;
    auto res = ::recv(sock_, &c, 1, 0);
    if (res < 0)
        throw std::system_error(errno, std::generic_category());
    return c;
}

std::vector<char>
TcpSocketEndpoint::readline() const
{
    std::vector<char> result;
    while (true) {
        auto c = readchar();
        if (c == '\n' or c == EOF)
            return result;
        result.push_back(c);
    }
}

void
TcpSocketEndpoint::write(const std::vector<char>& buffer)
{
    if (::write(sock_, buffer.data(), buffer.size()) < 0)
        throw std::system_error(errno, std::generic_category());
}

void
TcpSocketEndpoint::writeline(const char* const buffer, std::size_t length)
{
    if (::write(sock_, buffer, length) < 0)
        throw std::system_error(errno, std::generic_category());
    if (::write(sock_, "\n", 1) < 0)
        throw std::system_error(errno, std::generic_category());
}

void
TcpSocketEndpoint::writeline(const std::vector<char>& buffer)
{
    writeline(&buffer[0], buffer.size());
}

//==============================================================================

// following namespace prevents an ODR violation with definitions in p2p.cpp
namespace {

enum class CtrlMsgType
{
    STOP,
    ATTACH_INPUT,
    ATTACH_OUTPUT,
};

struct CtrlMsg
{
    CtrlMsg() {RING_ERR() << "hello DUMMY";}
    virtual CtrlMsgType type() const = 0;
    virtual ~CtrlMsg() = default;
};

struct StopCtrlMsg1 : CtrlMsg
{
    explicit StopCtrlMsg1() : CtrlMsg () { RING_ERR() << "hello STOP"; }
    CtrlMsgType type() const override { return CtrlMsgType::STOP; }
};

struct AttachInputCtrlMsg : CtrlMsg
{
    explicit AttachInputCtrlMsg(const std::shared_ptr<Stream>& stream)
        : CtrlMsg ()
        , stream {stream} {RING_ERR() << "hello INPUT";}
    CtrlMsgType type() const override { return CtrlMsgType::ATTACH_INPUT; }
    const std::shared_ptr<Stream> stream;
};

struct AttachOutputCtrlMsg : CtrlMsg
{
    explicit AttachOutputCtrlMsg(const std::shared_ptr<Stream>& stream)
        : CtrlMsg ()
        , stream {stream} {RING_ERR() << "hello OUTPUT";}
    CtrlMsgType type() const override { return CtrlMsgType::ATTACH_OUTPUT; }
    const std::shared_ptr<Stream> stream;
};

} // namespace <anonymous>

//==============================================================================

class PeerConnection::PeerConnectionImpl
{
public:
    PeerConnectionImpl(Account& account, const std::string& peer_uri,
                       std::unique_ptr<ConnectionEndpoint> endpoint)
        : account {account}
        , peer_uri {peer_uri}
        , endpoint_ {std::move(endpoint)}
        , eventLoopFut_ {std::async(std::launch::async, [this]{ eventLoop();})} {}

    ~PeerConnectionImpl() {
        auto x = std::make_unique<StopCtrlMsg1>();
        RING_ERR() << "close connection " << int(x->type());
        ctrlChannel << std::move(x);
        if (eventLoopFut_.valid())
            eventLoopFut_.wait();
        RING_ERR() << "peer con dead";
    }

    const Account& account;
    const std::string peer_uri;
    Channel<std::unique_ptr<CtrlMsg>> ctrlChannel;

private:
    std::unique_ptr<ConnectionEndpoint> endpoint_;
    std::map<DRing::DataTransferId, std::shared_ptr<Stream>> inputs_;
    std::map<DRing::DataTransferId, std::shared_ptr<Stream>> outputs_;
    std::future<void> eventLoopFut_;

    void eventLoop();

    template <typename L, typename C>
    void handle_stream_list(L& stream_list, const C& callable) {
        if (stream_list.empty())
            return;
        const auto& item = std::begin(stream_list);
        auto& stream = item->second;
        try {
            if (callable(stream))
                return;
            RING_DBG() << "EOF on stream #" << stream->getId();
        } catch (const std::system_error& e) {
            RING_WARN() << "Stream #" << stream->getId()
                        << " IO failed with code = " << e.code();
        } catch (const std::exception& e) {
            RING_ERR() << "Unexpected exception during IO with stream #"
                       << stream->getId()
                       << ": " << e.what();
        }
        stream->close();
        stream_list.erase(item);
    }
};

void
PeerConnection::PeerConnectionImpl::eventLoop()
{
    RING_DBG() << "Peer connection to " << peer_uri << " ready";
    while (true) {
        // Process ctrl orders first
        while (true) {
            std::unique_ptr<CtrlMsg> msg;
            if (outputs_.empty() and inputs_.empty()) {
                ctrlChannel >> msg;
                RING_ERR() << "[BAD] " << int(msg->type());
            } else if (!ctrlChannel.empty()) {
                msg = ctrlChannel.receive();
                RING_ERR();
            } else
                break;

            switch (msg->type()) {
                case CtrlMsgType::ATTACH_INPUT:
                {
                    RING_ERR() << "input";
                    auto& input_msg = static_cast<AttachInputCtrlMsg&>(*msg);
                    auto id = input_msg.stream->getId();
                    inputs_.emplace(id, std::move(input_msg.stream));
                }
                break;

                case CtrlMsgType::ATTACH_OUTPUT:
                {
                    RING_ERR() << "output";
                    auto& output_msg = static_cast<AttachOutputCtrlMsg&>(*msg);
                    auto id = output_msg.stream->getId();
                    outputs_.emplace(id, std::move(output_msg.stream));
                }
                break;

                case CtrlMsgType::STOP:
                    RING_ERR() << "stop";
                    endpoint_.reset();
                    inputs_.clear();
                    outputs_.clear();
                    return;

                default: RING_ERR("BUG: got unhandled control msg!");  break;
            }
        }

        // Then handles IO streams
        std::vector<char> buf(IO_BUFFER_SIZE);
        handle_stream_list(inputs_, [&](auto& stream){
                if (!stream->read(buf))
                    return false;
                endpoint_->write(buf);
                return true;
            });
        handle_stream_list(outputs_, [&](auto& stream){
                endpoint_->read(buf);
                return buf.size() != 0 and stream->write(buf);
            });
    }
}

//==============================================================================

PeerConnection::PeerConnection(Account& account, const std::string& peer_uri,
                               std::unique_ptr<ConnectionEndpoint> endpoint)
    : pimpl_(std::make_unique<PeerConnectionImpl>(account, peer_uri, std::move(endpoint)))
{}

PeerConnection::~PeerConnection()
{
    close();
}

void
PeerConnection::close()
{}

void
PeerConnection::attachInputStream(const std::shared_ptr<Stream>& stream)
{
    pimpl_->ctrlChannel << std::make_unique<AttachInputCtrlMsg>(stream);
}

void
PeerConnection::attachOutputStream(const std::shared_ptr<Stream>& stream)
{
    pimpl_->ctrlChannel << std::make_unique<AttachOutputCtrlMsg>(stream);
}

} // namespace ring
