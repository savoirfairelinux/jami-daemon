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
#include <cmath>
#include <cerrno>

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
    constexpr bool operator()(const TimedJob<Queue, Clock>& lhs,
                              const TimedJob<Queue, Clock>& rhs) const {
        return lhs.callTime > rhs.callTime;
    }
};

template< class T >
constexpr const T& clamp( const T& a, const T& x, const T& b )
{
    return std::max(a, std::min(x, b));
}

//==================================================================================================
// Constants

static constexpr unsigned TX_BUFFER_SIZE = 10; // Maximal number of packet waiting for transmission
static constexpr unsigned MAX_ACK_CT = 15; // Channel packet format uses 4 bits to encode AckCt
static constexpr unsigned ACK_MAX_DELAY = 4; // Maximal number of received packet before sending an ACK
static constexpr auto ACK_DELAY = std::chrono::milliseconds(10); // ACK after this delay if no ACK send elsewhere
static constexpr auto HEARTBEAT_DELAY = std::chrono::seconds(60); // Heatbeat packets pacing
static constexpr auto MAX_RTT = std::chrono::seconds(10); // Limit RTT computation to this value
static constexpr auto MAX_TX_DELAY = std::chrono::milliseconds(100); // Maximum transmission pacing

//==================================================================================================

namespace Packet
{
// Wire format: all words are given in network order (bigendian).
//
// The protocol has been developped using some concepts from following existing works:
// UDT: relations between concepts
// SST: monotonic TSR, ACK per packet
// QUIC: variable length fields, protocol version, multi-frame per packet
//
// This protocol doesn't add any cryptocraphic layer, it is designed to be overlayed
// on top of a secure transport like DTLS, dedicated to this purpose.
// The goal is to keep fields simple, easy to parse (related fields are 32-bits word based).
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
//  | AckCt |          ASN = Acknowledgment Sequence Number         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  These packets use the common channel header without any payloads and all flags bits set to 0.
//  This implies they can be sent ONLY if protocol version is negotiated.
//  TSN, AckCt and ASN are processed normally.
//
//
// Generic frame format (channel flags 1 is set):
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
// Stream frame format:
//
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |1|x d o o o s s|  ExtraFlags   |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | SID (8, 16, 24 or 32 bits) variable length ~  |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | Offset (0, 16, 24, 32, 40, 48, 56 or 64 bits) variable length ~ |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | DataLength (0 or 16 bits) variable length ~ |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | Data, variable length (not word aligned) ~  |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Flags:
//      bit x: set if ExtraFlags presents (8 bits)
//      bits ooo: offset field size
//      bit d: set if data lenght field present (16 bits)
//      bits ss: SID (StreamId) field size
//
//   ExtraFlags:
//      An optional  8-bits extra flags, only present if flag x is set.
//      These bits are defined as follow:
//
//		 0 1 2 3 4 5 6 7
//		+-+-+-+-+-+-+-+-+
//		|F P B E r r r r|
//		+-+-+-+-+-+-+-+-+
//
//			bit F: end of stream, set on last bytes for this stream
//			bit P: set if data must be pushed to application as soon as possible
//			bit B: this block starts a message
//			bit E: this block ends a message
//			bits r: reserved, must be set to 0
//
//   SID:
//      Stream Identifier, 0 is reserved and not used in packets
//
//   Offset:
//		Byte offset for data inside this frame
//
//   DataLength:
//      if d bit is set, represent the number of data bytes
//
//

enum ChannelFlags {
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
inline void
setChannelFlag(std::vector<T>& vec, Packet::ChannelFlags bit)
{
    const uint32_t mask = htonl((1u << (28 + bit)) & 0xF0000000);
    auto p = &reinterpret_cast<uint32_t*>(vec.data())[0];
    *p = (*p & ~mask) | mask;
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
inline uint8_t*
initChannel(std::vector<T>& vec, std::size_t payload_size=0, const void* payload=nullptr)
{
    vec.resize(PACKET_HEADER_BYTES + payload_size);
    auto pl_begin = reinterpret_cast<uint8_t*>(vec.data()) + PACKET_HEADER_BYTES;
    if (payload)
        return std::copy_n(reinterpret_cast<const uint8_t*>(payload), payload_size, pl_begin);
    return pl_begin;
}

template <typename T>
inline void
dump(const std::vector<T>& vec)
{
    auto p = reinterpret_cast<const uint32_t*>(vec.data());
    for (std::size_t i=0; i < (vec.size() * sizeof(T) / 4); ++i, ++p)
        RING_ERR("[%zu] %08x", i, ntohl(*p));
}

template <typename T>
inline void
initProto(std::vector<T>& vec, uint64_t id, uint32_t proto)
{
    std::array<uint32_t, 3> data;
    data[0] = htonl(id >> 32);
    data[1] = htonl(id & 0xFFFFFFFF);
    data[2] = htonl(proto);
    Packet::initChannel(vec, data.size() * 4, data.data());
    Packet::setChannelFlag(vec, ChannelFlags::INIT);
}

template <typename T>
inline void
initHeartbeat(std::vector<T>& vec)
{
    Packet::initChannel(vec);
}

template <typename T>
inline uint8_t*
firstFramePtr(std::vector<T>& vec)
{
    return reinterpret_cast<uint8_t*>(vec.data()) + PACKET_HEADER_BYTES;
}

enum FrameFlags {
    FRAMEF_STREAM = 0x80,
    FRAMEF_EXTRA  = 0x40,
    FRAMEF_DATA   = 0x20,
};

// Unpack a frame segment by reading StreamId, Offset and DataLength fields.
// Return a pointer on first frame's payload byte, or first byte of next frame if no payload.
inline uint8_t*
unpackStreamFrame(uint8_t* frame, uint8_t& extra_flags, uint32_t& sid, uint64_t& offset,
                  uint16_t& data_length)
{
    const bool x = frame[0] & FRAMEF_EXTRA;
    const bool d = frame[0] & FRAMEF_DATA;
    const auto o = (frame[0] >> 2) & 0x7;
    const auto s = frame[0] & 0x3;

    if (x)
        extra_flags = *(++frame);
    else
        extra_flags = 0;

    // read Stream Id
    sid = *(++frame);
    if (s > 0)
        sid = (sid << 8) + *(++frame);
    if (s > 1)
        sid = (sid << 8) + *(++frame);
    if (s > 2)
        sid = (sid << 8) + *(++frame);

    // read offset (if exists)
    if (o > 0) {
        offset = *(++frame);
        offset = (offset << 8) + *(++frame);
        if (o > 1)
            offset = (offset << 8) + *(++frame);
        if (o > 2)
            offset = (offset << 8) + *(++frame);
        if (o > 3)
            offset = (offset << 8) + *(++frame);
        if (o > 4)
            offset = (offset << 8) + *(++frame);
        if (o > 5)
            offset = (offset << 8) + *(++frame);
        if (o > 6)
            offset = (offset << 8) + *(++frame);
    } else
        offset = 0;

    // bit 'd' set => DataLength field
    if (d) {
        data_length = *(++frame);
        data_length = (data_length << 8) + *(++frame);
        return frame; // first byte of payload
    }

    data_length = 0;
    return frame; // first byte of next frame
}

unsigned
getStreamFrameHeaderSize(uint8_t& sid_size, uint8_t& off_size, uint8_t extra_flags, uint32_t sid,
                         uint64_t offset=0, uint16_t data_length=0)
{
    sid_size = (std::log2(sid) / 8) + 1;
    unsigned frame_size = 1 + sid_size;
    if (extra_flags)
        ++frame_size;
    if (data_length)
        frame_size += 2;
    if (offset) {
        off_size = clamp(2, static_cast<int>(std::log2(offset)) / 8, 8);
        frame_size += off_size;
        --off_size;
    } else
        off_size = 0;
    return frame_size;
}

inline uint8_t*
packStreamFrame(std::vector<uint8_t>& vec, uint8_t extra_flags, uint32_t sid,
                uint64_t offset=0, uint16_t data_length=0, const uint8_t* data=nullptr)
{
    // Compute total frame size
    uint8_t sid_size, off_size;
    unsigned frame_size = getStreamFrameHeaderSize(sid_size, off_size, extra_flags, sid,
                                                   offset, data_length);
    frame_size += data_length;

    // Init a stream frame packet
    uint8_t* frame_ptr = Packet::initChannel(vec, frame_size);
    Packet::setChannelFlag(vec, ChannelFlags::FRAME);

    // Fill frame bytes
    frame_ptr[0] = FRAMEF_STREAM | (off_size << 2) | (sid_size - 1);

    if (data_length)
        frame_ptr[0] |= FRAMEF_DATA;

    if (extra_flags) {
        *(frame_ptr++) |= FRAMEF_EXTRA;
        *(frame_ptr++) = extra_flags;
    } else
        ++frame_ptr;

    for (int i = sid_size-1; i >= 0; --i)
        *(frame_ptr++) = (sid >> (i*8)) & 0xff;

    if (offset) {
        for (int i = off_size; i >= 0; --i)
            *(frame_ptr++) = (offset >> (i*8)) & 0xff;
    }

    if (data_length) {
        *(frame_ptr++) = data_length >> 8;
        *(frame_ptr++) = data_length & 0xff;
    }

    if (data)
        return std::copy_n(data, data_length, frame_ptr);

    return frame_ptr;
}

} // namespace ring::ReliableSocket::Packet

//==================================================================================================

struct TxPacket {
    std::vector<uint8_t> bytes {};
    DataStream* stream {nullptr};
};

//==================================================================================================

class TxFrameBuffer
{
public:
    TxFrameBuffer(unsigned size) : maxPacket_(size) {}

    bool push(std::shared_ptr<TxPacket> pkt);
    std::shared_ptr<TxPacket> pop();
    void flush();
    std::size_t available();

private:
    unsigned maxPacket_;
    std::mutex accMtx_ {};
    std::queue<std::shared_ptr<TxPacket>> outQ_ {}; // packets to transmit
};

bool
TxFrameBuffer::push(std::shared_ptr<TxPacket> pkt)
{
    std::lock_guard<std::mutex> lk {accMtx_};
    if (outQ_.size() == maxPacket_)
        return false; // full
    outQ_.emplace(pkt);
    return true;
}

std::shared_ptr<TxPacket>
TxFrameBuffer::pop()
{
    std::lock_guard<std::mutex> lk {accMtx_};
    if (outQ_.empty())
        return {};
    auto pkt_ptr = outQ_.front();
    outQ_.pop();
    return pkt_ptr;
}

void
TxFrameBuffer::flush()
{
    std::lock_guard<std::mutex> lk {accMtx_};
    while (!outQ_.empty())
        outQ_.pop();
}

std::size_t
TxFrameBuffer::available()
{
    return maxPacket_ - outQ_.size();
}

//==================================================================================================

template <class Clock>
class TxScheduler
{
public:
    using Duration = typename Clock::duration;
    using TimePoint = typename Clock::time_point;

    TxScheduler(DataConnection& parent);
    ~TxScheduler();

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
            DataConnection::SeqNum pkt_seq;
            if (dc_.sendPkt(pkt->bytes, pkt_seq) > 0)
                dc_.savePkt(pkt_seq, pkt);
            scheduled_ = true;
        } else {
            //RING_WARN("tx: no packet");
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

class RxQueue
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Timer = TimedJob<RxQueue, Clock>;

    RxQueue(DataConnection& dc);
    ~RxQueue();

    bool push(std::vector<uint8_t>&& pkt);
    void attachStream(std::shared_ptr<DataStream> stream);
    void detachStream(std::shared_ptr<DataStream> stream);

#if 0
    template <typename T>
    void addTimedJob(const T& delay, const Timer::Callable& callback);

    template <typename T>
    void addTimedJob(const T& delay, Timer::Callable&& callback);

    template <typename T>
    void addTimedJob(const T& delay, Timer&& timer);

    void addTimedJob(const TimePoint& when, Timer&& timer);
#endif

private:
    static constexpr unsigned RX_QUEUE_SIZE {8192}; // unit = packet
    void threadJob();

    DataConnection& dc_;

    std::unordered_map<StreamId, std::shared_ptr<DataStream>> streamMap_ {};

    std::mutex jobMutex_ {};
    std::condition_variable jobCv_ {};
    std::priority_queue<Timer,
                        std::vector<Timer>,
                        SortTimedJob<RxQueue, Clock>> timerQueue_;
    std::deque<std::vector<uint8_t>> pktQueue_ {};

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
RxQueue::threadJob()
{
    while (running_) {
        std::unique_lock<std::mutex> lk {jobMutex_};

        jobCv_.wait(lk, [this]{ return !running_ or !pktQueue_.empty(); });

#if 0
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
#endif

        // loop on received packets
        decltype(pktQueue_) queue;
        std::swap(pktQueue_, queue);

        // permit reception of new packets
        lk.unlock();

        for (auto& pkt : queue) {
            if (!running_)
                return;

            // loop on frame segments
            // unpack each frame data and pass payload or event to the corresponding DataStream
            auto frame = Packet::firstFramePtr(pkt);
            const auto end_frame = pkt.data() + pkt.size();
            while (frame < end_frame) {
                uint8_t extra_flags;
                uint16_t data_length;
                uint32_t sid;
                uint64_t offset;
                frame = Packet::unpackStreamFrame(frame, extra_flags, sid, offset, data_length);
                const auto it = streamMap_.find(sid);
                if (it != streamMap_.cend()) {
                    if (data_length)
                        it->second->onRxPayload(frame, data_length, offset);
                    if (extra_flags & 0x80)
                        it->second->onCloseStream();
                }
                if (data_length)
                    frame += data_length;
                break;
            }
        }
    }
}

void
RxQueue::attachStream(std::shared_ptr<DataStream> stream)
{
    std::lock_guard<std::mutex> lk {jobMutex_};
    streamMap_.emplace(stream->getId(), stream);
}

void
RxQueue::detachStream(std::shared_ptr<DataStream> stream)
{
    std::lock_guard<std::mutex> lk {jobMutex_};
    streamMap_.erase(stream->getId());
}

bool
RxQueue::push(std::vector<uint8_t>&& pkt)
{
    std::lock_guard<std::mutex> lk {jobMutex_};
    if (pktQueue_.size() >= RX_QUEUE_SIZE)
        return false;

    pktQueue_.emplace_back(std::move(pkt));
    jobCv_.notify_one();
    return true;
}

#if 0
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
    std::lock_guard<std::mutex> lk {txMtx_};

    tls_ = tls;
    maxPayload_ = tls_->getMaxPayload();
    RING_DBG("[FTP:%lx] maxPayload=%u", id_, maxPayload_);

    txQueue_.reset(new TxScheduler<Clock>(*this));
    txBuffer_.reset(new TxFrameBuffer(TX_BUFFER_SIZE));

    rxQueue_.reset(new RxQueue(*this));

    if (isClient_)
        txQueue_->update();

    // FOR DEBUG ONLY
    Manager::instance().registerEventHandler(reinterpret_cast<uintptr_t>(this),
                                             [this]{ idleJob(); });

    if (isClient_) {
        testMutex_.lock();
        std::thread t([this]{ testJob(); });
        t.detach();
    }
}

void
DataConnection::testJob()
{
    std::array<uint8_t, 60000> buf;
    auto ds = std::make_shared<DataStream>(1);
    attachStream(ds);

    while (!testMutex_.try_lock()) {
        if (ds->sendData(buf.data(), buf.size()) < 0) {
            RING_ERR("sendData() error, errno=%s", strerror(errno));
            break;
        }
    }
    RING_WARN("test finished");
}

void
DataConnection::disconnect()
{
    {
        std::lock_guard<std::mutex> lk {txMtx_};

        // FOR DEBUG ONLY
        Manager::instance().unregisterEventHandler(reinterpret_cast<uintptr_t>(this));

        txQueue_.reset();
        txBuffer_.reset();
        rxQueue_.reset();
        tls_ = nullptr;
    }

    testMutex_.unlock();
    ioCv_.notify_all();

    // for testJob
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void
DataConnection::idleJob()
{
    if (protocol_) {
        // Send Heartbeat
        // TODO: put it into TxScheduler
        auto now = Clock::now();
        if (now - lastHeartbeat_ >= HEARTBEAT_DELAY) {
            lastHeartbeat_ = now;
            sendHeartbeat();
        }

#if 0
        if (isClient_) {
            auto pkt = std::make_shared<TxPacket>();
            // Create the biggest frame packet (zero initialized)
            uint8_t sid_size, off_size;
            uint16_t data_length = maxPayload_ - Packet::getStreamFrameHeaderSize(sid_size, off_size, 0, 1, 0, 0xffff);
            //uint16_t data_length = 1000;
            Packet::packStreamFrame(pkt->bytes, 0, 1, 0, data_length - PACKET_HEADER_BYTES);
            for (unsigned i=0; i < TX_BUFFER_SIZE; ++i)
                pushPacket(pkt);
        }
#endif
    }

    // Statistics output
    std::lock_guard<std::mutex> lk {statMutex_};
    auto now = Clock::now();
    float delta = cast_to_s(now - statSync_).count();
    if (delta > 5) {
        auto cum_rtt = cast_to_ms(cumRtt_.load()).count();
        auto cum_pps = cumPps_.load();
        RING_WARN("[TX] RTT (ms): %lu, RTT var.: %.1f, PPS: %.1f, Power: %.1f, Loss: %u",
                  cum_rtt, (cumRttVar_.load()) * 1e-3, cum_pps, cum_pps / cum_rtt,
                  static_cast<unsigned>(cumLoss_.load() / delta));
        RING_WARN("[RX] Bytes: %.3fMbits/s, Bad: %zu, Rx Loss/OoO: %zu",
                  rxBytes_.load() * 8 / delta / 1e6, rxBadPkt_.load(), rxLossPkt_.load());
        rxBytes_ = rxBadPkt_ = rxLossPkt_ = 0;
        cumRtt_ = std::chrono::milliseconds(500);
        cumRttVar_ = 0;
        cumPps_ = 0;
        cumLoss_= 0;
        statSync_ = now;
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

std::shared_ptr<TxPacket>
DataConnection::nextTxPacket(TimePoint& when)
{
    auto packet_ts = Clock::now();
    std::shared_ptr<TxPacket> pkt;

    if (!protocol_) {
        // non-negotiated server does nothing here
        if (!isClient_)
            return {};

        // Client: force INIT packet until negotiated
        pkt = std::make_shared<TxPacket>();
        Packet::initProto(pkt->bytes, id_, PROTOCOL_VERSION);
    } else {
        pkt = txBuffer_->pop();
    }

    if (!pkt)
        return {};

    // Delay next call
    when = packet_ts + txDelay_;
    return pkt;
}

bool
DataConnection::pushPacket(std::shared_ptr<TxPacket> pkt)
{
    auto res = txBuffer_->push(pkt);
    txQueue_->update();
    return res;
}

ssize_t
DataConnection::sendPkt(std::vector<uint8_t>& pkt, SeqNum& pkt_seq,
                        SeqNum ack_seq, unsigned ack_count)
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
    if (!txSeq_) {
        RING_ERR("[FTP:%zu] sequence number overflow", id_);
        stopConnection();
        return -1;
    }

    pkt_seq = txSeq_++;
    if (pkt_seq == netStatSeq_) {
        // Reset network statistics
        netStatTime_ = Clock::now();
        cumAckCount_ = 0;
        netStatSent_ = pkt_seq - rxAckSeq_; // number of packets sent since last acknowledgment
    }

    Packet::setTsn(pkt, pkt_seq);
    Packet::setAsnAckCnt(pkt, ack_seq, ack_count);

    if (delayedAck_) {
        delayedAck_ = 0;
        // TODO: stop ack timer
    }

    //Packet::dump(pkt);
    return tls_->send(pkt);
}

ssize_t
DataConnection::sendPkt(std::vector<uint8_t>& pkt, SeqNum& pkt_seq)
{
    SeqNum ack_seq;
    unsigned ack_count;
    getTxAckState(ack_seq, ack_count);
    return sendPkt(pkt, pkt_seq, ack_seq, ack_count);
}

ssize_t
DataConnection::sendPkt(std::vector<uint8_t>& pkt)
{
    SeqNum pkt_seq; // needed but not used
    SeqNum ack_seq;
    unsigned ack_count;
    getTxAckState(ack_seq, ack_count);
    return sendPkt(pkt, pkt_seq, ack_seq, ack_count);
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
    RING_DBG("[FTP:%zu] send heartbeat", id_);
    sendPkt(pkt);
}

void
DataConnection::sendAck(SeqNum ack_seq, unsigned ack_count)
{
    SeqNum pkt_seq; // needed but not used
    std::vector<uint8_t> pkt;
    Packet::initHeartbeat(pkt);
    sendPkt(pkt, pkt_seq, ack_seq, ack_count);
}

inline void
DataConnection::savePkt(SeqNum seq, std::shared_ptr<TxPacket> pkt)
{
    std::lock_guard<std::mutex> lk {ackWaitMapMutex_};
    ackWaitMap_.emplace(seq, pkt);
}

void
DataConnection::onAckPkt(SeqNum seq, unsigned count)
{
    std::lock_guard<std::mutex> lk {ackWaitMapMutex_};
    do {
        auto pair = ackWaitMap_.find(seq - (--count));
        if (pair == ackWaitMap_.cend())
            continue;
        auto pkt = pair->second;
        ackWaitMap_.erase(seq);
        if (pkt->stream)
            pkt->stream->onAck(seq);
    } while (count);
}

DataConnection::SeqNum
DataConnection::processTransmissionFields(const std::vector<uint8_t>& pkt)
{
    // Infer the global packet sequence number from its partial representation (TSN)
    uint32_t peer_tsn = Packet::getTsn(pkt);
    int32_t seq_delta = ((static_cast<int32_t>(peer_tsn) << 4) - (static_cast<int32_t>(rxSeq_) << 4)) >> 4;
    SeqNum pkt_seq = rxSeq_ + seq_delta;

    // As sequence progression are monotonic without wraparound,
    // a negative seq delta means out-of-order or duplicate packets.
    // Dup and too old OOO packets are dropped.
    if (seq_delta <= 0) {
        if (seq_delta <= -MISS_ORDERING_LIMIT) {
            RING_WARN("[FTP:%zu] drop too old pkt %016lx", id_, pkt_seq);
            ++rxOldPkt_;
            return 0;
        }

        // Duplicate?
        if (rxMask_[-seq_delta]) {
            RING_WARN("[FTP:%zu] drop dup pkt %016lx", id_, pkt_seq);
            ++rxDupPkt_;
            return 0;
        }

        // Accepted out-of-order packet
        RING_DBG("[FTP:%zu] OOO detected (%u)", id_, -seq_delta);
        rxMask_.set(-seq_delta);
    } else { // seq_delta > 0
        // Make sure we don't wraparound, all the design is based on this assumption.
        // This should never append as our tx engine protect against that.
        // But... in case of a foolish peer.
        if (pkt_seq < rxSeq_) {
            RING_ERR("[FTP:%zu] fatal error, rx sequence number has wraparound!", id_);
            ++rxBadPkt_;
            stopConnection();
            return 0;
        }

        // Increment bigger than 1 means possible loss or OoO.
        // At this stage, just consider them as lost
        if (seq_delta > 1) {
            rxLossPkt_ += seq_delta - 1;
        }

        // This is a new packet: update our internal rx state
        rxSeq_ = pkt_seq;
        rxMask_ <<= seq_delta;
        rxMask_.set(0);
    }

    return pkt_seq;
}

void
DataConnection::processAcknowledgmentFields(const std::vector<uint8_t>& pkt)
{
    // As for TSN, infer the global acknowledgment sequence number of this packet
    uint32_t pkt_asn;
    uint8_t pkt_ackct;
    Packet::getAsnAckCt(pkt, pkt_asn, pkt_ackct);
    int32_t ack_delta = ((static_cast<int32_t>(pkt_asn) << 4) - (static_cast<int32_t>(rxAckSeq_) << 4)) >> 4;
    SeqNum ack_seq = rxAckSeq_ + ack_delta;

    unsigned ack_count = 0; // counter of number of packets acknowlegded by this one

    // New acknowledgments?
    if (ack_delta > 0) {
        // Update out internal ack state
        {
            std::lock_guard<std::mutex> lk {txMtx_};
            rxAckSeq_ = ack_seq;
        }
        rxAckMask_ <<= ack_delta;

        // Compute number of contiguous acknowledged sequences and record that in the ack window
        // note: ackct = 0 if 1 packet is acknowledged
        // note2: we don't take in account possible OoO packets that contains ACK info
        ack_count += std::min(ack_delta, pkt_ackct + 1);
        rxAckMask_ |= (1 << ack_count) - 1; // set all bits from ack_count-1 downto 0

        // Acknowledge all contiguous packets
        onAckPkt(ack_seq, ack_count);

        // TODO: drop too late ack packets

        ack_delta = 0; // reset the delta before OoO processing just below
    }

    // Handle acknowledgement data in a OoO packet.
    // note: an OoO packet acknowledges an in-order packet.
    unsigned long ack_mask = (1ul << pkt_ackct) - 1;
    if ((rxAckMask_.to_ulong() & ack_mask) != ack_mask) {
        for (unsigned i=0; i < pkt_ackct; ++i) {
            unsigned bit = -ack_delta + i;
            if (bit >= rxAckMask_.size())
                break;
            if (!rxAckMask_[bit]) {
                rxAckMask_.set(bit);
                ++ack_count;

                // Acknowledge one packet
                onAckPkt(ack_seq);
            }
        }
    }

    cumAckCount_ += ack_count;

    // Network statistics is trigged when the marked sequence as made (or supposed to) a round-trip
    if (ack_seq >= netStatSeq_) {
        std::lock_guard<std::mutex> lk {txMtx_};
        netStatSeq_ = txSeq_; // next trig point

        // RTT (of the group of acked sequences), clamped to [1, MAX_RTT]
        auto rtt = cast_to_us(Clock::now() - netStatTime_).count();
        rtt = std::max(1l, std::min(rtt, cast_to_us(MAX_RTT).count()));
        auto cum_rtt_us = ((cumRtt_.load().count() * 7) + rtt) / 8;
        cumRtt_ = std::chrono::microseconds(cum_rtt_us);

        // RTT variance measure
		float rtt_var = fabsf(rtt - cum_rtt_us);
		cumRttVar_ = ((cumRttVar_ * 7.f) + rtt_var) / 8.f;

        // Number of unique ACK during this round-trip
        float pps = cumAckCount_ * 1000000.f / rtt;
		cumPps_ = ((cumPps_ * 7.f) + pps) / 8.f;

        // Packet loss rate during this round-trip (OoO not accounted)
        float loss = static_cast<float>(netStatSent_ - cumAckCount_) / netStatSent_;
        loss = clamp(0.f, loss, 1.f);
        cumLoss_ = ((cumLoss_ * 7.f) + loss) / 8.f;

        // Delay transmittion if loss
        // TODO: this the job of the Congestion Control layer
        if (cumLoss_ > 0) {
            //auto delay_us = std::min(cum_rtt_us, cast_to_us(MAX_TX_DELAY).count());
            //txDelay_ = std::chrono::microseconds(delay_us);
            //RING_ERR("delay=%zuus", delay_us);
        }
    }
}

void
DataConnection::processPacket(std::vector<uint8_t>&& pkt)
{
    // Drop packet without enought data to decode it
    if (pkt.size() < PACKET_HEADER_BYTES) {
        ++rxBadPkt_;
        return;
    }

    // All packets indicate at least the packet sequence number and acknowlegment information
    auto pkt_seq = processTransmissionFields(pkt);
    if (pkt_seq == 0)
        return;
    RING_DBG("rx %016lx", pkt_seq);
    processAcknowledgmentFields(pkt);

    // process INIT payload immediatly
    auto flags = Packet::getChannelFlags(pkt);
    if (!protocol_) {
        if (isClient_) {
            // packet refused by server?
            if (flags.test(Packet::ChannelFlags::INIT)) {
                RING_ERR("[FTP:%zu] fatal error, protocol refused by server", id_);
                ++rxBadPkt_;
                stopConnection();
                return;
            }

            protocol_ = PROTOCOL_VERSION; // commit the end of negotiation
        } else { // server
            // drop non-init packet or without enough information
            auto size = Packet::getChannelPayloadSize(pkt);
            if (!flags.test(Packet::ChannelFlags::INIT) or !size) {
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
    } else if (!isClient_ and flags.test(Packet::ChannelFlags::INIT)) {
        // drop init packets after negotation
        RING_WARN("[FTP:%zu] drop too late INIT pkt", id_);
        return;
    }

    // do not ACK an heartbeat/ack-only packets
    bool ack_ack = pkt.size() == PACKET_HEADER_BYTES;

    // note: txAckCt_ and txAckSeq_ not mutex protected, as read in same context of write op.
    int32_t ack_seq_delta = pkt_seq - txAckSeq_;
    if (ack_seq_delta == 1) {
        // increment AckCt and record current ack state
        updateTxAckState(pkt_seq, std::min(txAckCt_ + 1, MAX_ACK_CT));
        // Force ACK packet at least each ACK_MAX_DELAY packets
        if (++delayedAck_ > ACK_MAX_DELAY) {
            delayedAck_ = 0;
            sendAck(txAckSeq_, txAckCt_);
        } else if (!ack_ack) {
            // Do not ACK acks (except if ACK_MAX_DELAY reached)
            // Let idle job to flush delayed ACK at next loop
            //TODO: start timer
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

    // transmit packet with FRAME data to upper layer (async)
    if (flags.test(Packet::ChannelFlags::FRAME)) {
        rxBytes_ += pkt.size();
        rxQueue_->push(std::move(pkt));
    }
}

void
DataConnection::attachStream(std::shared_ptr<DataStream> stream)
{
    rxQueue_->attachStream(stream);
    stream->setDataConnection(this);
}

void
DataConnection::detachStream(std::shared_ptr<DataStream> stream)
{
    rxQueue_->detachStream(stream);
    stream->setDataConnection(nullptr);
}

void
DataConnection::waitWrite()
{
    std::unique_lock<std::mutex> lk {txMtx_};
    ioCv_.wait(lk, [this]{ return txBuffer_ and txBuffer_->available() > 0; });
}

//==================================================================================================

DataStream::~DataStream()
{
    // unlock applications
    std::lock_guard<std::mutex> lk {mtx_};
    dc_ = nullptr;
    flags_.set(FlagsBit::ACK);
    cv_.notify_all();
}

void
DataStream::setDataConnection(DataConnection* dc)
{
    std::lock_guard<std::mutex> lk {mtx_};
    flags_.set(FlagsBit::ACK);
    cv_.notify_all();
    dc_ = dc;
}

ssize_t
DataStream::sendData(const uint8_t* buffer, std::size_t buffer_size)
{
    std::unique_lock<std::mutex> lk {mtx_};

    std::size_t bytes_send = 0;
    while (buffer_size) {
        if (!dc_) {
            errno = EINTR;
            return -1;
        }
        lk.unlock();
        dc_->waitWrite();

        auto src = buffer;
        auto bytes_to_send = std::min(static_cast<std::size_t>(dc_->maxPayload_ - 20), buffer_size);
        auto pkt = std::make_shared<TxPacket>();
        pkt->stream = this;
        Packet::packStreamFrame(pkt->bytes, 0, sid_, 0, bytes_to_send, src);
        RING_DBG("send packet (%zu)", bytes_to_send);
        flags_.reset(FlagsBit::ACK);
        if (!dc_->pushPacket(std::move(pkt))) {
            RING_DBG("txbuf full");
            continue; // TxBuffer is full
        }
        RING_DBG("packet sent");

        lk.lock();
        cv_.wait(lk, [this]{ return flags_[FlagsBit::ACK]; });

        src += bytes_to_send;
        buffer_size -= bytes_to_send;
        bytes_send += bytes_to_send; RING_DBG("bytes send %zu", bytes_send);
    }

    return bytes_send;
}

void
DataStream::onAck(DataConnection::SeqNum)
{
    std::lock_guard<std::mutex> lk {mtx_};
    flags_.set(FlagsBit::ACK); RING_DBG("packet ack");
    cv_.notify_one();
}

void
DataStream::onCloseStream()
{
    // Note: Called in the context of RxQueue's thread
}

void
DataStream::onRxPayload(const uint8_t*, std::size_t, uint64_t)
{
    // Note: Called in the context of RxQueue's thread
}

}} // namespace ring::ReliableSocket
