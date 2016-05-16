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

#include "data_transfer.h"
#include "manager.h"
#include "client/ring_signal.h"
#include "string_utils.h"
#include "security/tls_session.h"
#include "dring/datatransfer_interface.h"

#include <random>
#include <algorithm>
#include <exception>
#include <queue>
#include <deque>
#include <bitset>

namespace ring { namespace ReliableSocket {

//==================================================================================================
// Various helpers

template<typename... Args>
inline void
emitDataXferStatus(Args... args)
{
    emitSignal<DRing::DataTransferSignal::DataTransferStatus>(args...);
}

template <class... Args>
static auto cast_to_s(const Args&... args)
    -> decltype(std::chrono::duration_cast<std::chrono::seconds>(args...))
{
    return std::chrono::duration_cast<std::chrono::seconds>(args...);
}

template <class... Args>
static auto cast_to_ms(const Args&... args)
    -> decltype(std::chrono::duration_cast<std::chrono::milliseconds>(args...))
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(args...);
}

template <class... Args>
static auto cast_to_us(const Args&... args)
    -> decltype(std::chrono::duration_cast<std::chrono::microseconds>(args...))
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(args...);
}

template <class T>
inline void bswap32(T* mem, std::size_t len)
{
    auto mem32 = reinterpret_cast<uint32_t*>(mem);
    // Notes: benchmarks show using a lambda gives better performances than a direct call to htonl
    std::transform(mem32, mem32 + len/4, mem32, [](uint32_t x){ return htonl(x); });
}

template <typename Queue, typename Clock>
struct TimedJob
{
    using TimePoint = typename Clock::time_point;
    using Callable = std::function<void(TimedJob<Queue, Clock>& self)>;
    TimedJob(Queue* owner, const TimedJob::TimePoint& when, const TimedJob::Callable& func)
        : owner(owner), callTime(when), callback(func) {}

    TimedJob(Queue* owner, const TimedJob::TimePoint& when, TimedJob::Callable&& func)
        : owner(owner), callTime(when), callback(std::move(func)) {}

    template <typename T>
    void reschedule(const T& delay) { owner->addTimedJob(delay, std::move(*this)); }
    void reschedule(const TimePoint& when) { owner->addTimedJob(when, std::move(*this)); }
    Queue* owner {nullptr};
    TimePoint callTime {};
    Callable callback {};
};

template <typename Queue, typename Clock>
struct SortTimedJob
{
    constexpr bool operator()(const TimedJob<Queue, Clock>& lhs, const TimedJob<Queue, Clock>& rhs) const {
        return lhs.callTime > rhs.callTime;
    }
};

//==================================================================================================
// Constants

static constexpr unsigned MAX_ACK_CT = 15; // Channel packet format uses 4 bits to encode AckCt
static constexpr unsigned ACK_MAX_DELAY = 4; // Maximal number of received packet before sending an ACK
static constexpr auto ACK_DELAY = std::chrono::milliseconds(10); // ACK after this delay if no ACK send elsewhere
static constexpr auto HEARTBEAT_DELAY = std::chrono::seconds(3);

//==================================================================================================

namespace Packet
{

// Wire format: all words are given in network order (bigendian).
//
// Channel packet format:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | Flags |          TSN = Transmittion Sequence Number           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | AckCt |         ASN = Acknowledgment Sequence Number          |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                                                               |
//  ~                        Channel payload                        ~
//  |                                                               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Flags:
//      bit 0: if set, this is an initialization packet (see initialization payload)
//      bit 1: channel payload contains frames (after any others special payloads, like init packet)
//      bit 2-3: reserved and must be set to 0
//
//   TSN:
//      Transmittion Sequence Number
//      Monotonicaly increased for each packet transmitted.
//      This is the first 28-bits of the 64-bits peer TSN.
//
//   AckCt:
//      Number of contiguous acknowledged sequence number immediately before the following ASN.
//
//   ASN:
//      Acknowledgment Sequence Number
//      Gives the highest sequence number received so far.
//
//
//
// Initialization packet payload format (client, flags 0 is set):
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                          Channel Id                           |
//  |                                                               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     Protocol Version Tag                      |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Channel Id:
//      The unique identifiant of the communication channel
//
//   Protocol Version Tag:
//      An opaque value representing the client's protocol requested
//
//
//
// Initialization packet payload format (server, flags 0 is set):
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                          Channel Id                           |
//  |                                                               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                    Protocol Version Tag #1                    |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                    Protocol Version Tag #2                    |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                                                               |
//  ~                              ...                              ~
//  |                                                               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                    Protocol Version Tag #n                    |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Channel Id:
//      The unique identifiant of the communication channel
//
//   Protocol Version Tag #n:
//      An opaque value representing the server's protocol supported
//
//
// Heartbeat packets:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |0 0 0 0|          TSN = Transmittion Sequence Number           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | AckCt |          ASN = Acknowledgment Sequence Number          |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  These packets use the common channel header without any payloads and all flags bits set to 0.
//  This implies they can be sent ONLY if protocol version is negotiated.
//  TSN, AckCt and ASN are processed normally.
//
//
// Frame packet payload format (flags 1 is set):
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |     Flags     |        type-dependent data follow ...         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Flags:
//      bit 0: set if its a STREAM frame type, see STREAM frame for remaining bits, not set for
//          control frame
//
//

enum Flags {
    // We count bit for LSB to MSB (!= of upper schematics)
    INIT = 3,
    FRAME = 2,
    RSV1 = 1,
    RSV2 = 0,
};

template <typename U=uint8_t, typename T>
inline U*
getChannelPayload(std::vector<T>& vec)
{
    return reinterpret_cast<U*>(reinterpret_cast<uint8_t*>(vec.data()) + PACKET_HEADER_BYTES);
}

template <typename U=uint8_t, typename T>
inline const U*
getChannelPayload(const std::vector<T>& vec)
{
    return reinterpret_cast<const U*>(reinterpret_cast<const uint8_t*>(vec.data()) + PACKET_HEADER_BYTES);
}

template <typename U=uint8_t, typename T>
inline unsigned
getChannelPayloadSize(const std::vector<T>& vec)
{
    return (vec.size() * sizeof(T) - PACKET_HEADER_BYTES) / sizeof(U);
}

template <typename T>
inline std::bitset<4>
getChannelFlags(const std::vector<T>& vec)
{
    return reinterpret_cast<const uint8_t*>(vec.data())[0] >> 4;
}

template <typename T>
inline void
setChannelFlags(std::vector<T>& vec, std::bitset<4> flags)
{
    static const uint32_t mask = htonl(0xF0000000);
    auto p = &reinterpret_cast<uint32_t*>(vec.data())[0];
    *p = (*p & ~mask) | htonl(flags.to_ulong() << 28);
}

template <typename T>
inline uint32_t
getTsn(const std::vector<T>& vec)
{
    return htonl(reinterpret_cast<const uint32_t*>(vec.data())[0]) & 0x0FFFFFFF;
}

template <typename T>
inline void
setTsn(std::vector<T>& vec, uint32_t tsn)
{
    static const uint32_t mask = htonl(0x0FFFFFFF);
    auto p = &reinterpret_cast<uint32_t*>(vec.data())[0];
    *p = (*p & ~mask) | (htonl(tsn) & mask);
}

template <typename T>
inline void
getAsnAckCt(const std::vector<T>& vec, uint32_t& asn, uint8_t& ackct)
{
    auto v = htonl(reinterpret_cast<const uint32_t*>(vec.data())[1]);
    asn = v & 0x0FFFFFFF;
    ackct = v >> 28;
}

template <typename T>
inline void
setAsnAckCnt(std::vector<T>& vec, uint32_t asn, uint32_t ackcnt)
{
    reinterpret_cast<uint32_t*>(vec.data())[1] = htonl((asn & 0x0fffffff) + ((ackcnt & 0xf) << 28));
}

template <typename T>
inline void
initChannel(std::vector<T>& vec, const void* payload=nullptr, std::size_t payload_size=0)
{
    vec.resize(PACKET_HEADER_BYTES + payload_size);
    std::copy_n(reinterpret_cast<const uint8_t*>(payload), payload_size,
                reinterpret_cast<uint8_t*>(vec.data()) + PACKET_HEADER_BYTES);
}

template <typename T>
inline void
dump(const std::vector<T>& vec)
{
    auto p = reinterpret_cast<const uint32_t*>(vec.data());
    for (std::size_t i=0; i < (vec.size() * sizeof(T) / 4); ++i, ++p)
        RING_ERR("[%zu] %08x", i, *p);
}

template <typename T>
inline void
initProto(std::vector<T>& vec, uint64_t id, uint32_t proto)
{
    std::array<uint32_t, 3> data;
    data[0] = ntohl(id >> 32);
    data[1] = ntohl(id & 0xFFFFFFFF);
    data[2] = ntohl(proto);
    Packet::initChannel(vec, data.data(), data.size() * 4);
    Packet::setChannelFlags(vec, 1<<INIT);
}

template <typename T>
inline void
initHeartbeat(std::vector<T>& vec)
{
    std::array<uint8_t, 1200> data;
    Packet::initChannel(vec, data.data(), data.size());
}

} // namespace ring::ReliableSocket::Packet

//==================================================================================================

#if 0
class LossRecorder
{
public:
    LossRecorder() {}
    void init(std::size_t size);
    std::size_t length() const { return length_; }
    bool empty() const { return length_ == 0; }

    SeqNum::seqnum_t getFirst() const;
    void insert(SeqNum::seqnum_t first, SeqNum::seqnum_t last);
    void insert(SeqNum::seqnum_t seq);
    bool remove(SeqNum::seqnum_t seq);
    SeqNum::seqnum_t popFirst();

private:
    using Record = std::pair<SeqNum::seqnum_t, SeqNum::seqnum_t>;

    NON_COPYABLE(LossRecorder);
    mutable std::mutex mutex_ {};
    std::list<Record> records_ {};
    std::size_t limit_;
    std::size_t length_ {0}; // number of record without SeqNum::INVALID number
};

void
LossRecorder::init(size_t size)
{
    limit_ = size;
}

void
LossRecorder::insert(SeqNum::seqnum_t first, SeqNum::seqnum_t last)
{
    std::lock_guard<std::mutex> lk {mutex_};

    if (!records_.empty()) {
        // try to merge with latest insertion
        auto& latest = records_.back();
        if (latest.second == SeqNum::decrease(first)) {
            latest.second = last;
            length_ += SeqNum::length(first, last);
            RING_DBG("loss: merged 0x%08x-0x%08x (len=%zu)", first, last, length_);
            return;
        }
    }

    // add new entry
    records_.emplace_back(first, last);
    length_ += SeqNum::length(first, last);
    RING_DBG("loss: inserted 0x%08x-0x%08x (len=%zu)", first, last, length_);
}

void
LossRecorder::insert(SeqNum::seqnum_t seq)
{
    std::lock_guard<std::mutex> lk {mutex_};

    if (!records_.empty()) {
        // try to merge with latest insertion
        auto& latest = records_.back();
        if (latest.second == SeqNum::decrease(seq)) {
            latest.second = seq;
            ++length_;
            RING_DBG("loss: merged 0x%08x (len=%zu)", seq, length_);
            return;
        }
    }

    // add new entry
    records_.emplace_back(seq, seq);
    ++length_;
    RING_DBG("loss: inserted 0x%08x (len=%zu)", seq, length_);
}

bool
LossRecorder::remove(SeqNum::seqnum_t seq)
{
    std::lock_guard<std::mutex> lk {mutex_};

    if (seq == SeqNum::INVALID)
        return false;

    // TODO: make it O(1)
    const auto& iter = std::find_if(records_.begin(), records_.end(), [seq](const Record& rec) {
            return SeqNum::offset(rec.first, seq) >= 0 and SeqNum::offset(rec.second, seq) <= 0;
        });
    if (iter == records_.cend())
        return false;

    --length_;

    RING_DBG("loss: remove 0x%08x from 0x%08x-0x%08x", seq, iter->first, iter->second);

    // 1-element record : drop record
    if (iter->first == iter->second) {
        records_.erase(iter);
        return true;
    }

    // start or stop seqnum has to be removed: modify them
    if (iter->first == seq) {
        iter->first = SeqNum::increase(iter->first);
        return true;
    }
    if (iter->second == seq) {
        iter->second = SeqNum::decrease(iter->second);
        return true;
    }

    // seqnum is somewhere in the range: split the record at seq position
    records_.emplace(iter, iter->first, SeqNum::decrease(seq));
    iter->first = SeqNum::increase(seq);
    return true;
}

SeqNum::seqnum_t
LossRecorder::getFirst() const
{
    std::lock_guard<std::mutex> lk {mutex_};
    if (records_.empty())
        return SeqNum::INVALID;
    return records_.front().first;
}

SeqNum::seqnum_t
LossRecorder::popFirst()
{
    std::lock_guard<std::mutex> lk {mutex_};
    if (records_.empty())
        return SeqNum::INVALID;

    --length_;
    auto& front = records_.front();
    auto seq = front.first;

    if (front.first == front.second)
        records_.pop_front(); // 1-element record : drop record
    else
        front.first = SeqNum::increase(front.first);

    return seq;
}
#endif

//==================================================================================================

#if 0
class TxDataBuffer
{
public:
    TxDataBuffer(unsigned size, unsigned payload_size) : maxPacket_(size), mss_(payload_size) {}

    SeqNum::seqnum_t getFirstSentPacketSeq();
    std::shared_ptr<std::vector<uint8_t>> nextPacket(SeqNum::seqnum_t seq);
    bool push(std::vector<uint8_t>&& packet);
    void remove(SeqNum::seqnum_t seq);
    void removeTo(SeqNum::seqnum_t seq);
    bool retransmit(SeqNum::seqnum_t seq);

private:
    unsigned maxPacket_;
    unsigned mss_;
    std::mutex accMtx_ {};
    std::queue<std::shared_ptr<std::vector<uint8_t>>> toSend_ {};
    std::map<SeqNum::seqnum_t, std::shared_ptr<std::vector<uint8_t>>> sent_ {};
};

bool
TxDataBuffer::push(std::vector<uint8_t>&& packet)
{
    std::lock_guard<std::mutex> lk {accMtx_};
    RING_DBG("txb: size %zu (%zu, %zu)", toSend_.size() + sent_.size(), toSend_.size(), sent_.size());
    if (toSend_.size() + sent_.size() == maxPacket_)
        return false; // full
    toSend_.emplace(std::make_shared<std::vector<uint8_t>>());
    auto& ptr = toSend_.back();
    *ptr = std::move(packet);
    return true;
}

std::shared_ptr<std::vector<uint8_t>>
TxDataBuffer::nextPacket(SeqNum::seqnum_t seq)
{
    if (seq == SeqNum::INVALID)
        return {};
    std::lock_guard<std::mutex> lk {accMtx_};
    if (toSend_.empty()) {
        RING_DBG("txDataBuffer: empty");
        return {};
    }
    auto pkt = toSend_.front();
    Packet::setDataSeq(*pkt, seq);
    sent_.emplace(seq, pkt);
    toSend_.pop();
    return pkt;
}

bool
TxDataBuffer::retransmit(SeqNum::seqnum_t seq)
{
    std::lock_guard<std::mutex> lk {accMtx_};
    auto iter = sent_.find(seq);
    if (iter == sent_.cend())
        return false;
    // replace this sent packet as next packet to send
    toSend_.emplace(std::move(iter->second));
    sent_.erase(iter);
    return true;
}

SeqNum::seqnum_t
TxDataBuffer::getFirstSentPacketSeq()
{
    std::lock_guard<std::mutex> lk {accMtx_};
    if (sent_.empty())
        return SeqNum::INVALID;
    return sent_.begin()->first;
}

void
TxDataBuffer::remove(SeqNum::seqnum_t seq)
{
    if (seq == SeqNum::INVALID)
        return;
    std::lock_guard<std::mutex> lk {accMtx_};
    sent_.erase(seq);
}

void
TxDataBuffer::removeTo(SeqNum::seqnum_t seq)
{
    if (seq == SeqNum::INVALID)
        return;
    std::lock_guard<std::mutex> lk {accMtx_};
    for (auto it = sent_.begin(); it != sent_.end(); ) {
        if (SeqNum::cmp(it->first, seq) <= 0)
            it = sent_.erase(it);
        else
            break;
    }
}
#endif

//==================================================================================================

template <class Clock>
class TxScheduler
{
public:
    using Duration = typename Clock::duration;
    using TimePoint = typename Clock::time_point;

    TxScheduler(DataConnection& parent);
    ~TxScheduler();

    ssize_t send(const std::vector<uint8_t>& pkt);
    void update(const TimePoint& ts, bool reshedule = false);
    void update(bool reshedule = false); // update now (force popPacket_ call as soon as possible)

private:
    void threadJob();

    DataConnection& dc_;
    std::mutex jobMutex_ {};
    std::condition_variable jobCv_ {};
    bool scheduled_ {false}; // true when we need to call popPacket_ at popPacketTime_ time
    TimePoint popPacketTime_ {};
    std::atomic_bool running_ {true};
    std::thread thread_ {};
};

template <class Clock>
TxScheduler<Clock>::TxScheduler(DataConnection& parent)
    : dc_(parent)
    , popPacketTime_(Clock::now())
    , thread_(&TxScheduler::threadJob, this)
{}

template <class Clock>
TxScheduler<Clock>::~TxScheduler()
{
    running_ = false;
    jobCv_.notify_one();
    if (thread_.joinable())
        thread_.join();
}

template <class Clock>
void
TxScheduler<Clock>::update(const typename Clock::time_point& ts, bool reshedule)
{
    std::lock_guard<std::mutex> lk {jobMutex_};

    if (scheduled_ and !reshedule) {
        if (ts < popPacketTime_)
            return;
    }

    popPacketTime_ = ts;
    scheduled_ = true;
    jobCv_.notify_one();
}

template <class Clock>
void
TxScheduler<Clock>::update(bool reshedule)
{
    update(Clock::now(), reshedule);
}

template <class Clock>
void
TxScheduler<Clock>::threadJob()
{
    while (running_) {
        std::unique_lock<std::mutex> lk {jobMutex_};

        // wait for scheduling
        if (!scheduled_)
            jobCv_.wait(lk, [this]{ return !running_ or scheduled_; });

        // wait until schedule time is over or re-scheduling
        while (true) {
            if (!running_)
                return;

            auto when = popPacketTime_;
            if (!jobCv_.wait_until(lk, when,
                                   [this, when] { return !running_ or popPacketTime_ < when; }))
                break;
        }

        if (!running_)
            return;

        lk.unlock();
        TimePoint nextTime;
        auto pkt = dc_.nextTxPacket(nextTime);
        lk.lock();

        if (pkt) {
            popPacketTime_ = nextTime;
            dc_.sendPkt(*pkt); // ignore failures
            scheduled_ = true;
        } else {
            RING_WARN("tx: no packet");
            scheduled_ = false;
        }
    }
}

//==================================================================================================

#if 0
class RxBuffer
{
public:
    RxBuffer(unsigned size) : size_(size) {}

    bool push(std::vector<uint8_t>&& pkt);
    bool pop(std::vector<uint8_t>& pkt);

private:
    std::mutex pktMutex_ {};
    std::list<std::vector<uint8_t>> packets_ {};
    unsigned size_;
    SeqNum::seqnum_t lastPopSeq_ {SeqNum::INVALID};
};

bool
RxBuffer::push(std::vector<uint8_t>&& pkt)
{
    std::lock_guard<std::mutex> lk {pktMutex_};

    // limit number of stored packets
    if (!packets_.empty()) {
        auto seq = Packet::getDataSeq(pkt);
        if (SeqNum::length(Packet::getDataSeq(packets_.back()), seq) > size_) {
            RING_ERR("rxbuf: full, wait for 0x%08x", SeqNum::increase(lastPopSeq_));
            return false;
        }
    }

    packets_.emplace_front(std::move(pkt));
    return true;
}

bool
RxBuffer::pop(std::vector<uint8_t>& pkt)
{
    std::lock_guard<std::mutex> lk {pktMutex_};
    if (packets_.empty())
        return false;

    // first packet?
    if (lastPopSeq_ == SeqNum::INVALID) {
        pkt = std::move(packets_.back());
        lastPopSeq_ = Packet::getDataSeq(pkt);
        packets_.pop_back();
        return true;
    }

    // give packet in order
    auto seq = Packet::getDataSeq(packets_.back());
    if (seq != SeqNum::increase(lastPopSeq_))
        return false;

    pkt = std::move(packets_.back());
    packets_.pop_back();
    lastPopSeq_ = seq;
    return true;
}
#endif

//==================================================================================================

#if 0
class RxQueue
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Timer = TimedJob<RxQueue, Clock>;

    RxQueue(DataConnection& dc);
    ~RxQueue();

    void push(std::vector<uint8_t>&& pkt);

    template <typename T>
    void addTimedJob(const T& delay, const Timer::Callable& callback);

    template <typename T>
    void addTimedJob(const T& delay, Timer::Callable&& callback);

    template <typename T>
    void addTimedJob(const T& delay, Timer&& timer);

    void addTimedJob(const TimePoint& when, Timer&& timer);

private:
    static constexpr unsigned RX_QUEUE_SIZE {8192}; // unit = packet
    void threadJob();

    DataConnection& dc_;

    std::mutex jobMutex_ {};
    std::condition_variable jobCv_ {};
    std::priority_queue<Timer,
                        std::vector<Timer>,
                        SortTimedJob<RxQueue, Clock>> timerQueue_;
    std::deque<std::vector<uint8_t>> rxQueue_ {};

    std::atomic_bool running_ {true};
    std::thread thread_ {};
};

RxQueue::RxQueue(DataConnection& dc)
    : dc_(dc)
{
    thread_ = std::thread(&RxQueue::threadJob, this);
}

RxQueue::~RxQueue()
{
    running_ = false;
    jobCv_.notify_one();
    if (thread_.joinable())
        thread_.join();
}

void
RxQueue::push(std::vector<uint8_t>&& pkt)
{
    std::lock_guard<std::mutex> lk {jobMutex_};
    if (rxQueue_.size() < RX_QUEUE_SIZE) {
        rxQueue_.emplace_back(std::move(pkt));
        jobCv_.notify_one();
    } else
        RING_WARN("rxb: full");
}

template <typename T>
void
RxQueue::addTimedJob(const T& delay, const Timer::Callable& func)
{
    auto when = Clock::now() + delay; // do it out of locked area to not be delayed
    std::lock_guard<std::mutex> lk {jobMutex_};
    timerQueue_.emplace(this, when, func);
    jobCv_.notify_one();
}

template <typename T>
void
RxQueue::addTimedJob(const T& delay, Timer::Callable&& func)
{
    auto when = Clock::now() + delay; // do it out of locked area to not be delayed
    std::lock_guard<std::mutex> lk {jobMutex_};
    timerQueue_.emplace(this, when, std::move(func));
    jobCv_.notify_one();
}

template <typename T>
void
RxQueue::addTimedJob(const T& delay, Timer&& timer)
{
    auto when = Clock::now() + delay; // do it out of locked area to not be delayed
    std::lock_guard<std::mutex> lk {jobMutex_};
    timer.callTime = when;
    timerQueue_.emplace(std::move(timer));
    jobCv_.notify_one();
}

void
RxQueue::addTimedJob(const TimePoint& when, Timer&& timer)
{
    std::lock_guard<std::mutex> lk {jobMutex_};
    timer.callTime = when;
    timerQueue_.emplace(std::move(timer));
    jobCv_.notify_one();
}

void
RxQueue::threadJob()
{
    while (running_) {
        std::unique_lock<std::mutex> lk {jobMutex_};

        // Wait for kill signal or incoming packet until timer exhausting (if exist)
        while (rxQueue_.empty()) {
            if (!running_)
                return;

            Clock::time_point when;
            if (timerQueue_.empty())
                when = Clock::now() + std::chrono::seconds(60);
            else
                when = timerQueue_.top().callTime;

            if (!jobCv_.wait_until(lk, when, [this, when] {
                        return !running_
                            or !rxQueue_.empty()
                            or (!timerQueue_.empty() and timerQueue_.top().callTime != when);
                    }) and !timerQueue_.empty()) {
                // handle all exhausted timers, rescheduled timer are re-instered after
                // the end of the loop to be handled at next thread loop
                decltype(timerQueue_) tmp_timer_queue;

                std::swap(timerQueue_, tmp_timer_queue);
                lk.unlock();
                auto now = Clock::now();
                while (!tmp_timer_queue.empty()) {
                    if (!running_)
                        return;
                    auto timer = std::move(tmp_timer_queue.top());
                    when = timer.callTime;
                    if (when > now)
                        break;
                    timer.callback(timer);
                    tmp_timer_queue.pop();
                }
                lk.lock();
                std::swap(timerQueue_, tmp_timer_queue);

                // Finaly insert new timers scheduled during the execution loop
                while (!tmp_timer_queue.empty()) {
                    timerQueue_.emplace(std::move(tmp_timer_queue.top()));
                    tmp_timer_queue.pop();
                }
            }
        }

        // Process incoming packets
        decltype(rxQueue_) rxq = std::move(rxQueue_);
        rxQueue_.clear();
        lk.unlock();

        int count = 5; // limit number of packet processed by iteration loop to have a chance to process timers
        for (auto& pkt : rxq) {
            if (!running_)
                return;

            if (Packet::isCtrl(pkt))
                dc_.processCtrlPacket(pkt);
            else
                dc_.processDataPacket(std::move(pkt));

            if (--count == 0)
                break;
        }

        //lk.lock();
    }
}
#endif

//==================================================================================================

using DataConnectionMap = std::map<DRing::DataConnectionId, std::weak_ptr<DataConnection>>;

static DataConnectionMap&
get_data_connection_map()
{
    static DataConnectionMap map;
    return map;
}

static DRing::DataConnectionId
get_unique_connection_id()
{
    static std::atomic<DRing::DataConnectionId> uid {0};
    uid += 1;
    return uid;
}

std::shared_ptr<DataConnection>
makeDataConnection(const std::string& account_id, const std::string& peer_id, bool is_client)
{
    auto cnx = std::make_shared<DataConnection>(account_id, peer_id, is_client);
    auto& map = get_data_connection_map();
    map.emplace(cnx->id_, cnx);
    return cnx;
}

std::shared_ptr<DataConnection>
DataConnection::getDataConnection(const DRing::DataConnectionId& tid)
{
    auto& map = get_data_connection_map();
    auto iter = map.find(tid);
    if (iter == map.cend())
        return {};
    if (auto dt = iter->second.lock())
        return dt;
    map.erase(iter);
    return {};
}

DataConnection::DataConnection(const std::string& account_id, const std::string& peer_id,
                               bool is_client)
    : id_(get_unique_connection_id())
    , isClient_(is_client)
    , startTime_(Clock::now())
    , statSync_(Clock::now())
    , lastHeartbeat_(Clock::now())
      //, timeEXPCount_(1)
      //, timeRTT_(10 * SYN_DELAY)
      //, timeRTTVar_(timeRTT_ / 2)
      //, lastRxTime_(Clock::now())
{
    info_.account = account_id;
    info_.peer = peer_id;
    info_.code = DRing::DataTransferCode::CODE_UNKNOWN;
}

DataConnection::~DataConnection()
{
    disconnect();
}

void
DataConnection::stopConnection()
{
    {
        std::lock_guard<std::mutex> lk {txMtx_};
        txSeq_ = 0;
    }

    tls_->shutdown();
}

void
DataConnection::connect(tls::TlsSession* tls)
{
    tls_ = tls;
    maxPayload_ = tls_->getMaxPayload() - PACKET_HEADER_BYTES;
    RING_DBG("[FTP:%lx] maxPayload=%u", id_, maxPayload_);

    txQueue_.reset(new TxScheduler<Clock>(*this));
    //rxQueue_.reset(new RxQueue(*this));
    //txLossList_.reset(new LossRecorder());
    //rxLossList_.reset(new LossRecorder());

    if (isClient_)
        txQueue_->update();

    // FOR DEBUG ONLY
    Manager::instance().registerEventHandler(reinterpret_cast<uintptr_t>(this),
                                             [this]{ idleJob(); });
}

void
DataConnection::disconnect()
{
    // FOR DEBUG ONLY
    Manager::instance().unregisterEventHandler(reinterpret_cast<uintptr_t>(this));

    txQueue_.reset();

#if 0
    // Stop handshake job
    handshakeComplete_ = true;

    txQueue_.reset();
    rxQueue_.reset();
    txBuffer_.reset();
    rxBuffer_.reset();
    txLossList_.reset();
    rxLossList_.reset();
#endif

    tls_ = nullptr;
}

void
DataConnection::idleJob()
{
    if (protocol_) {
        // Send Heartbeat
        auto now = Clock::now();
        if (now - lastHeartbeat_ >= HEARTBEAT_DELAY) {
            lastHeartbeat_ = now;
            sendHeartbeat();
        }
    }

    // Statistics output
    std::lock_guard<std::mutex> lk {statMutex_};
    auto now = Clock::now();
    float delta = cast_to_s(now - statSync_).count();
    if (delta > 5) {
        RING_WARN("\nrx: Bytes: %.3fMbits/s, Bad:%zu",
                  rxBytes_.load() * 8 / delta / 1e6, rxBadPkt_.load());
        statSync_ = now;
        rxBytes_ = rxBadPkt_ = 0;
    }
}

inline void
DataConnection::updateTxAckState(SeqNum seq, unsigned count)
{
    std::lock_guard<std::mutex> lk {ackMtx_};
    txAckSeq_ = seq;
    txAckCt_ = count;
}

inline void
DataConnection::getTxAckState(SeqNum& seq, unsigned& count)
{
    std::lock_guard<std::mutex> lk {ackMtx_};
    seq = txAckSeq_;
    count = txAckCt_;
}

std::shared_ptr<std::vector<uint8_t>>
DataConnection::nextTxPacket(TimePoint& when)
{
    auto packet_ts = Clock::now();
    std::shared_ptr<std::vector<uint8_t>> pkt;

    if (!protocol_) {
        // non-negotiated server does nothing here
        if (!isClient_)
            return {};

        // Client: force INIT packet until negotiated
        pkt = std::make_shared<std::vector<uint8_t>>();
        Packet::initProto(*pkt, id_, PROTOCOL_VERSION);
    }

    // Add any waiting frame to the end
    // TODO

    if (!pkt)
        return {};

    // Delay next call
    when = packet_ts + txDelay_;
    return pkt;
}

ssize_t
DataConnection::sendPkt(std::vector<uint8_t>& pkt, SeqNum ack_seq, unsigned ack_count)
{
#ifndef NDEBUG
    if (pkt.size() < PACKET_HEADER_BYTES) {
        RING_ERR("FATAL: TxQueue::send() : pkt.size() < PACKET_HEADER_BYTES");
        std::terminate();
    }
#endif

    if (pkt.size() > maxPayload_) {
        RING_ERR("[FTP:%zu] packet oversized (%zu > %u)", id_, pkt.size(), maxPayload_);
        return -1;
    }

    std::lock_guard<std::mutex> lk {txMtx_};
    if (!txSeq_)
        return -1;

    auto tsn = txSeq_++;
    Packet::setTsn(pkt, tsn);
    Packet::setAsnAckCnt(pkt, ack_seq, ack_count);

    if (delayedAck_) {
        delayedAck_ = 0;
        // TODO: stop ack timer
    }

#if 0
    // debug: loss simulation
    if (isClient_ and (++txLossSimu_ % 5) == 0) {
        RING_ERR("tx: drop 0x%08x", tsn);
        return pkt.size();
    }
#endif

    //Packet::dump(pkt);
    return tls_->send(pkt);
}

ssize_t
DataConnection::sendPkt(std::vector<uint8_t>& pkt)
{
    SeqNum ack_seq;
    unsigned ack_count;
    getTxAckState(ack_seq, ack_count);
    return sendPkt(pkt, ack_seq, ack_count);
}

void
DataConnection::sendProto()
{
    std::vector<uint8_t> pkt;
    Packet::initProto(pkt, id_, PROTOCOL_VERSION);
    sendPkt(pkt);
}

void
DataConnection::sendHeartbeat()
{
    std::vector<uint8_t> pkt;
    Packet::initHeartbeat(pkt);
    sendPkt(pkt);
}

void
DataConnection::sendAck(SeqNum ack_seq, unsigned ack_count)
{
    std::vector<uint8_t> pkt;
    Packet::initHeartbeat(pkt);
    sendPkt(pkt, ack_seq, ack_count);
}

void
DataConnection::processPacket(std::vector<uint8_t>&& pkt)
{
    if (pkt.size() < PACKET_HEADER_BYTES) {
        ++rxBadPkt_;
        return;
    }

    // Packet sequence number field processing
    uint32_t peer_tsn = Packet::getTsn(pkt);
    int32_t seq_delta = ((static_cast<int32_t>(peer_tsn) << 4) - (static_cast<int32_t>(rxSeq_) << 4)) >> 4;
    SeqNum pkt_seq = rxSeq_ + seq_delta;

    // as seq progression are monotonic without wraparound, negative seq delta
    // are out-of-order or duplicate packets. Dup and too old OOO packets are dropped.
    if (seq_delta <= 0) {
        if (seq_delta <= MISS_ORDERING_LIMIT) {
            RING_WARN("[FTP:%zu] drop too old pkt %016lx", id_, pkt_seq);
            ++rxOldPkt_;
            return;
        }

        // Duplicate?
        if (rxMask_.test(seq_delta)) {
            RING_WARN("[FTP:%zu] drop too old pkt %016lx", id_, pkt_seq);
            ++rxDupPkt_;
            return;
        }

        // Accepted out-of-order packet
        RING_DBG("[FTP:%zu] OOO detected (%u)", id_, -seq_delta);
        rxMask_.set(-seq_delta);
    } else {  // seq_delta > 0
        // Make sure we don't wraparound
        if (pkt_seq < rxSeq_) {
            // This should never append but .... (tx must be protected for that)
            RING_ERR("[FTP:%zu] fatal error, rx sequence number has wraparound!", id_);
            ++rxBadPkt_;
            stopConnection();
            return;
        }

        // New packet
        rxSeq_ = pkt_seq;
        rxMask_ = rxMask_.to_ulong() >> seq_delta; // slide the OOO window
        rxMask_.set(0);

        if (seq_delta > 1)
            RING_WARN("[FTP:%zu] possible loss of %u pkt", id_, seq_delta - 1);
    }

    // Acknowledgment field processing
    uint32_t pkt_asn;
    uint8_t pkt_ackct;
    Packet::getAsnAckCt(pkt, pkt_asn, pkt_ackct);
    int32_t ack_delta = ((static_cast<int32_t>(pkt_asn) << 4) - (static_cast<int32_t>(rxAckSeq_) << 4)) >> 4;
    SeqNum ack_seq = rxAckSeq_ + seq_delta;

    // New acknowledgments?
    unsigned ack_count = 0;
    if (ack_delta > 0) {
        rxAckSeq_ = ack_seq;
        rxAckMask_ = rxAckMask_.to_ulong() >> ack_delta; // slide the ack window

        // compute the number of contiguous acknowledged packets and record them in the ack window
        // note: ackct = 0 if 1 packet is acknowledged
        // note2: we don't take in account possible OOO packets that contains ACK info
        ack_count = std::min(ack_delta, pkt_ackct + 1);
        RING_DBG("[FTP:%zu] %u ack pkt", id_, ack_count);
        rxAckMask_ |= (1 << ack_count) - 1;

        // TODO: acknowledge upper layer about ack pkts

        // TODO: drop too late ack packets

        ack_delta = 0;
    }

    // Handle acknowledgement data in a OoO packet.
    // note: this OoO packet acknowledges an in-order packet.
    uint32_t ack_mask = (1 << pkt_ackct) - 1;
    if ((rxAckMask_.to_ulong() & ack_mask) != ack_mask) {
        for (int i=0; i < std::min(pkt_ackct, (uint8_t)rxAckMask_.size()); ++i) {
            int bit = -ack_delta + i;
            if (!rxAckMask_.test(bit)) {
                // TODO: acknowledge upper layer about ack pkts
                rxAckMask_.set(bit);
                ++ack_count;
            }
        }
    }

    // process INIT payload immediatly
    auto flags = Packet::getChannelFlags(pkt);
    if (!protocol_) {
        if (isClient_) {
            // packet refused by server?
            if (flags.test(Packet::Flags::INIT)) {
                RING_ERR("[FTP:%zu] fatal error, protocol refused by server", id_);
                ++rxBadPkt_;
                stopConnection();
                return;
            }

            protocol_ = PROTOCOL_VERSION; // commit the end of negotiation
        } else { // server
            // drop non-init packet or without enough information
            auto size = Packet::getChannelPayloadSize(pkt);
            if (!flags.test(Packet::Flags::INIT) or !size) {
                ++rxBadPkt_;
                return;
            }

            // send our support protocol if requested one is not permited
            auto payload = Packet::getChannelPayload<uint32_t>(pkt);
            auto peer_proto = ntohl(payload[2]);
            if (peer_proto != PROTOCOL_VERSION) {
                RING_WARN("[FTP:%zu] refusing client protocol (%08x)", id_, peer_proto);
                ++rxBadPkt_;
                sendProto();
                return; // drop any other data as non-supported protocol
            }

            // Save peer ConnectionId and process remaining data normally
            peerConnectionId_ = ntohl(payload[0]);
            peerConnectionId_ <<= 32;
            peerConnectionId_ += ntohl(payload[1]);

            RING_DBG("[FTP:%zu] client protocol accepted (%08x), client ID: %016lx", id_, peer_proto, peerConnectionId_);
            protocol_ = PROTOCOL_VERSION; // commit the end of negotiation

            sendHeartbeat(); // immediate confirmation using heartbeat
            txQueue_->update(); // let a chance to waiting frames
        }
    } else if (!isClient_ and flags.test(Packet::Flags::INIT)) {
        // drop init packets after negotation
        RING_WARN("[FTP:%zu] drop too late INIT pkt", id_);
        ++rxBadPkt_;
        return;
    }

    // transmit packet to upper layer (async)
    //rxQueue_->push(std::move(pkt));

    // do not ACK an heartbeat/ack-only packets
    bool ack_ack = pkt.size() == PACKET_HEADER_BYTES;

    int32_t ack_seq_delta = pkt_seq - txAckSeq_;
    RING_DBG("ack_seq_delta: %d", ack_seq_delta);
    if (ack_seq_delta == 1) {
        // increment AckCt and record current ack state
        updateTxAckState(pkt_seq, std::max(txAckCt_ + 1, MAX_ACK_CT));
        // Force ACK packet at least each ACK_MAX_DELAY packets
        if (++delayedAck_ > ACK_MAX_DELAY) {
            delayedAck_ = 0;
            sendAck(txAckSeq_, txAckCt_);
        } else if (!ack_ack) {
            // Do not ACK acks (except if ACK_MAX_DELAY reached)
            // Let idle job to flush delayed ACK at next loop
            //doAck_ = Clock::now() + ACK_DELAY;
        }
    } else if (ack_seq_delta > 1) {
        // non-contiguous sequences

        // flush delayed ACK
        if (delayedAck_) {
            delayedAck_ = 0;
            sendAck(txAckSeq_, txAckCt_);
            // TODO: stop ack timer
        }

        // reset ack state
        updateTxAckState(pkt_seq, 0);

        // acknowledge the current state immediately
        if (!ack_ack)
            sendAck(pkt_seq, 0);
    } else { // ack_seq_delta < 0
        // rx an OoO packet

        // flush delayed ACK
        if (delayedAck_) {
            delayedAck_ = 0;
            sendAck(txAckSeq_, txAckCt_);
            // TODO: stop ack timer
        }

        // acknowledge this sequence immediately
        if (!ack_ack)
            sendAck(pkt_seq, 0);
    }
}

}} // namespace ring::ReliableSocket
