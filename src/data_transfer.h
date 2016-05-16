/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

#pragma once

#include "datatransfer_interface.h"
#include "noncopyable.h"

#include <string>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <ios>
#include <vector>
#include <list>
#include <functional>
#include <utility>
#include <atomic>
#include <thread>
#include <chrono>
#include <array>
#include <bitset>

// Forward declarations
namespace ring { namespace tls {
class TlsSession;
}} // namespace ring::tls

namespace ring { namespace ReliableSocket {

class DataConnection;
class DataTransfer;
class LossRecorder;
class RxBuffer;
class TxDataBuffer;
class RxQueue;
template <class T> class TxScheduler;

static constexpr uint32_t PROTOCOL_VERSION {0xABADCAFE};

template<typename T>
struct Timestamp : T
{
    static T get_now() {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<T>(now);
    }

    Timestamp() : T(get_now()) {}

    inline bool newer(const T& other) const {
        return other < *this;
    }

    inline bool older(const T& other) const {
        return other > *this;
    }
};

// Packet headers must be 32bit aligned and always made of PACKET_HEADER_WORDS 32bit words
// Control Field are also 32bit aligned, but not payload data.

static constexpr unsigned PACKET_HEADER_WORDS {2};
static constexpr unsigned PACKET_HEADER_BYTES {4 * PACKET_HEADER_WORDS};

/**
 * A Channel instance represents a low-level link between local and remote peer.
 * It's build over the transport protocol and manage all packets coming from.
 * All application streams are build over a channel.
 * The existance of a channel depends on the packet sequence number. This number
 * starts at 1 and cannot overlap. Befor its maximal limit is reach, all application streams
 * are encouraged to migrate on another channel, or be definitively closed (that will be forced
 * by the channel protocol if the maximal packet sequence number is reached).
 */

class DataConnection : public std::enable_shared_from_this<DataConnection>
{
public:
    static std::shared_ptr<DataConnection> getDataConnection(const DRing::DataConnectionId& tid);

    DataConnection(const std::string& account_id, const std::string& peer_id, bool is_client);
    ~DataConnection();

    DRing::DataConnectionId getId() const noexcept { return id_; }
    void getInfo(DRing::DataTransferInfo& info) const;
    void setStatus(DRing::DataTransferCode code);

    void stopConnection();

    // Called by underlaying transport
    void connect(tls::TlsSession* tls);
    void disconnect();

    // Entry point for packets comming from underlaying transport
    void processPacket(std::vector<uint8_t>&& buf);

    // Application use this to send data to the remote side
    bool sendData(const std::vector<uint8_t>& s);

private:
    using SeqNum = uint64_t;
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    NON_COPYABLE(DataConnection);

    const DRing::DataConnectionId id_;
    const bool isClient_;
    tls::TlsSession* tls_ {nullptr};
    uint32_t maxPayload_ {0}; // initialized at TLS connection, then negotiated by handshake
    std::atomic<uint32_t> protocol_ {0}; // negotiated communication protocol, 0 is invalid (non negotiated)

    mutable std::mutex infoMutex_ {};
    DRing::DataConnectionInfo info_ {};

    const Clock::time_point startTime_ {};

private: // Statistics
    std::mutex statMutex_ {};
    Clock::time_point statSync_;
    std::atomic<std::size_t> rxPkt_ {0};
    std::atomic<std::size_t> rxBytes_ {0};
    std::atomic<std::size_t> rxLossPkt_ {0};

    // Sum of rxBadPkt_, rxOldPkt_ and rxDupPkt_ gives number of dropped packets at reception
    std::atomic<std::size_t> rxBadPkt_ {0};
    std::atomic<std::size_t> rxOldPkt_ {0};
    std::atomic<std::size_t> rxDupPkt_ {0};

    void idleJob();

private: // Peer info
    uint64_t peerConnectionId_ {0};

private: // Tx related members
    std::mutex txMtx_ {};
    SeqNum txSeq_ {1}; // overall sequence number, monotonicaly increased, no wrap
    SeqNum txAckSeq_ {0}; // last acknowledged sequence number
    int txAckCt_ {0}; // number of contiguous received sequence number below txAckSeq_
    std::chrono::microseconds txDelay_ {100000};
    std::unique_ptr<TxScheduler<Clock>> txQueue_;
    Clock::time_point lastHeartbeat_;

#ifndef NDEBUG
    unsigned txLossSimu_ {0};
#endif

    std::shared_ptr<std::vector<uint8_t>> nextTxPacket(TimePoint& when);
    ssize_t sendPkt(std::vector<uint8_t>&);
    void sendProto();
    void sendHeartbeat();

private: // Rx related members
    static constexpr int MISS_ORDERING_LIMIT = 32; // Maximal accepted distance of out-of-order packet

    SeqNum rxSeq_ {0}; // highest received sequence number
    std::bitset<MISS_ORDERING_LIMIT> rxMask_ {}; // out-of-order packets window

    SeqNum rxAckSeq_ {0}; // highest received ACK sequence number
    std::bitset<MISS_ORDERING_LIMIT> rxAckMask_ {}; // out-of-order ack packets window

#if 0
private: // Tx related members
    std::unique_ptr<TxDataBuffer> txBuffer_;
    std::unique_ptr<LossRecorder> txLossList_;

private: // Rx related members
    std::unique_ptr<RxQueue> rxQueue_;
    std::unique_ptr<RxBuffer> rxBuffer_;
    std::unique_ptr<LossRecorder> rxLossList_;
#endif

private:
    friend std::shared_ptr<DataConnection> makeDataConnection(const std::string& account_id,
                                                              const std::string& peer_id,
                                                              bool is_client);
    friend class TxScheduler<Clock>;
};

std::shared_ptr<DataConnection> makeDataConnection(const std::string& account_id,
                                                   const std::string& peer_id,
                                                   bool is_client);

class FileTransfer
{
public:
    static DRing::DataTransferId newFileTransfer(const DRing::DataConnectionId& cid,
                                                 const std::string& name,
                                                 std::ifstream&& stream);
};

}} // namespace ring::ReliableSocket
