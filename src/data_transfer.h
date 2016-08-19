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
#include "threadloop.h"
#include "noncopyable.h"
#include "ip_utils.h"

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
#include <unordered_map>

// Forward declarations
namespace ring { namespace tls {
class TlsSession;
}} // namespace ring::tls

namespace ring { namespace ReliableSocket {

class DataConnection;
class DataTransfer;
class DataStream;
class RxQueue;
class TxFrameBuffer;
class TxPacket;
template <class T> class TxScheduler;

using StreamId = uint32_t; // Storage type for a stream id, only lowest 24-bits are used

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
 * A class to carries all congestion control needed information
 * Values are filled by DataConnection class during connection.
 * No more changed after disconnection, reset at each connection.
 */
class CongestionControl {
private:
    friend class DataConnection;

    // These values are computed from low-pass filtered instant values
    std::chrono::microseconds cumRtt_; // cumulative RTT, start at 500ms
    std::chrono::microseconds cumRttVar_; // cumulative RTT variance
    unsigned cumPps_; // cumulative packet that made a round-trip
    unsigned cumLoss_; // cumulative loss packet

    // Cummulative values
    std::size_t txBytes_;
    std::size_t rxBytes_;
    std::size_t rxLossPkt_;
    std::size_t rxBadPkt_;
    std::size_t rxOldPkt_;
    std::size_t rxDupPkt_;

    void reset();
};

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
private:
    NON_COPYABLE(DataConnection);
    DRing::DataConnectionId genUUID();

    friend class DataStream;

public: // ctor/dtor
    DataConnection(const std::string& account_id, const std::string& peer_id, bool is_client);
    ~DataConnection();

public:
    using SeqNum = uint64_t; // Full Sequence Number, 0 is invalid

    std::shared_ptr<DataConnection> sharedPtr() { return shared_from_this(); }

    DRing::DataConnectionId getId() const noexcept { return id_; }
    void getInfo(DRing::DataConnectionInfo& info) const;
    void setStatus(DRing::DataConnectionCode code);

    void stopConnection();
    bool isFailed() const { return false; }
    bool disconnected() const;

    void attachStream(std::shared_ptr<DataStream> stream);
    void detachStream(std::shared_ptr<DataStream> stream);

    // Called by underlaying transport
    void connect(tls::TlsSession* tls);
    void disconnect();

    // Remote peer information
    IpAddr getRemoteAddress() const;

    // Entry point for packets comming from underlaying transport
    void processIncomingPacket(std::vector<uint8_t>&& buf);

    ssize_t addStreamFrame(StreamId sid, const void* buffer, std::size_t len);

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    const DRing::DataConnectionId id_;
    const bool isClient_;
    tls::TlsSession* tls_ {nullptr};
    uint32_t maxPayload_ {0}; // initialized at TLS connection, then negotiated by handshake
    std::atomic<uint32_t> protocol_ {0}; // negotiated communication protocol, 0 is invalid (non negotiated)

    mutable std::mutex infoMutex_ {};
    DRing::DataConnectionInfo info_ {};

private: // Peer info
    uint64_t peerConnectionId_ {0};

private: // Tx related members
    mutable std::mutex txMtx_ {};
    std::unique_ptr<TxScheduler<Clock>> txQueue_;
    friend class TxScheduler<Clock>;

    std::unique_ptr<TxFrameBuffer> txBuffer_;
    SeqNum txSeq_ {1}; // overall sequence number, monotonicaly increased, no wrap
    std::chrono::microseconds txDelay_ {1000};
    std::chrono::microseconds txTargetSpeed_ {1000};
    SeqNum netStatSeq_ {1}; // sequence number to trig network statistics
    SeqNum netStatSent_ {0};
    unsigned netAckCount_ {0};
    Clock::time_point netStatTime_ {};
    std::condition_variable ioCv_ {};
    void waitWrite();

    std::size_t pktSnd_ {0};
    std::size_t pktLate_ {0};
    std::size_t pktForced_ {0};

#ifndef NDEBUG
    unsigned txLossSimu_ {0};
#endif

    std::shared_ptr<TxPacket> nextTxPacket(TimePoint&);
    ssize_t sendPkt(std::vector<uint8_t>&, SeqNum&, SeqNum, unsigned, Clock::time_point&);
    ssize_t sendPkt(std::vector<uint8_t>&, SeqNum&, Clock::time_point&);
    ssize_t sendPkt(std::vector<uint8_t>&);
    void sendProto();
    void sendHeartbeat();
    bool pushPacket(std::shared_ptr<TxPacket> pkt);
    void transmit(std::shared_ptr<TxPacket> pkt);

private: // Tx ack related members
    std::mutex ackMtx_ {};
    SeqNum txAckSeq_ {0}; // last acknowledged sequence number
    unsigned txAckCt_ {0}; // number of contiguous received sequence number below txAckSeq_
    std::atomic<unsigned> delayedAck_ {0}; // number of contiguous ACK not send yet
    std::atomic_bool doAck_ {false};
    Clock::time_point ackTime_ {};

    void sendAck(SeqNum seq, unsigned count);
    void updateTxAckState(SeqNum seq, unsigned count);
    void getTxAckState(SeqNum& seq, unsigned& count);
    void flushDelayedAck();

private: // Rx related members
    static constexpr int MISS_ORDERING_LIMIT = 32; // maximal accepted distance of out-of-order packet

    SeqNum rxSeq_ {0}; // highest received sequence number
    std::bitset<MISS_ORDERING_LIMIT> rxMask_ {}; // out-of-order packets window

    SeqNum rxAckSeq_ {0}; // highest received ACK sequence number
    std::bitset<64> rxAckMask_ {}; // out-of-order ack packets window

    std::mutex ackWaitMapMutex_ {};
    std::map<SeqNum, std::shared_ptr<TxPacket>> ackWaitMap_ {}; // packets waiting for acknowledgment

    std::unique_ptr<RxQueue> rxQueue_;
    friend class RxQueue;

    SeqNum processTransmissionFields(const std::vector<uint8_t>&);
    void processAcknowledgmentFields(const std::vector<uint8_t>&, Clock::time_point);
    void checkForAcknowledgment(SeqNum seq, bool force);
    bool processChannelFlags(SeqNum seq, const std::vector<uint8_t>& pkt, bool& has_data);
    void onAckSeq(SeqNum, Clock::time_point, unsigned count=1);
    void onExpiredSeq(SeqNum, Clock::time_point);

private: // Congestion control and connection statistics
    CongestionControl cc_;
    TimePoint lastStats_ {};
    std::size_t prevTxBytes_ {0};

    CongestionControl getCC() const; // make a safe copy of CongestionControl data
    std::chrono::microseconds getCumRtt() const; // safe CongestionControl.cumRtt_ accessor
    void logStatistics();

private: // Tests
    std::unique_ptr<ThreadLoop> testLoop_;
    void testChannelFlood();
    void testAppSendData();
    void testAppRecvData();
};

extern std::shared_ptr<DataConnection> makeDataConnection(const std::string& account_id,
                                                          const std::string& peer_id,
                                                          bool is_client);

// DataStream
// Hi-level protocol class connecting application streams to low-level data connection.
class DataStream
{
public:
    using Clock = std::chrono::high_resolution_clock;
public:
    DataStream(StreamId sid) : sid_(sid) {}
    ~DataStream();

public: // global API
    StreamId getId() const noexcept { return sid_; }
    void setDataConnection(DataConnection* dc);

public: // application side API
    // send a close packet to peer and detach
    void close() {};

    // Byte-oriented
    ssize_t sendData(const void* buffer, std::size_t buffer_size);
    ssize_t recvData(void* buffer, std::size_t buffer_size);
    bool canRead() const;

    // Message-oriented
    ssize_t sendMsg(const uint8_t* buffer, std::size_t buffer_size);
    ssize_t recvMsg(uint8_t* buffer, std::size_t buffer_size);

private: // DataConnection side API
    friend class RxQueue;
    friend class DataConnection;

    void onAck(std::shared_ptr<TxPacket>);
    void onExpire(std::shared_ptr<TxPacket>);
    void onRxPayload(const uint8_t*, std::size_t, uint64_t);
    void onCloseStream();

private:
    enum FlagsBit {ACK=0, BYTES_AVAILABLE=1};
    StreamId sid_;
    mutable std::mutex mtx_ {};
    std::condition_variable cv_ {};
    std::bitset<2> flags_ {0};
    DataConnection::SeqNum ackSeq_ {0};
    DataConnection* dc_ {nullptr};
    std::list<std::shared_ptr<TxPacket>> ackwait_ {};
    uint64_t offset_ {0};
    uint64_t gapOffset_ {0}; // offset of first byte not received yet
    uint64_t readOffset_ {0}; // offset of first byte not pushed to application
    std::map<uint64_t, std::vector<uint8_t>> reorderBuffer_ {};
};

}} // namespace ring::ReliableSocket
