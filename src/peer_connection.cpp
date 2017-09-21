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

#include "dring/configurationmanager_interface.h"
#include "dring/peer_connection_const.h"
#include "client/ring_signal.h"

#include "account.h"
#include "string_utils.h"
#include "rw_mutex.h"
#include "channel.h"

#include <algorithm>
#include <future>
#include <map>
#include <atomic>
#include <mutex>

namespace ring
{

/**
 * SendRequest --(on response)--> connect relayAddr --(connected)-----> waitForStreamEvent
 * ---(on stream offert)--> send meta --(on start)--------------------------------------^
 * +--(on stream started)--> send data -------------------------------------------------^
 * +--(on rx stream meta)--> signal client --(on client accept)--> consumer/tx 'start'--^
 * +--(on rx stream data)--> write data into consumer ----------------------------------^
 */

using namespace DRing::PeerConnection;
using lock = std::lock_guard<std::mutex>;

class Stream {
public:
    Stream() = default;
    Stream(const std::string& id, std::unique_ptr<ContentProvider> provider = nullptr);

    Stream(Stream&&) = default;
    Stream& operator =(Stream&&) = default;

    bool isProcessable() const;
    bool isClosed() const;

    void start(std::unique_ptr<ContentConsumer>);
    void stop();
    void toggle_pause();

    void run_once(ConnectionEndpoint&);

    const std::string id;

private:
    std::unique_ptr<ContentProvider> provider_;
    std::unique_ptr<ContentConsumer> consumer_;

    friend std::ostream& operator<<(std::ostream& os, const Stream& stream);
};

std::ostream&
operator<<(std::ostream& os, const Stream& stream)
{
    os << "[Stream " << stream.id << "] ";
    return os;
}

Stream::Stream(const std::string& id, std::unique_ptr<ContentProvider> provider)
    : id {id}
    , provider_ {std::move(provider)}
{}

bool
Stream::isProcessable() const
{
    return true;
}

bool
Stream::isClosed() const
{
    return true;
}

void
Stream::start(std::unique_ptr<ContentConsumer> consumer)
{
    (void)consumer;
}

void
Stream::stop()
{}

void
Stream::run_once(ConnectionEndpoint& endpoint)
{
    (void)endpoint;
}

//==============================================================================

enum class CtrlMsgType {
    DUMMY,
    ACCEPT,
    REFUSE,
    ABORT,
};

struct CtrlMsg {
    virtual CtrlMsgType type() const { return CtrlMsgType::DUMMY; }
    virtual ~CtrlMsg() = default;
};

struct StreamCtrlMsg : public CtrlMsg {
    explicit StreamCtrlMsg(const std::string& id) : id {id} {}
    const std::string id;
};

struct AcceptCtrlMsg : public StreamCtrlMsg {
    AcceptCtrlMsg(const std::string& id, std::unique_ptr<ContentConsumer> consumer)
        : StreamCtrlMsg {id}
        , consumer {std::move(consumer)} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::ACCEPT; }
    std::unique_ptr<ContentConsumer> consumer;
};

struct RefuseCtrlMsg : public StreamCtrlMsg {
    explicit RefuseCtrlMsg(const std::string& id) : StreamCtrlMsg {id} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::REFUSE; }
};

struct AbortCtrlMsg : public StreamCtrlMsg {
    explicit AbortCtrlMsg(const std::string& id) : StreamCtrlMsg {id} {}
    virtual CtrlMsgType type() const override { return CtrlMsgType::ABORT; }
};

class PeerConnection::PeerConnectionImpl {
public:
    PeerConnectionImpl(Account& account, const std::string& peer_uri,
                       std::unique_ptr<ConnectionEndpoint> endpoint)
        : account {account}
        , peer_uri {peer_uri}
        , endpoint {std::move(endpoint)}
        {}

    const Account& account;
    const std::string peer_uri;
    std::unique_ptr<ConnectionEndpoint> endpoint;

    std::atomic<DRing::PeerConnection::ConnectionState> state {ConnectionState::CREATED};

    std::future<void> connectingFut;
    std::future<void> eventLoopFut;

    mutable rw_mutex streamMutex;
    std::map<std::string, Stream> streams;
    unsigned long streams_counter {0}; ///< stream uid generator

    Channel<CtrlMsg> ctrlChan;
    Channel<Stream> streamChan;

    void changeState(ConnectionState new_state);
    void eventLoop();
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

    emitSignal<DRing::PeerConnectionSignal::ConnectionStateChange>(
        account.getAccountID(), peer_uri, int(old_state), int(new_state));

    if (new_state == ConnectionState::CONNECTED)
        eventLoopFut = std::async([this]{ eventLoop(); });
}

void
PeerConnection::PeerConnectionImpl::eventLoop()
{
    while (true) {
        // Append new streams
        Stream stream;
        while (!streamChan.empty())
            streams.emplace(stream.id, streamChan.receive());

        // Process ctrl orders
        while (!ctrlChan.empty()) {
            CtrlMsg msg;

            ctrlChan >> msg;
            try {
                switch (msg.type()) {
                    case CtrlMsgType::ACCEPT:
                    {
                        auto& accept_msg = static_cast<AcceptCtrlMsg&>(msg);
                        streams.at(accept_msg.id).start(std::move(accept_msg.consumer));
                    }
                    break;

                    case CtrlMsgType::REFUSE:
                    {
                        auto& refuse_msg = static_cast<RefuseCtrlMsg&>(msg);
                        streams.at(refuse_msg.id).stop();
                    }
                    break;

                    case CtrlMsgType::ABORT:
                    {
                        auto& abort_msg = static_cast<AbortCtrlMsg&>(msg);
                        streams.at(abort_msg.id).stop();
                    }
                    break;

                    default: break;
                }
            } catch (...) {}
        }

        // Process streams, remove closed ones
        auto item = std::begin(streams);
        while (item != std::end(streams)) {
            auto& st = item->second;
            if (st.isProcessable()) {
                st.run_once(*endpoint);
            } else if (st.isClosed()) {
                streams.erase(item++);
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
    pimpl_->connectingFut.wait();
    pimpl_->eventLoopFut.wait();
    close();
}

void
PeerConnection::close()
{
    pimpl_->endpoint.reset();
    pimpl_->changeState(ConnectionState::CLOSED);
}

std::string
PeerConnection::newStream(std::unique_ptr<ContentProvider> provider)
{
    auto id = to_string(pimpl_->streams_counter++);
    pimpl_->streamChan << Stream {id, std::move(provider)};
    return id;
}

void
PeerConnection::acceptStream(const std::string& id, std::unique_ptr<ContentConsumer> consumer)
{
    pimpl_->ctrlChan << AcceptCtrlMsg {id, std::move(consumer)};
}

void
PeerConnection::refuseStream(const std::string& id)
{
    pimpl_->ctrlChan << RefuseCtrlMsg {id};
}

void
PeerConnection::abortStream(const std::string& id)
{
    pimpl_->ctrlChan << AbortCtrlMsg {id};
}

} // namespace ring
