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

// Forward declarations
namespace ring { namespace tls {
class TlsSession;
}} // namespace ring::tls

namespace ring { namespace ReliableSocket {

class DataConnection;
class DataTransfer;
class LossRecorder;
class RxBuffer;
class TxBuffer;
class RxQueue;
template <class T> class TxQueue;

using ChannelId = uint8_t;

static constexpr uint8_t PROTOCOL_VERSION {1};

template<typename T>
struct Timestamp : T {
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

namespace SeqNum
{
using seqnum_t = int32_t;

static constexpr seqnum_t THRESHOLD {0x3FFFFFFF}; // threshold for comparing seq. no.
static constexpr seqnum_t MAX {0x7FFFFFFF}; // maximum sequence number
static constexpr seqnum_t INVALID {-1}; // invalid sequence number

inline static int cmp(seqnum_t seq1, seqnum_t seq2) {
    return (std::abs(seq1 - seq2) < THRESHOLD) ? (seq1 - seq2) : (seq2 - seq1);
}

inline static int offset(seqnum_t seq1, seqnum_t seq2) {
    if (std::abs(seq1 - seq2) < THRESHOLD)
        return seq2 - seq1;

    if (seq1 < seq2)
        return seq2 - seq1 - MAX - 1;

    return seq2 - seq1 + MAX + 1;
}

inline static unsigned length(seqnum_t seq1, seqnum_t seq2) {
    return (seq1 <= seq2) ? (seq2 - seq1 + 1) : (seq2 - seq1 + MAX + 2);
}

inline static seqnum_t increase(seqnum_t seq, seqnum_t inc) {
    return (MAX - seq >= inc) ? seq + inc : seq - MAX + inc - 1;
}

inline static seqnum_t increase(seqnum_t seq) {
    return (seq == MAX) ? 0 : seq + 1;
}

inline static seqnum_t decrease(seqnum_t seq) {
    return (seq == 0) ? MAX : seq - 1;
}

} // namespace SeqNum

// Packet headers must be 32bit aligned and always made of PACKET_HEADER_WORDS 32bit words
// Control Field are also 32bit aligned, but not payload data.

enum class PacketType : uint16_t {
    ACK=0,
    NACK,
    HANDSHAKE,
};

static constexpr unsigned PACKET_HEADER_WORDS {3};
static constexpr unsigned PACKET_HEADER_BYTES {4 * PACKET_HEADER_WORDS};

class DataConnection : public std::enable_shared_from_this<DataConnection>
{
public:
    static std::shared_ptr<DataConnection> getDataConnection(const DRing::DataConnectionId& tid);

    DataConnection(const std::string& account_id, const std::string& peer_id);
    ~DataConnection();

    DRing::DataConnectionId getId() const noexcept { return id_; }
    void getInfo(DRing::DataTransferInfo& info) const;
    void setStatus(DRing::DataTransferCode code);

    void connect(tls::TlsSession* tls);
    void disconnect();

    void processPacket(std::vector<uint8_t>&& buf);
    bool sendData(const std::vector<uint8_t>& s);

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    NON_COPYABLE(DataConnection);

    const DRing::DataConnectionId id_;
    tls::TlsSession* tls_ {nullptr};
    uint32_t maxPayload_ {0}; // initialized at TLS connection, then negotiated by handshake
    int protocol_ {0}; // negotiated communication protocol

    mutable std::mutex infoMutex_ {};
    DRing::DataConnectionInfo info_ {};

    const Clock::time_point startTime_ {};

private: // Statistics
    std::mutex statMutex_ {};
    Clock::time_point statSync_;
    std::size_t rxBadPkt_ {0};
    std::size_t rxPkt_ {0};
    std::size_t rxBytes_ {0};
    std::size_t rxLossPkt_ {0};
    std::size_t rxACK_ {0};
    std::size_t txDataPkt_ {0};
    std::size_t txNAK_ {0};
    std::size_t txReTxPkt_ {0};

    void idleJob();
    void processCtrlPacket(std::vector<uint8_t>&);
    void processDataPacket(std::vector<uint8_t>&&);

private: // Handshake members
    std::atomic<unsigned> handshakeState_ {0};
    std::atomic_bool handshakeComplete_ {false};
    Clock::time_point lastHandshakeTime_ {};
    bool handshakeJob();
    void onPeerHandshake(int protocol, const uint32_t*);

private: // overall communication
    std::mutex ackMutex_ {}; // rx/tx queue sync for acknowledgement

private: // Tx related members
    SeqNum::seqnum_t initialSeq_ {SeqNum::INVALID};
    SeqNum::seqnum_t txSeq_ {SeqNum::INVALID};
    SeqNum::seqnum_t lastTxAck_ {SeqNum::INVALID};
    std::chrono::microseconds txDelay_ {1};
    std::unique_ptr<TxQueue<Clock>> txQueue_;
    std::unique_ptr<TxBuffer> txBuffer_;
    std::unique_ptr<LossRecorder> txLossList_;

    std::shared_ptr<std::vector<uint8_t>> nextTxPacket(TimePoint& when);
    void sendNack(const std::array<uint32_t, 2>&, int);
    void sendSingleAck();
    void sendHandshake();

private: // Rx related members
    SeqNum::seqnum_t peerInitSeqNum_ {SeqNum::INVALID};
    SeqNum::seqnum_t lastRxSeqNum_ {SeqNum::INVALID};
    SeqNum::seqnum_t lastRxAck_ {SeqNum::INVALID};
    std::size_t rxPktCount_ {0};
    std::unique_ptr<RxQueue> rxQueue_;
    std::unique_ptr<RxBuffer> rxBuffer_;
    std::unique_ptr<LossRecorder> rxLossList_;

    friend std::shared_ptr<DataConnection> makeDataConnection(const std::string& account_id,
                                                              const std::string& peer_id);

    friend RxQueue;
};

std::shared_ptr<DataConnection> makeDataConnection(const std::string& account_id,
                                                   const std::string& peer_id);

class FileTransfer
{
public:
    static DRing::DataTransferId newFileTransfer(const DRing::DataConnectionId& cid,
                                                 const std::string& name,
                                                 std::ifstream&& stream);
};

}} // namespace ring::ReliableSocket
