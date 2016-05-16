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

static constexpr auto HANDSHAKE_RATE = std::chrono::milliseconds(250);

//==================================================================================================

namespace Packet
{

// General Packet Structure:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                         Packet Header                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                                                               |
//  ~                      Data / Control Field                     ~
//  |                                                               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// Data Packet Header:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |0|                        Sequence Number                      |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                          Time Stamp                           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |ff |o|                     Message Number                      |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   bit 0:
//      0: Data Packet
//
// Control Packet Header:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |1|            Type             |             Reserved          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          Time Stamp                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                        Additional Info                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   bit 1:
//      1: Control Packet
//
//   bits 1-15:
//      0: Protocol Connection Handshake
//          Add. Info:    Protocol version
//          Control Info: Handshake information (see CHandShake)
//

template <typename T>
inline bool
isData(const std::vector<T>& vec)
{
    return (reinterpret_cast<const uint32_t*>(vec.data())[0] & (1u << 31)) == 0;
}

template <typename T>
inline bool
isCtrl(const std::vector<T>& vec)
{
    return !isData(vec);
}

template <typename U=uint8_t, typename T>
inline U*
getPayload(std::vector<T>& vec)
{
    return reinterpret_cast<U*>(reinterpret_cast<uint8_t*>(vec.data()) + PACKET_HEADER_BYTES);
}

template <typename U=uint8_t, typename T>
inline const U*
getPayload(const std::vector<T>& vec)
{
    return reinterpret_cast<const U*>(reinterpret_cast<const uint8_t*>(vec.data()) + PACKET_HEADER_BYTES);
}

template <typename U=uint8_t, typename T>
inline unsigned
getPayloadSize(const std::vector<T>& vec)
{
    return (vec.size() * sizeof(T) - PACKET_HEADER_BYTES) / sizeof(U);
}

template <typename T>
inline SeqNum::seqnum_t
getDataSeq(const std::vector<T>& vec)
{
    return reinterpret_cast<const uint32_t*>(vec.data())[0] & ((1u << 31)-1);
}

template <typename T>
inline void
setDataSeq(std::vector<T>& vec, SeqNum::seqnum_t seq)
{
    reinterpret_cast<uint32_t*>(vec.data())[0] = seq;
}

template <typename T>
inline PacketType
getCtrlType(const std::vector<T>& vec)
{
    return static_cast<PacketType>((reinterpret_cast<const uint32_t*>(vec.data())[0] & ((1u << 31)-1)) >> 16);
}

template <typename T>
inline void
setCtrlType(std::vector<T>& vec, PacketType type)
{
    reinterpret_cast<uint32_t*>(vec.data())[0] = (1u << 31) | (static_cast<uint32_t>(type) << 16);
}

template <typename T>
inline uint32_t
getCtrlAddInfo(const std::vector<T>& vec)
{
    return reinterpret_cast<const uint32_t*>(vec.data())[2];
}

template <typename T>
inline void
setCtrlAddInfo(std::vector<T>& vec, uint32_t info)
{
    reinterpret_cast<uint32_t*>(vec.data())[2] = info;
}

template <typename T>
inline uint32_t
getTimestamp(const std::vector<T>& vec)
{
    return reinterpret_cast<const uint32_t*>(vec.data())[1];
}

template <typename T>
inline void
setTimestamp(std::vector<T>& vec, uint32_t ts)
{
    reinterpret_cast<uint32_t*>(vec.data())[1] = ts;
}

template <typename T>
inline void
initData(std::vector<T>& vec, const void* payload, std::size_t payload_size)
{
    vec.resize(PACKET_HEADER_BYTES + payload_size);
    std::copy_n(reinterpret_cast<const uint8_t*>(payload), payload_size,
                reinterpret_cast<uint8_t*>(vec.data()) + PACKET_HEADER_BYTES);
}

template <typename T>
inline void
initCtrl(std::vector<T>& vec, PacketType type, uint32_t info=0,
         const void* payload=nullptr, std::size_t payload_size=0)
{
    initData(vec, payload, payload_size);
    setCtrlType(vec, type);
    setCtrlAddInfo(vec, info);
}

template <typename T>
inline void
dump(const std::vector<T>& vec)
{
    auto p = reinterpret_cast<const uint32_t*>(vec.data());
    for (std::size_t i=0; i < (vec.size() * sizeof(T) / 4); ++i, ++p)
        RING_ERR("[%zu] %08x", i, *p);
}

} // namespace ring::ReliableSocket::Packet

//==================================================================================================

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

//==================================================================================================

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

//==================================================================================================

template <class Clock>
class TxQueue
{
public:
    using Duration = typename Clock::duration;
    using TimePoint = typename Clock::time_point;
    using PopPacketFunc = std::function<std::shared_ptr<std::vector<uint8_t>>(TimePoint&)>;

    TxQueue(tls::TlsSession* tls, const PopPacketFunc& func);
    ~TxQueue();

    void init(tls::TlsSession* tls, const PopPacketFunc& func);
    ssize_t send(const std::vector<uint8_t>& pkt);
    void update(const TimePoint& ts, bool reshedule = false);
    void update(bool reshedule = false); // update now (force popPacket_ call as soon as possible)

private:
    void threadJob();

    tls::TlsSession* tls_ {nullptr};
    std::mutex jobMutex_ {};
    std::condition_variable jobCv_ {};
    bool scheduled_ {false}; // true when we need to call popPacket_ at popPacketTime_ time
    TimePoint popPacketTime_ {};
    PopPacketFunc popPacket_ {};

    unsigned lossSimu_ {0};

    std::atomic_bool running_ {true};
    std::thread thread_ {};

    friend class SecureIceTransport;
};

template <class Clock>
TxQueue<Clock>::TxQueue(tls::TlsSession* tls, const PopPacketFunc& func)
{
    tls_ = tls;
    popPacket_ = func;
    popPacketTime_ = Clock::now();
    scheduled_ = false;
    thread_ = std::thread(&TxQueue::threadJob, this);
}

template <class Clock>
TxQueue<Clock>::~TxQueue()
{
    running_ = false;
    jobCv_.notify_one();
    if (thread_.joinable())
        thread_.join();
}

template <class Clock>
ssize_t
TxQueue<Clock>::send(const std::vector<uint8_t>& pkt)
{
#ifndef NDEBUG
    if (pkt.size() < PACKET_HEADER_BYTES) {
        RING_ERR("FATAL: TxQueue::send() : pkt.size() < PACKET_HEADER_BYTES");
        std::terminate();
    }
#endif

    // convert to network order: full packet for ctrl, only headers for data
    std::vector<uint8_t> net_pkt = pkt; // don't touch packet, TODO: make non-dual copy version
    if (Packet::isData(net_pkt)) {
        // DEBUG: loss simulation
        if ((++lossSimu_ % 5) == 0) {
            RING_ERR("tx: drop 0x%08x", Packet::getDataSeq(net_pkt));
            return net_pkt.size();
        }
        RING_DBG("tx: 0x%08x", Packet::getDataSeq(net_pkt));
        bswap32(net_pkt.data(), PACKET_HEADER_BYTES);
    }
    else {
        //RING_DBG("tx: ctrl %u", (unsigned)Packet::getCtrlType(net_pkt));
        bswap32(net_pkt.data(), net_pkt.size());
    }

    return tls_->send(net_pkt);
}

template <class Clock>
void
TxQueue<Clock>::update(const typename Clock::time_point& ts, bool reshedule)
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
TxQueue<Clock>::update(bool reshedule)
{
    update(Clock::now(), reshedule);
}

template <class Clock>
void
TxQueue<Clock>::threadJob()
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
        auto pkt = popPacket_(nextTime);
        lk.lock();

        if (pkt) {
            popPacketTime_ = nextTime;
            send(*pkt); // ignore failures
            scheduled_ = true;
        } else {
            RING_WARN("tx: no packet");
            scheduled_ = false;
        }
    }
}

//==================================================================================================

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

//==================================================================================================

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
makeDataConnection(const std::string& account_id, const std::string& peer_id)
{
    auto cnx = std::make_shared<DataConnection>(account_id, peer_id);
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

static constexpr auto SYN_DELAY = std::chrono::milliseconds(10);
static constexpr auto MIN_EXP_DELAY = std::chrono::milliseconds(100);

DataConnection::DataConnection(const std::string& account_id, const std::string& peer_id)
    : id_(get_unique_connection_id())
    , startTime_(Clock::now())
    , statSync_(Clock::now())
    , timeEXPCount_(1)
    , timeRTT_(10 * SYN_DELAY)
    , timeRTTVar_(timeRTT_ / 2)
    , lastRxTime_(Clock::now())
{
    info_.account = account_id;
    info_.peer = peer_id;
    info_.code = DRing::DataTransferCode::CODE_UNKNOWN;

    std::uniform_int_distribution<int32_t> dist(0, SeqNum::MAX);
    initialSeq_ = dist(Manager::instance().getRandomEngine());
    txSeq_ = initialSeq_;
    lastRxAck_ = SeqNum::decrease(initialSeq_);
    RING_DBG("[FTP:%zu] init seq: %08x", id_, initialSeq_);
}

DataConnection::~DataConnection()
{
    disconnect();
}

void
DataConnection::connect(tls::TlsSession* tls)
{
    tls_ = tls;
    maxPayload_ = tls_->getMaxPayload() - PACKET_HEADER_BYTES;
    RING_DBG("[FTP:%lx] maxPayload=%u", id_, maxPayload_);

    txQueue_.reset(new TxQueue<Clock>(tls_, [this](TimePoint& t) { return nextTxPacket(t); }));
    rxQueue_.reset(new RxQueue(*this));
    txLossList_.reset(new LossRecorder());
    rxLossList_.reset(new LossRecorder());

    auto exp_int = timeEXPCount_.load() * (timeRTT_ + 4 * timeRTTVar_) + SYN_DELAY;
    RING_ERR("init exp: %zu", cast_to_us(exp_int).count());
    rxQueue_->addTimedJob(exp_int,
                          [this](RxQueue::Timer& t){
                              unsigned exp_count = timeEXPCount_++;
                              auto exp_int = exp_count * (timeRTT_ + 4 * timeRTTVar_) + SYN_DELAY;
                              if (exp_int < exp_count * MIN_EXP_DELAY)
                                  exp_int = exp_count * MIN_EXP_DELAY;
                              auto next_exp_time = lastRxTime_ + exp_int;
                              //RING_ERR("exp_int: %zu", cast_to_us(exp_int).count());
                              if (txLossList_->empty()) {
                                  // Consider all sent packets before the largest rx ACK as lost
                                  auto first = txBuffer_->getFirstSentPacketSeq();
                                  if (first != SeqNum::INVALID) {
                                      std::lock_guard<std::mutex> lk {ackMutex_};
                                      auto last = SeqNum::decrease(lastRxAck_);
                                      if (first <= last) {
                                          RING_WARN("tx: lost? [0x%08x-0x%08x] (%u)",
                                                    first, last, SeqNum::length(first, last));
                                          txLossList_->insert(first, last);
                                      }
                                  }
                              }
                              sendHeartBeat();
                              lastRxTime_ = Clock::now();
                              t.reschedule(next_exp_time);
                          });

    // Send handshake packet until we get an complete hanshake from peer
    Manager::instance().addTask([this]{ return handshakeJob(); });

    // FOR DEBUG ONLY
    Manager::instance().registerEventHandler(reinterpret_cast<uintptr_t>(this),
                                             [this]{ idleJob(); });
}

void
DataConnection::disconnect()
{
    // FOR DEBUG ONLY
    Manager::instance().unregisterEventHandler(reinterpret_cast<uintptr_t>(this));

    // Stop handshake job
    handshakeComplete_ = true;

    txQueue_.reset();
    rxQueue_.reset();
    txBuffer_.reset();
    rxBuffer_.reset();
    txLossList_.reset();
    rxLossList_.reset();

    tls_ = nullptr;
}

void
DataConnection::idleJob()
{
    while (sendData(std::vector<uint8_t>(maxPayload_, 'x')));

    if (handshakeState_) {
        std::vector<uint8_t> pkt;
        while (rxBuffer_->pop(pkt))
            RING_DBG("app: rx 0x%08x", Packet::getDataSeq(pkt));
    }

    // Statistics output
    std::lock_guard<std::mutex> lk {statMutex_};
    auto now = Clock::now();
    float delta = cast_to_s(now - statSync_).count();
    if (delta > 5) {
        RING_WARN("\nrx: Bytes: %.3fMbits/s, Pkt: %zu, Loss: %zu (%zu NAK), Bad:%zu\ntx: Pkt: %zu (%zu ACK), Ret: %zu, LossLen: %zu",
                  rxBytes_ * 8 / delta / 1e6, rxPkt_, rxLossPkt_, txNAK_, rxBadPkt_, txDataPkt_, rxACK_, txReTxPkt_, txLossList_->length());
        statSync_ = now;
        rxPkt_ = rxLossPkt_ = txNAK_ = rxBytes_ = rxBadPkt_ = txDataPkt_ = txReTxPkt_ = rxACK_ = 0;
    }
}

bool
DataConnection::handshakeJob()
{
    if (handshakeComplete_)
        return false; // remove this job when peer is setup

    // request rate limitation
    if ((Clock::now() - lastHandshakeTime_) > HANDSHAKE_RATE)
        sendHandshake();

    return true;
}

std::shared_ptr<std::vector<uint8_t>>
DataConnection::nextTxPacket(TimePoint& when)
{
    if (!txBuffer_)
        return {};

    auto packet_ts = Clock::now();
    std::shared_ptr<std::vector<uint8_t>> pkt;

    // Retransmit the oldest loss-marked packet in priority
    {
        auto seq = txLossList_->popFirst();
        if (seq != SeqNum::INVALID) {
            if (txBuffer_->retransmit(seq)) {
                RING_WARN("tx: retx seq 0x%08x -> 0x%08x", seq, txSeq_);
                std::lock_guard<std::mutex> lk {statMutex_};
                ++txReTxPkt_;
            } else
                RING_ERR("tx: lost 0x%08x not in sent buf", seq);
        }
    }

    // New packet
    pkt = txBuffer_->nextPacket(txSeq_);
    if (!pkt)
        return {};

    txSeq_ = SeqNum::increase(txSeq_);
    Packet::setTimestamp(*pkt, cast_to_ms(packet_ts - startTime_).count());

    // Delay next call
    when = packet_ts + txDelay_;

    std::lock_guard<std::mutex> lk {statMutex_};
    ++txDataPkt_;

    return pkt;
}

bool
DataConnection::sendData(const std::vector<uint8_t>& payload)
{
    if (!txBuffer_)
        return false;

    std::vector<uint8_t> pkt;
    Packet::initData(pkt, payload.data(), payload.size());
    auto res = txBuffer_->push(std::move(pkt));
    if (!res)
        return false;

    txQueue_->update();
    return true;
}

void
DataConnection::sendNack(const std::array<uint32_t, 2>& data, int len)
{
    std::vector<uint8_t> pkt;
    if (len == 1)
        Packet::initCtrl(pkt, PacketType::NACK, 0, &data[1], 4);
    else
        Packet::initCtrl(pkt, PacketType::NACK, 0, &data[0], 4*len);

    txQueue_->send(pkt);

    std::lock_guard<std::mutex> lk {statMutex_};
    ++txNAK_;
}

void
DataConnection::sendSingleAck()
{
    auto ack = lastRxSeqNum_;
    RING_DBG("tx: ACK 0x%08x", ack);

    std::vector<uint8_t> pkt;
    Packet::initCtrl(pkt, PacketType::ACK, ack);
    txQueue_->send(pkt);
}

void
DataConnection::sendHandshake()
{
    std::array<uint32_t, 3> data;
    data[0] = maxPayload_;
    data[1] = initialSeq_;
    data[2] = handshakeState_;
    std::vector<uint8_t> pkt;
    Packet::initCtrl(pkt, PacketType::HANDSHAKE, PROTOCOL_VERSION, data.data(), 4 * data.size());
    lastHandshakeTime_ = Clock::now();
    txQueue_->send(pkt);
}

void
DataConnection::sendHeartBeat()
{
    std::vector<uint8_t> pkt;
    Packet::initCtrl(pkt, PacketType::HEART_BEAT, 0);
    txQueue_->send(pkt);
}

void
DataConnection::onPeerHandshake(int protocol, const uint32_t* peer_data)
{
    protocol_ = protocol;
    maxPayload_ = std::min(peer_data[0], maxPayload_);
    peerInitSeqNum_ = peer_data[1];
    lastRxSeqNum_ = SeqNum::decrease(peerInitSeqNum_);
    RING_DBG("[FTP:%lx] peer handshake (%u), MSS=%u, ISeq=%08x",
             id_, peer_data[2], maxPayload_, peerInitSeqNum_);
}

void
DataConnection::processPacket(std::vector<uint8_t>&& pkt)
{
    if (pkt.size() < PACKET_HEADER_BYTES) {
        std::lock_guard<std::mutex> lk {statMutex_};
        ++rxBadPkt_;
        return;
    }

    // convert packet into host order
    bswap32(pkt.data(), PACKET_HEADER_BYTES);

    // Drop any data packet until rx data enabled
    if (handshakeState_ == 0) {
        if (Packet::isCtrl(pkt))
            processCtrlPacket(pkt);
        return;
    }

    // Acceptable packet, just store, process later
    rxQueue_->push(std::move(pkt));
}

void
DataConnection::processCtrlPacket(std::vector<uint8_t>& pkt)
{
    timeEXPCount_ = 1;
    lastRxTime_ = Clock::now();

    auto data = Packet::getPayload<uint32_t>(pkt);
    auto data_size = Packet::getPayloadSize<uint32_t>(pkt);
    bswap32(data, 4 * data_size);

    switch (Packet::getCtrlType(pkt)) {
        case PacketType::HANDSHAKE:
        {
            if (handshakeComplete_)
                return;

            auto proto = Packet::getCtrlAddInfo(pkt);
            if (proto != PROTOCOL_VERSION)
                return;

            if (data_size != 3) {
                ++rxBadPkt_;
                return;
            }

            unsigned peer_state = data[2];
            RING_WARN("[FTP:%zu] rx handshake %u (our=%u)", id_, peer_state, handshakeState_.load());

            if (handshakeState_ == 0) {
                onPeerHandshake(PROTOCOL_VERSION, data);
                // enable rx data
                rxBuffer_.reset(new RxBuffer(8192));
                handshakeState_ = 1;
            }

            if (peer_state > 0) {
                // enable tx data
                txBuffer_.reset(new TxDataBuffer(32, maxPayload_));
                handshakeState_ = 2;
                handshakeComplete_ = true;
            }

            if (peer_state < 2)
                sendHandshake();
        }
        break;

        case PacketType::ACK:
        {
            auto ack = Packet::getCtrlAddInfo(pkt);
            {
                std::unique_lock<std::mutex> lk {ackMutex_};
                // drop old or repeated ACK
                if (SeqNum::offset(lastRxAck_, ack) <= 0) {
                    RING_DBG("ACK: 0x%08x (old/repeat)", ack);
                    return;
                }

                RING_DBG("ACK: 0x%08x", ack);
                lastRxAck_ = ack;
                txLossList_->remove(ack);
                lk.unlock();

                txBuffer_->removeTo(ack);
            }

            std::lock_guard<std::mutex> lk {statMutex_};
            ++rxACK_;
        }
        break;

        case PacketType::NACK:
            if (data_size == 1) {
                RING_DBG("NACK: 0x%08x", data[0]);
                txLossList_->insert(data[0]);
            } else if (data_size == 2) {
                RING_DBG("NACK: 0x%08x-0x%08x (%u)", data[0], data[1],
                         SeqNum::length(data[0], data[1]));
                txLossList_->insert(data[0], data[1]);
            }
            else
                RING_WARN("NACK: corrupted (data size = %u)", Packet::getPayloadSize(pkt));
            break;

        default:
            break;
    }
}

void
DataConnection::processDataPacket(std::vector<uint8_t>&& pkt)
{
    timeEXPCount_ = 1;

    auto seq = Packet::getDataSeq(pkt);
    ++rxPktCount_; // Used for ACK and congestion, not statistics

    // check sequence number validity
    auto offset = SeqNum::offset(lastRxSeqNum_, seq);
    if (offset < 0) {
        RING_DBG("rx(0x%08x): dup or invalid", seq);
        return;
    }

    auto payload_size = Packet::getPayloadSize(pkt);

    // record packet for application level
    if (!rxBuffer_->push(std::move(pkt))) {
        RING_DBG("rx(0x%08x): full", seq);
        return;
    }

    RING_DBG("rx(0x%08x): good", seq);

    {
        std::lock_guard<std::mutex> lk {statMutex_};
        ++rxPkt_;
        rxBytes_ += payload_size;
    }

    // loss detection
    const auto waited_rx_seq = SeqNum::increase(lastRxSeqNum_);
    if (SeqNum::cmp(seq, waited_rx_seq) > 0) {
        // Immediat loss report
        std::array<uint32_t, 2> lossdata;
        lossdata[0] = waited_rx_seq;
        lossdata[1] = SeqNum::decrease(seq);
        RING_DBG("rx: lost [0x%08x, 0x%08x]", lossdata[0], lossdata[1]);
        sendNack(lossdata, (waited_rx_seq == SeqNum::decrease(seq)) ? 1 : 2);

        auto loss = SeqNum::length(lastRxSeqNum_, seq) - 2;
        {
            std::lock_guard<std::mutex> lk {statMutex_};
            rxLossPkt_ += loss;
        }
    }

    // update largest rx sequence number
    if (SeqNum::cmp(seq, lastRxSeqNum_) > 0)
        lastRxSeqNum_ = seq;

    // send a single ack at each rx packet
    sendSingleAck();
}

}} // namespace ring::ReliableSocket
