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
    // Blocking method
    if (::connect(sock_, addr_, addr_.getLength()) < 0)
        throw std::system_error(errno, std::system_category());
    RING_WARN() << "tcp socket connected to " << addr_.toString(true, true);
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
        throw std::system_error(errno, std::system_category());
    buffer.resize(res);
}

char
TcpSocketEndpoint::readchar() const
{
    char c = EOF;
    auto res = ::recv(sock_, &c, 1, 0);
    if (res < 0)
        throw std::system_error(errno, std::system_category());
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
        throw std::system_error(errno, std::system_category());
}

void
TcpSocketEndpoint::writeline(const char* const buffer, std::size_t length)
{
    if (::write(sock_, buffer, length) < 0)
        throw std::system_error(errno, std::system_category());
    if (::write(sock_, "\n", 1) < 0)
        throw std::system_error(errno, std::system_category());
}

void
TcpSocketEndpoint::writeline(const std::vector<char>& buffer)
{
    writeline(&buffer[0], buffer.size());
}

//==============================================================================

enum class CtrlMsgType
{
    DUMMY,
    ATTACH_INPUT,
    ATTACH_OUTPUT,
    REFUSE,
    ABORT,
    STOP,
};

struct CtrlMsg
{
    virtual CtrlMsgType type() const { return CtrlMsgType::DUMMY; }
    virtual ~CtrlMsg() = default;
};

struct AttachInputCtrlMsg : public CtrlMsg
{
    explicit AttachInputCtrlMsg(const std::shared_ptr<Stream>& stream)
        : CtrlMsg ()
        , stream {stream} {}
    CtrlMsgType type() const override { return CtrlMsgType::ATTACH_INPUT; }
    const std::shared_ptr<Stream> stream;
};

struct AttachOutputCtrlMsg : public CtrlMsg
{
    explicit AttachOutputCtrlMsg(const std::shared_ptr<Stream>& stream)
        : CtrlMsg ()
        , stream {stream} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::ATTACH_OUTPUT; }
    const std::shared_ptr<Stream> stream;
};

struct RefuseCtrlMsg : public CtrlMsg
{
    explicit RefuseCtrlMsg(const DRing::DataTransferId& id) : CtrlMsg (), id {id} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::REFUSE; }
    const DRing::DataTransferId id;
};

struct AbortCtrlMsg : public CtrlMsg
{
    explicit AbortCtrlMsg(const DRing::DataTransferId& id) : CtrlMsg (), id {id} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::ABORT; }
    const DRing::DataTransferId id;
};

struct StopCtrlMsg : public CtrlMsg
{
    explicit StopCtrlMsg() : CtrlMsg () {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::STOP; }
};

//==============================================================================

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
            changeState(ConnectionState::CONNECTED);
        }

    const Account& account;
    const std::string peer_uri;
    std::unique_ptr<ConnectionEndpoint> endpoint;

    std::atomic<ConnectionState> state {ConnectionState::CREATED};

    Channel<std::unique_ptr<CtrlMsg>> ctrlChannel;
    std::map<DRing::DataTransferId, std::shared_ptr<Stream>> inputs;
    std::map<DRing::DataTransferId, std::shared_ptr<Stream>> outputs;

    std::future<void> eventLoopFut;

    void changeState(ConnectionState);

    void eventLoop();
};

void
PeerConnection::PeerConnectionImpl::changeState(ConnectionState new_state)
{
    ConnectionState old_state;
    do {
        old_state = state.load();
        if (old_state == new_state ||
            old_state == ConnectionState::FAILED ||
            old_state == ConnectionState::CLOSED)
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
        while (true) {
            std::unique_ptr<CtrlMsg> msg;
            if (outputs.empty() and inputs.empty()) {
                ctrlChannel >> msg;
            } else if (!ctrlChannel.empty()) {
                msg = ctrlChannel.receive();
            } else
                break;

            switch (msg->type()) {
                case CtrlMsgType::ATTACH_INPUT:
                {
                    auto& input_msg = static_cast<AttachInputCtrlMsg&>(*msg);
                    auto id = input_msg.stream->getId();
                    inputs.emplace(id, std::move(input_msg.stream));
                }
                break;

                case CtrlMsgType::ATTACH_OUTPUT:
                {
                    auto& output_msg = static_cast<AttachOutputCtrlMsg&>(*msg);
                    auto id = output_msg.stream->getId();
                    outputs.emplace(id, std::move(output_msg.stream));
                }
                break;

                case CtrlMsgType::REFUSE:
                {
                    //auto& refuse_msg = static_cast<RefuseCtrlMsg&>(*msg);
                    // TODO
                }
                break;

                case CtrlMsgType::ABORT:
                {
                    //auto& abort_msg = static_cast<AbortCtrlMsg&>(*msg);
                    // TODO
                }
                break;

                case CtrlMsgType::STOP:
                {
                    return;
                }
                break;

                default: break;
            }
        }

        // Process output streams
        if (!outputs.empty()) {
            const auto& item = std::begin(outputs);
            auto& stream = item->second;
            std::vector<char> buf(3000);
            try {
                endpoint->read(buf);
                if (buf.size() == 0 or !stream->write(buf)) {
                    RING_DBG() << "EOF on output stream #" << stream->getId();
                    stream->close();
                    outputs.erase(item);
                }
            } catch (const std::system_error& e) {
                RING_WARN() << "output #" << stream->getId()
                            << " IO failed with code = " << e.code();
                stream->close();
                outputs.erase(item);
            } catch (const std::exception& e) {
                RING_ERR() << "Unexpected exception during IO with output stream #" << stream->getId()
                           << ": " << e.what();
                stream->close();
                outputs.erase(item);
            }
        }

        // Process input streams
        if (!inputs.empty()) {
            const auto& item = std::begin(inputs);
            auto& stream = item->second;
            std::vector<char> buf(3000);
            try {
                if (stream->read(buf)) {
                    endpoint->write(buf);
                } else {
                    RING_DBG() << "EOF on input stream #" << stream->getId();
                    stream->close();
                    inputs.erase(item);
                }
            }  catch (const std::system_error& e) {
                if (e.code() == std::errc::connection_reset) {
                    RING_WARN() << "input #" << stream->getId()
                                << ": connection reset by peer ";
                } else {
                    RING_WARN() << "input #" << stream->getId()
                                << ": IO failed with code = " << e.code();
                }
                stream->close();
                inputs.erase(item);
            } catch (const std::exception& e) {
                RING_ERR() << "Unexpected exception during IO with input stream #" << stream->getId()
                           << ": " << e.what();
                stream->close();
                outputs.erase(item);
            }
        }
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
    if (pimpl_->eventLoopFut.valid())
        pimpl_->eventLoopFut.wait();
}

void
PeerConnection::close()
{
    pimpl_->endpoint->close();
    pimpl_->inputs.clear();
    pimpl_->outputs.clear();
    pimpl_->ctrlChannel << std::make_unique<StopCtrlMsg>();
    pimpl_->changeState(ConnectionState::CLOSED);
}

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

void
PeerConnection::refuseStream(const DRing::DataTransferId& id)
{
    pimpl_->ctrlChannel << std::make_unique<RefuseCtrlMsg>(id);
}

void
PeerConnection::abortStream(const DRing::DataTransferId& id)
{
    pimpl_->ctrlChannel << std::make_unique<AbortCtrlMsg>(id);
}

} // namespace ring
