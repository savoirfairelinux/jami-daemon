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

template<class T, std::size_t max_size>
class SafeQueue
{
public:
    using element = T;

    template<class... Args>
    bool emplace(Args&&... args) {
        std::lock_guard<std::mutex> lk {mutex_};
        if (queue_.size() >= max_size)
            return false;
        queue_.emplace(std::forward<Args>(args)...);
        return true;
    }

    T& front() {
        std::lock_guard<std::mutex> lk {mutex_};
        return queue_.front();
    }

    const T& front() const {
        std::lock_guard<std::mutex> lk {mutex_};
        return queue_.front();
    }

    void pop() {
        std::lock_guard<std::mutex> lk {mutex_};
        queue_.pop();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk {mutex_};
        return queue_.size();
    }

    bool empty() const {
        return size() == 0;
    }

private:
    mutable std::mutex mutex_ {};
    std::queue<T> queue_ {};
};

template <typename Clock>
struct TimedJob
{
    using TimePoint = typename Clock::time_point;
    using Callable = std::function<bool(TimePoint& callTime)>;
    TimedJob(const TimedJob::TimePoint& when, const TimedJob::Callable& func)
        : callTime(when), callback(func) {}
    TimePoint callTime {};
    Callable callback {};
};

template <typename Clock>
struct SortTimedJob
{
    constexpr bool operator()(const TimedJob<Clock>& lhs, const TimedJob<Clock>& rhs) const {
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
            return;
        }
    }

    // add new entry
    records_.emplace_back(first, last);
    length_ += SeqNum::length(first, last);
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
            return;
        }
    }

    // add new entry
    records_.emplace_back(seq, seq);
    ++length_ ;
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

    RING_DBG("rem loss rec 0x%08x-0x%08x (0x%08x)", iter->first, iter->second, seq);

    // 1-element record : drop record
    if (iter->first == iter->second) {
        records_.erase(iter);
        return true;
    }

    // start or stop seqnum has to be removed: modify them
    if (iter->first == seq) {
        SeqNum::increase(iter->first);
        return true;
    }
    if (iter->second == seq) {
        SeqNum::decrease(iter->second);
        return true;
    }

    // seqnum is somewhere in the range: split the record at seq position
    Record left {iter->first, SeqNum::decrease(seq)};
    Record right {SeqNum::increase(seq), iter->second};
    records_.insert(iter, left);
    records_.insert(std::next(iter), right);
    records_.erase(iter);

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
    auto seq = records_.front().first;
    remove(seq);
    return seq;
}

//==================================================================================================

class TxBuffer
{
public:
    TxBuffer(unsigned size, unsigned payload_size) : maxPacket_(size), mss_(payload_size) {}

    std::shared_ptr<std::vector<uint8_t>> getSentPacket(SeqNum::seqnum_t seq);
    std::shared_ptr<std::vector<uint8_t>> nextPacket();
    bool push(std::vector<uint8_t>&& packet);
    bool remove(SeqNum::seqnum_t seq);

private:
    unsigned maxPacket_;
    unsigned mss_;
    std::mutex packetsMutex_ {};
    std::queue<std::shared_ptr<std::vector<uint8_t>>> toSend_ {};
    std::list<std::shared_ptr<std::vector<uint8_t>>> sent_ {};
};

std::shared_ptr<std::vector<uint8_t>>
TxBuffer::getSentPacket(SeqNum::seqnum_t seq)
{
    if (seq == SeqNum::INVALID)
        return {};
    std::lock_guard<std::mutex> lk {packetsMutex_};
    // TODO: make an O(1) version
    auto iter = std::find_if(sent_.cbegin(), sent_.cend(),
                             [seq](const std::shared_ptr<std::vector<uint8_t>>& pkt){
                                 return seq == Packet::getDataSeq(*pkt);
                             });
    if (iter == sent_.cend())
        return {};
    return *iter;
}

bool
TxBuffer::push(std::vector<uint8_t>&& packet)
{
    std::lock_guard<std::mutex> lk {packetsMutex_};
    if (toSend_.size() + sent_.size() == maxPacket_)
        return false;
    toSend_.emplace(std::make_shared<std::vector<uint8_t>>());
    auto& ptr = toSend_.back();
    *ptr = std::move(packet);
    return true;
}

std::shared_ptr<std::vector<uint8_t>>
TxBuffer::nextPacket()
{
    std::lock_guard<std::mutex> lk {packetsMutex_};
    if (toSend_.empty())
        return {};
    sent_.emplace_front(toSend_.front());
    toSend_.pop();
    return sent_.front();
}

bool
TxBuffer::remove(SeqNum::seqnum_t seq)
{
    if (seq == SeqNum::INVALID)
        return false;
    std::lock_guard<std::mutex> lk {packetsMutex_};
    // TODO: make an O(1) version
    bool found = false;
    sent_.remove_if([seq, &found](const std::shared_ptr<std::vector<uint8_t>>& pkt) {
            found = seq == Packet::getDataSeq(*pkt);
            return found;
        });
    return found;
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
        //RING_DBG("tx: 0x%08x", Packet::getDataSeq(net_pkt));
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

        TimePoint nextTime;
        if (auto pkt = popPacket_(nextTime)) {
            popPacketTime_ = nextTime;
            send(*pkt); // ignore failures
            scheduled_ = true;
        } else
            scheduled_ = false;
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
};

bool
RxBuffer::push(std::vector<uint8_t>&& pkt)
{
    std::lock_guard<std::mutex> lk {pktMutex_};

    // limit number of stored packets
    if (!packets_.empty()) {
        auto seq = Packet::getDataSeq(pkt);
        if (SeqNum::length(Packet::getDataSeq(packets_.back()), seq) > size_)
            return false;
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
    pkt = std::move(packets_.back());
    packets_.pop_back();
    return true;
}

//==================================================================================================

class RxQueue
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using Timer = TimedJob<Clock>;

    RxQueue(DataConnection& dc);
    ~RxQueue();

    void push(std::vector<uint8_t>&& pkt);

private:
    void threadJob();
    void addTimedJob(const Timer::TimePoint&, const Timer::Callable&);

    DataConnection& dc_;

    std::mutex jobMutex_ {};
    std::condition_variable jobCv_ {};
    std::priority_queue<Timer,
                        std::vector<Timer>,
                        SortTimedJob<Clock>> timerQueue_;
    SafeQueue<std::vector<uint8_t>, 1000> rxQueue_ {};

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
    if (rxQueue_.emplace(std::move(pkt)))
        jobCv_.notify_one();
}

void
RxQueue::addTimedJob(const Timer::TimePoint& when, const Timer::Callable& func)
{
    std::lock_guard<std::mutex> lk {jobMutex_};
    timerQueue_.emplace(when, func);
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
            if (!timerQueue_.empty()) {
                auto when = timerQueue_.top().callTime;
                if (!jobCv_.wait_until(lk, when, [this, when] {
                            return !running_
                                or !rxQueue_.empty()
                                or timerQueue_.top().callTime != when;
                        })) {
                    // handle all exhausted timers, rescheduled timer are re-instered after
                    // the end of the loop to be handled at next thread loop
                    decltype(timerQueue_) tmp_timer_queue;
                    std::queue<Timer>  rescheduled_timers;
                    std::swap(timerQueue_, tmp_timer_queue);
                    lk.unlock();
                    auto now = Clock::now();
                    while (!tmp_timer_queue.empty()) {
                        if (!running_)
                            return;
                        auto timer = tmp_timer_queue.top();
                        tmp_timer_queue.pop();
                        when = timer.callTime;
                        if (when > now)
                            break;
                        if (timer.callback(when)) {
                            timer.callTime = when;
                            rescheduled_timers.push(timer);
                        }
                    }
                    lk.lock();

                    // Re-insert re-scheduled timers
                    while (!rescheduled_timers.empty()) {
                        tmp_timer_queue.emplace(std::move(rescheduled_timers.front()));
                        rescheduled_timers.pop();
                    }

                    // Loop on public queue to instert into temporary queue
                    // as first one as more chance to be empty.
                    while (!timerQueue_.empty()) {
                        tmp_timer_queue.emplace(std::move(timerQueue_.top()));
                        timerQueue_.pop();
                    }
                    std::swap(timerQueue_, tmp_timer_queue);
                }
            } else
                jobCv_.wait(lk, [this] { return !running_ or !rxQueue_.empty(); });
        }

        // Process incoming packets
        //int count = 10; // limit number of packet processed by iteration loop to have a chance to loop other the whole process
        while (running_ and !rxQueue_.empty()) {
            auto& pkt = rxQueue_.front();
            if (Packet::isCtrl(pkt))
                dc_.processCtrlPacket(pkt);
            else
                dc_.processDataPacket(std::move(pkt));
            rxQueue_.pop();

            //if (--count == 0)
            //    break;
        }
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

DataConnection::DataConnection(const std::string& account_id, const std::string& peer_id)
    : id_(get_unique_connection_id())
    , startTime_(Clock::now())
    , statSync_(Clock::now())
{
    info_.account = account_id;
    info_.peer = peer_id;
    info_.code = DRing::DataTransferCode::CODE_UNKNOWN;

    std::uniform_int_distribution<int32_t> dist(0, SeqNum::MAX);
    initialSeq_ = dist(Manager::instance().getRandomEngine());
    txSeq_ = initialSeq_;
    lastRxAck_ = SeqNum::decrease(initialSeq_);
    RING_DBG("init seq: %08x", initialSeq_);
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
        while (rxBuffer_->pop(pkt));
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
    std::shared_ptr<std::vector<uint8_t>> pkt;

    if (!txBuffer_)
        return {};

    auto packet_ts = Clock::now();

    // Priority to lost packets
    {
        std::lock_guard<std::mutex> ackLock {ackMutex_};
        pkt = txBuffer_->getSentPacket(txLossList_->popFirst());
    }

    // New packet
    if (!pkt) {
        pkt = txBuffer_->nextPacket();
        if (!pkt)
            return {};
        Packet::setDataSeq(*pkt, txSeq_);
        txSeq_ = SeqNum::increase(txSeq_);
    } else {
        std::lock_guard<std::mutex> lk {statMutex_};
        ++txReTxPkt_;
    }

    Packet::setTimestamp(*pkt, cast_to_ms(packet_ts - startTime_).count());

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
    // ACK sequence is the last rx seq if no loss, or the lost seq minus 1
    SeqNum::seqnum_t ack;
    if (rxLossList_->empty())
        ack = lastRxSeqNum_;
    else
        ack = SeqNum::decrease(rxLossList_->getFirst());

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
DataConnection::onPeerHandshake(int protocol, const uint32_t* peer_data)
{
    protocol_ = protocol;
    maxPayload_ = std::min(peer_data[0], maxPayload_);
    peerInitSeqNum_ = peer_data[1];
    lastRxSeqNum_ = SeqNum::decrease(peerInitSeqNum_);
    lastTxAck_ = peerInitSeqNum_;
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
                txBuffer_.reset(new TxBuffer(32, maxPayload_));
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
                std::lock_guard<std::mutex> ackLock {ackMutex_};

                // drop old or repeated ACK
                //RING_DBG("rx: cmp(0x%08x, 0x%08x)", lastRxAck_, ack, SeqNum::offset(lastRxAck_, ack));
                if (SeqNum::offset(lastRxAck_, ack) <= 0)
                    return;

                lastRxAck_ = ack;
                //RING_DBG("rx: ACK 0x%08x", ack);

                txBuffer_->remove(ack);
                txLossList_->remove(ack);
            }

            std::lock_guard<std::mutex> lk {statMutex_};
            ++rxACK_;
        }
        break;

        case PacketType::NACK:
            if (data_size == 1) {
                RING_DBG("rx: NACK 0x%08x", data[0]);
                txLossList_->insert(data[0]);
            } else if (data_size == 2) {
                RING_DBG("rx: NACK 0x%08x-0x%08x (%u)", data[0], data[1],
                         SeqNum::length(data[0], data[1]));
                txLossList_->insert(data[0], data[1]);
            }
            else
                RING_WARN("rx: NACK corrupted (data size = %u)", Packet::getPayloadSize(pkt));
            break;

        default:
            break;
    }
}

void
DataConnection::processDataPacket(std::vector<uint8_t>&& pkt)
{
    auto seq = Packet::getDataSeq(pkt);
    ++rxPktCount_; // Used for ACK and congestion, not statistics

    //RING_DBG("rx: 0x%08x", seq);

    // check sequence number validity
    auto offset = SeqNum::offset(lastTxAck_, seq);
    if (offset < 0)
        return;

    auto payload_size = Packet::getPayloadSize(pkt);

    // record packet for application level
    if (!rxBuffer_->push(std::move(pkt)))
        return;

    {
        std::lock_guard<std::mutex> lk {statMutex_};
        ++rxPkt_;
        rxBytes_ += payload_size;
    }

    // loss detection
    const auto waited_rx_seq = SeqNum::increase(lastRxSeqNum_);
    if (SeqNum::cmp(seq, waited_rx_seq) > 0) {
        // record loss range
        rxLossList_->insert(waited_rx_seq, SeqNum::decrease(seq));

        // Immediat loss report
        std::array<uint32_t, 2> lossdata;
        lossdata[0] = waited_rx_seq;
        lossdata[1] = SeqNum::decrease(seq);
        sendNack(lossdata, (waited_rx_seq == SeqNum::decrease(seq)) ? 1 : 2);

        auto loss = SeqNum::length(lastRxSeqNum_, seq) - 2;
        {
            std::lock_guard<std::mutex> lk {statMutex_};
            rxLossPkt_ += loss;
        }
    }

    // update largest rx sequence number or remove retransmitted packet from loss list
    if (SeqNum::cmp(seq, lastRxSeqNum_) > 0)
        lastRxSeqNum_ = seq;
    else
        rxLossList_->remove(seq);

    // send a single ack at each rx packet
    sendSingleAck();
}

}} // namespace ring::ReliableSocket
