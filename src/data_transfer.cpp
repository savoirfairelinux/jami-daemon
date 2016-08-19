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
#include <iomanip>

namespace ring { namespace ReliableSocket {

//==================================================================================================
// Constants

static constexpr unsigned MAX_FRAME_HEADER_BYTES = 1+1+4+8+2; // Maximal size in bytes of a frame header
static constexpr unsigned MINIMAL_INIT_PAYLOAD_SIZE = 3 * 4; // Minimal number of data inside an INIT flagged packet
static constexpr unsigned TX_BUFFER_SIZE = 10; // Maximal number of packet waiting for transmission
static constexpr unsigned MAX_ACK_CT = 15; // Channel packet format uses 4 bits to encode AckCt
static constexpr unsigned MIN_UNACKED_PACKET = 2; // Minimal number of delayed ACK
static constexpr unsigned MAX_UNACKED_PACKET = 4; // Maximal number of ACK packet before sending an ACK
static constexpr auto ACK_DELAY = std::chrono::milliseconds(10); // ACK after this delay if unacked ACK exist
static constexpr auto HEARTBEAT_DELAY = std::chrono::seconds(60); // Heatbeat packets pacing
static constexpr auto MAX_RTT = std::chrono::seconds(10); // Limit RTT computation to this value
static constexpr auto MAX_TX_DELAY = std::chrono::milliseconds(100); // Maximum transmission pacing
static constexpr auto STATISTIC_DELAY = std::chrono::seconds(2); // Statistics logging pacing
static constexpr auto BASE_RTT = std::chrono::milliseconds(500); // RTT computation start

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
    return std::chrono::duration_cast<std::chrono::microseconds>(args...);
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

template <class T>
constexpr const T&
clamp(const T& low, const T& x, const T& high)
{
    return std::max(low, std::min(x, high));
}

template <class T>
unsigned long
usdelay(const T& start)
{
    return cast_to_us(T::clock::now() - start).count();
}

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
//		|F P M r r r r r|
//		+-+-+-+-+-+-+-+-+
//
//			bit F: end of stream, set on last bytes for this stream
//			bit P: set if data must be pushed to application as soon as possible
//			bit M: this block ends a message
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

enum FrameFlags {
    FRAMEF_STREAM = 0x80,
    FRAMEF_EXTRA  = 0x40,
    FRAMEF_DATA   = 0x20,
};

enum FrameExtraFlags {
    FRAMEXF_FIN = 0x80,
    FRAMEXF_PUSH = 0x40,
    FRAMEXF_EOM = 0x20,
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
        std::copy_n(reinterpret_cast<const uint8_t*>(payload), payload_size, pl_begin);
    return pl_begin;
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
        return ++frame; // first byte of payload
    }

    data_length = 0;
    return ++frame; // first byte of next frame
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
        off_size = clamp(2, static_cast<int>(std::ceil(std::log2(offset) / 8.)), 8);
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

template <typename T>
inline void
dump(const std::vector<T>& vec)
{
    auto p = reinterpret_cast<const uint32_t*>(vec.data());
    for (std::size_t i=0; i < PACKET_HEADER_WORDS; ++i, ++p)
        RING_DBG("[%zu] %08x", i, ntohl(*p));
    auto pk_size = vec.size() * sizeof(T);
    if (pk_size > PACKET_HEADER_BYTES)
        RING_DBG("Payload of %zu byte(s)", pk_size - PACKET_HEADER_BYTES);
}

} // namespace ring::ReliableSocket::Packet

//==================================================================================================

struct TxPacket {
    using Clock = std::chrono::high_resolution_clock;
    std::vector<uint8_t> bytes {};
    DataConnection::SeqNum full_seq {0};
    DataStream* stream {nullptr};
    bool ack {false}; // true if receiver should acknowledge this packet
    Clock::time_point time_tx;
    Clock::time_point time_ack;
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
    bool empty() const;

private:
    unsigned maxPacket_;
    std::queue<std::shared_ptr<TxPacket>> outQ_ {}; // packets to transmit
};

bool
TxFrameBuffer::push(std::shared_ptr<TxPacket> pkt)
{
    if (outQ_.size() == maxPacket_)
        return false; // full
    outQ_.emplace(pkt);
    return true;
}

std::shared_ptr<TxPacket>
TxFrameBuffer::pop()
{
    if (outQ_.empty()) {
        //RING_WARN("be");
        return {};
    }
    auto pkt_ptr = outQ_.front();
    outQ_.pop();
    return pkt_ptr;
}

void
TxFrameBuffer::flush()
{
    while (!outQ_.empty())
        outQ_.pop();
}

std::size_t
TxFrameBuffer::available()
{
    return maxPacket_ - outQ_.size();
}

bool
TxFrameBuffer::empty() const
{
    return outQ_.empty();
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
    std::unique_lock<std::mutex> lk {jobMutex_};

    while (running_) {
        // wait for scheduling
        if (!scheduled_)
            jobCv_.wait(lk, [this]{ return !running_ or scheduled_; });

        // wait until schedule time is over or re-scheduling
        do {
            auto when = popPacketTime_;
            if (!jobCv_.wait_until(lk, when,
                                   [this, when] { return !running_ or popPacketTime_ < when; }))
                break;
        } while (running_);

        if (!running_)
            return;

        TimePoint nextTime;
        auto pkt = dc_.nextTxPacket(nextTime);

        if (pkt) {
            lk.unlock();
            dc_.transmit(pkt);
            lk.lock();
            popPacketTime_ = nextTime;
            scheduled_ = true;
        } else {
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

void
CongestionControl::reset()
{
    cumRtt_ = BASE_RTT;
    cumRttVar_ = std::chrono::microseconds(0);
    cumPps_ = 0;
    txBytes_ = rxBytes_ = rxLossPkt_ = rxBadPkt_ = rxOldPkt_ = rxDupPkt_ = 0;
}

//==================================================================================================

class DataConnectionFactory {
public:
    std::shared_ptr<DataConnection> make(const std::string& account_id,
                                         const std::string& peer_id,
                                         bool is_client);

    std::shared_ptr<DataConnection> find(const DRing::DataConnectionId& cid) const;

private:
    mutable std::mutex mtx_ {};
    mutable std::map<DRing::DataConnectionId, std::weak_ptr<DataConnection>> map_;
};

std::shared_ptr<DataConnection>
DataConnectionFactory::make(const std::string& account_id,
                            const std::string& peer_id,
                            bool is_client)
{
    std::lock_guard<std::mutex> lk {mtx_};
    auto cnx = std::make_shared<DataConnection>(account_id, peer_id, is_client);
    map_.emplace(cnx->getId(), cnx);
    return cnx;
}

std::shared_ptr<DataConnection>
DataConnectionFactory::find(const DRing::DataConnectionId& cid) const
{
    std::lock_guard<std::mutex> lk {mtx_};
    auto pair = map_.find(cid);
    if (pair == map_.cend())
        return {};
    if (auto dc = pair->second.lock())
        return dc;
    map_.erase(pair);
    return {};
}

// Meyer Singleton
static DataConnectionFactory&
getDataConnectionFactory()
{
    static DataConnectionFactory ft;
    return ft;
}

//==================================================================================================

std::shared_ptr<DataConnection>
makeDataConnection(const std::string& account_id,
                   const std::string& peer_id,
                   bool is_client)
{
    auto& ft = getDataConnectionFactory();
    return ft.make(account_id, peer_id, is_client);
}

DataConnection::DataConnection(const std::string& account_id, const std::string& peer_id,
                               bool is_client)
    : id_(genUUID())
    , isClient_(is_client)
{
    info_.account = account_id;
    info_.peer = peer_id;
    info_.isClient = is_client;
    info_.code = DRing::DataConnectionCode::CODE_UNKNOWN;
}

DataConnection::~DataConnection()
{
    disconnect();
}

DRing::DataConnectionId
DataConnection::genUUID()
{
    static DRing::DataConnectionId cid {0};
    return ++cid;
}

void
DataConnection::getInfo(DRing::DataConnectionInfo& info) const
{
    std::lock_guard<std::mutex> lk {infoMutex_};
    info = info_;
}

void
DataConnection::stopConnection()
{
    std::lock_guard<std::mutex> lk {txMtx_};
    txSeq_ = 0;
    if (tls_)
        tls_->shutdown();
}

void
DataConnection::connect(tls::TlsSession* tls)
{
    std::unique_lock<std::mutex> lk {txMtx_};

    tls_ = tls;
    maxPayload_ = tls_->getMaxPayload();
    RING_DBG("[FTP:%lx] maxPayload=%u", id_, maxPayload_);

    cc_.reset();

    txQueue_.reset(new TxScheduler<Clock>(*this));
    txBuffer_.reset(new TxFrameBuffer(TX_BUFFER_SIZE));
    rxQueue_.reset(new RxQueue(*this));

    lk.unlock();

#if 0
    if (isClient_) {
        testLoop_ = std::unique_ptr<ThreadLoop>(new ThreadLoop(
                                                    []{ return true; },
                                                    [this]{ return testAppSendData(); },
                                                    []{}));
        testLoop_->start();
    } else {
        testLoop_ = std::unique_ptr<ThreadLoop>(new ThreadLoop(
                                                    []{ return true; },
                                                    [this]{ return testAppRecvData(); },
                                                    []{}));
        testLoop_->start();
    }
#endif

    auto shared = sharedPtr();
    Manager::instance().scheduleTask([shared]{ shared->logStatistics(); }, STATISTIC_DELAY);

    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        if (info_.code < 200)
            info_.code = DRing::DataConnectionCode::CODE_OK;
    }
}

void
DataConnection::disconnect()
{
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        if (info_.code < 200)
            info_.code = DRing::DataConnectionCode::CODE_DISCONNECTED;
    }

    // stop asap test application (may own blocked streams)
    if (testLoop_)
        testLoop_->stop();

    {
        std::lock_guard<std::mutex> lk {txMtx_};
        txBuffer_.reset();
        rxQueue_.reset();
    }

    // release stream's references in ack waiting packet
    {
        std::lock_guard<std::mutex> lk {ackWaitMapMutex_};
        ackWaitMap_.clear();
    }

    ioCv_.notify_all();
    if (testLoop_)
        testLoop_->join();

    // stop Tx/Rx layer
    {
        std::lock_guard<std::mutex> lk {txMtx_};
        txQueue_.reset();
    }

    tls_ = nullptr;
}

bool
DataConnection::disconnected() const
{
    std::lock_guard<std::mutex> lk {txMtx_};
    return !txBuffer_ or !rxQueue_;
}

IpAddr
DataConnection::getRemoteAddress() const
{
    std::unique_lock<std::mutex> lk {txMtx_};
    if (tls_)
        return tls_->getRemoteAddress();
    return {};
}

CongestionControl
DataConnection::getCC() const
{
    std::lock_guard<std::mutex> lk {txMtx_};
    return cc_;
}

std::chrono::microseconds
DataConnection::getCumRtt() const
{
    std::lock_guard<std::mutex> lk {txMtx_};
    return cc_.cumRtt_;
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
        // Old missed packets are re-sent in priority over new packets.
        auto rtt = getCumRtt();
        {
            std::unique_lock<std::mutex> lk1 {txMtx_, std::defer_lock};
            std::unique_lock<std::mutex> lk2 {ackWaitMapMutex_, std::defer_lock};
            std::lock(lk1, lk2);
            if (!ackWaitMap_.empty()) {
                auto pair = ackWaitMap_.cbegin();
                if (txBuffer_->empty()) {
                    pkt = std::move(pair->second);
                    ackWaitMap_.erase(pair);
                    txDelay_ = (rtt * 7 + txDelay_) / 8;
                    goto end;
                }

                auto delay = ((Clock::now() - pair->second->time_tx) - rtt).count();
                if (delay > 0) {
                    pkt = std::move(pair->second);
                    ackWaitMap_.erase(pair);
                    txDelay_ = (rtt / 2 + txTargetSpeed_ * 15) / 16;
                    goto end;
                }
            }

            if (!pkt and txBuffer_) {
                pkt = txBuffer_->pop();
                txDelay_ = (txDelay_ * 15 + txTargetSpeed_) / 16;
            }
        }
        if (pkt)
            ioCv_.notify_one(); // see waitWrite()
    }

    if (!pkt) {
        //RING_WARN("no more pkt");
        return {};
    }

end:
    // Delay next call
    when = packet_ts + txDelay_;
    //RING_DBG("next in %zuus", cast_to_us(txDelay_).count());
    return pkt;
}

bool
DataConnection::pushPacket(std::shared_ptr<TxPacket> pkt)
{
    std::unique_lock<std::mutex> lk {txMtx_};
    if (txBuffer_ and txBuffer_->push(pkt)) {
        lk.unlock();
        txQueue_->update();
        return true;
    }
    return false;
}

ssize_t
DataConnection::sendPkt(std::vector<uint8_t>& pkt, SeqNum& pkt_seq,
                        SeqNum ack_seq, unsigned ack_count, Clock::time_point& tp)
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

    std::lock_guard<std::mutex> tx_lk {txMtx_};

    // disconnected?
    if (!tls_)
        return -1;

    if (!txSeq_) {
        RING_ERR("[FTP:%zu] sequence number overflow", id_);
        stopConnection();
        return -1;
    }

    auto now = Clock::now();
    tp = now;
    pkt_seq = txSeq_++;

    // Reset network statistics when marked sequence reached
    if (pkt_seq == netStatSeq_) {
        netStatTime_ = now;
        netAckCount_ = 0;
        netStatSent_ = pkt_seq - rxAckSeq_; // number of packets sent since last acknowledgment
    }

    Packet::setTsn(pkt, pkt_seq);
    Packet::setAsnAckCnt(pkt, ack_seq, ack_count);

    //RING_DBG("tx: seq=%lx, ack=%lx-%u", pkt_seq, ack_seq, ack_count);
    //Packet::dump(pkt);
    auto res = tls_->send(pkt);

    if (res > 0)
        cc_.txBytes_ += pkt.size();

    return res;
}

void
DataConnection::transmit(std::shared_ptr<TxPacket> pkt)
{
    std::unique_lock<std::mutex> lk {ackWaitMapMutex_};
    //RING_ERR("-transmit: %zu", usdelay(tp_[0]));
    //auto start = Clock::now();
    if (sendPkt(pkt->bytes, pkt->full_seq, pkt->time_tx) > 0) {
        if (delayedAck_) {
            delayedAck_ = 0;
            doAck_ = false;
        }
        if (pkt->ack)
            ackWaitMap_.emplace(pkt->full_seq, pkt);
    }
    //RING_ERR("+transmit: %zu", usdelay(start));
    //tp_[0] = Clock::now();
}

ssize_t
DataConnection::sendPkt(std::vector<uint8_t>& pkt, SeqNum& pkt_seq, Clock::time_point& tp)
{
    SeqNum ack_seq;
    unsigned ack_count;
    getTxAckState(ack_seq, ack_count);
    return sendPkt(pkt, pkt_seq, ack_seq, ack_count, tp);
}

ssize_t
DataConnection::sendPkt(std::vector<uint8_t>& pkt)
{
    SeqNum pkt_seq; // needed but not used
    SeqNum ack_seq;
    unsigned ack_count;
    Clock::time_point tp;
    getTxAckState(ack_seq, ack_count);
    return sendPkt(pkt, pkt_seq, ack_seq, ack_count, tp);
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
    Clock::time_point tp;
    sendPkt(pkt, pkt_seq, ack_seq, ack_count, tp);
}

void
DataConnection::processIncomingPacket(std::vector<uint8_t>&& pkt)
{
    auto now = Clock::now();

    // Drop packet without enought data to decode it
    if (pkt.size() < PACKET_HEADER_BYTES) {
        std::lock_guard<std::mutex> lk {txMtx_};
        ++cc_.rxBadPkt_;
        return;
    }

    // Obtain information about current peer state and update our own state with
    auto pkt_seq = processTransmissionFields(pkt);
    if (pkt_seq == 0)
        return;
    //RING_DBG("rx: seq=%lx", pkt_seq);
    processAcknowledgmentFields(pkt, now);

    // Handle special packet immediatly (like INIT packet during protocol negotiation)
    // This method returns false if the whole packet must be drop without acknowledgement
    bool has_data;
    if (!processChannelFlags(pkt_seq, pkt, has_data))
        return;

    {
        std::lock_guard<std::mutex> lk {txMtx_};
        cc_.rxBytes_ += pkt.size();
    }

    // Check the need for sending an acknowledgment packet
    bool ack_ack = pkt.size() == PACKET_HEADER_BYTES; // do not ACK an heartbeat/ack-only packets
    checkForAcknowledgment(pkt_seq, ack_ack);

    // Finish by any transmit application data to upper layer
    if (has_data) {
        if (rxQueue_) {
            std::lock_guard<std::mutex> lk {txMtx_};
            rxQueue_->push(std::move(pkt));
        }
    }
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
            std::lock_guard<std::mutex> lk {txMtx_};
            ++cc_.rxOldPkt_;
            return 0;
        }

        // Duplicate?
        if (rxMask_[-seq_delta]) {
            RING_WARN("[FTP:%zu] drop dup pkt %016lx", id_, pkt_seq);
            std::lock_guard<std::mutex> lk {txMtx_};
            ++cc_.rxDupPkt_;
            return 0;
        }

        // Accepted out-of-order packet
        rxMask_.set(-seq_delta);
    } else { // seq_delta > 0
        // Make sure we don't wraparound, all the design is based on this assumption.
        // This should never append as our tx engine protect against that.
        // But... in case of a foolish peer.
        if (pkt_seq < rxSeq_) {
            RING_ERR("[FTP:%zu] fatal error, rx sequence number has wraparound!", id_);
            {
                std::lock_guard<std::mutex> lk {txMtx_};
                ++cc_.rxBadPkt_;
            }
            stopConnection();
            return 0;
        }

        // Increment bigger than 1 means possible loss or OoO.
        // At this stage, just consider them as lost
        if (seq_delta > 1) {
            std::lock_guard<std::mutex> lk {txMtx_};
            cc_.rxLossPkt_ += seq_delta - 1;
        }

        // This is a new packet: update our internal rx state
        rxSeq_ = pkt_seq;
        rxMask_ <<= seq_delta;
        rxMask_.set(0);
    }

    return pkt_seq;
}

void
DataConnection::processAcknowledgmentFields(const std::vector<uint8_t>& pkt, Clock::time_point tp)
{
    // As for TSN, infer the global acknowledgment sequence number of this packet
    uint32_t pkt_asn;
    uint8_t pkt_ackct;
    Packet::getAsnAckCt(pkt, pkt_asn, pkt_ackct);
    int32_t ack_delta = ((static_cast<int32_t>(pkt_asn) << 4) - (static_cast<int32_t>(rxAckSeq_) << 4)) >> 4;
    SeqNum ack_seq = rxAckSeq_ + ack_delta;

    //RING_DBG("rx ack: %lx-%u, d=%d, m=%016llx", ack_seq, pkt_ackct, ack_delta, rxAckMask_.to_ullong());
    ++pkt_ackct; // 0 means 1 packet acknowledged

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
        ack_count += std::min(ack_delta, static_cast<int32_t>(pkt_ackct));
        rxAckMask_ |= (1 << ack_count) - 1; // set all bits from ack_count-1 downto 0

        // Acknowledge all contiguous packets
        onAckSeq(ack_seq, tp, ack_count);

        ack_delta = 0; // reset the delta before OoO processing just below
    }

    // Handle acknowledgement data in a OoO packet.
    // note: an OoO packet acknowledges an in-order packet.
    unsigned long ack_mask = (1ul << (-ack_delta + pkt_ackct)) - 1;
    if ((rxAckMask_.to_ullong() & ack_mask) != ack_mask) {
        for (unsigned i=0; i < pkt_ackct; ++i) {
            unsigned bit = -ack_delta + i;
            if (bit >= rxAckMask_.size())
                break;
            if (!rxAckMask_[bit]) {
                rxAckMask_.set(bit);
                ++ack_count;

                // Acknowledge one packet
                onAckSeq(ack_seq, tp);
            }
        }
    }

    std::unique_lock<std::mutex> tx_lk {txMtx_};
    netAckCount_ += ack_count;

    // Network statistics is trigged when the marked sequence as made (or supposed to) a round-trip
    if (ack_seq >= netStatSeq_) {
        netStatSeq_ = txSeq_; // next trig point

        // RTT (of the group of acked sequences), clamped to [1, MAX_RTT]
        auto rtt = cast_to_us(tp - netStatTime_);
        rtt = clamp(std::chrono::microseconds(1), rtt, cast_to_us(MAX_RTT));
        cc_.cumRtt_ = ((cc_.cumRtt_ * 7) + rtt) / 8;

        // RTT variance measure
		auto rtt_var = rtt >= cc_.cumRtt_ ? rtt - cc_.cumRtt_ : cc_.cumRtt_ - rtt;
		cc_.cumRttVar_ = ((cc_.cumRttVar_ * 7) + rtt_var) / 8;

        // Number of unique ACK during this round-trip
        auto pps = netAckCount_ * 1000000ul / rtt.count();
		cc_.cumPps_ = ((cc_.cumPps_ * 7) + pps) / 8;

        // Packet loss rate during this round-trip (OoO not accounted)
        //float loss = static_cast<float>(netStatSent_ - cumAckCount_) / netStatSent_;
        //loss = clamp(0.f, loss, 1.f);
        //cc_.cumLoss_ = ((cc_.cumLoss_ * 7.f) + loss) / 8.f;
    }

    tx_lk.unlock();

    constexpr unsigned long long ooomask = (1ull << MISS_ORDERING_LIMIT) - 1;
    if (!rxAckMask_.all() and (rxAckMask_.to_ullong() & ooomask) == ooomask)
        txQueue_->update();
}

bool
DataConnection::processChannelFlags(SeqNum pkt_seq, const std::vector<uint8_t>& pkt, bool& has_data)
{
    auto flags = Packet::getChannelFlags(pkt);
    has_data = flags.test(Packet::ChannelFlags::FRAME);

    // protocol not negotiated?
    if (!protocol_) {
        if (isClient_) {
            // client protocol refused by server?
            if (flags.test(Packet::ChannelFlags::INIT)) {
                RING_ERR("[FTP:%zu] fatal error, protocol refused by server", id_);
                stopConnection(); // current implementation stops the connection
                return false;
            }

            // commit the end of negotiation
            protocol_ = PROTOCOL_VERSION;
        } else { // server side
            // during protocol negotiation, drop non-INIT packet or without enough information
            auto size = Packet::getChannelPayloadSize(pkt);
            if (!flags.test(Packet::ChannelFlags::INIT) or size < MINIMAL_INIT_PAYLOAD_SIZE) {
                std::lock_guard<std::mutex> lk {txMtx_};
                ++cc_.rxBadPkt_;
                return false;
            }

            // check for any supported client protocol
            auto payload = Packet::getChannelPayload<uint32_t>(pkt);
            unsigned count = Packet::getChannelPayloadSize<uint32_t>(pkt);
            uint32_t peer_proto = 0;
            for (unsigned i=2; i < count; ++i) {
                peer_proto = ntohl(payload[i]);
                if (peer_proto == PROTOCOL_VERSION) {
                    // save peer ConnectionId
                    peerConnectionId_ = ntohl(payload[0]);
                    peerConnectionId_ <<= 32;
                    peerConnectionId_ += ntohl(payload[1]);

                    RING_DBG("[FTP:%zu] client protocol %08x accepted, client ID: %016lx",
                             id_, peer_proto, peerConnectionId_);
                    protocol_ = PROTOCOL_VERSION; // commit the end of negotiation
                    return true;
                }

                RING_WARN("[FTP:%zu] client protocol %08x refused", id_, peer_proto);
            }

            // nothing supported.
            // send an INIT packet with our protocol version and drop this one
            sendProto();
            return false;
        }
    } else if (!isClient_ and flags.test(Packet::ChannelFlags::INIT)) {
        // server drops all INIT packets after negotation, but ACK them
        sendAck(pkt_seq, 0);
        return false;
    }

    // An INIT flagged packet shall not have application data and a data packet must have data
    if (has_data and (flags.test(Packet::ChannelFlags::INIT) or
                      pkt.size() == PACKET_HEADER_BYTES)) {
        std::lock_guard<std::mutex> lk {txMtx_};
        ++cc_.rxBadPkt_;
        return false;
    }

    return true;
}

void
DataConnection::checkForAcknowledgment(SeqNum pkt_seq, bool ack_ack)
{
    // note: txAckCt_ and txAckSeq_ not mutex protected, as read in same context of write op.
    int32_t ack_seq_delta = pkt_seq - txAckSeq_;
    if (ack_seq_delta == 1) {
        // increment AckCt and record current ack state
        updateTxAckState(pkt_seq, std::min(txAckCt_ + 1, MAX_ACK_CT));
        ++delayedAck_;

        // Do not ACK acks before a minimal unsent ack count
        if (ack_ack /*and delayedAck_ < MAX_UNACKED_PACKET*/)
            return;

        if (delayedAck_ < MAX_UNACKED_PACKET) {
            // We let the idle loop send the ACK until a threshold
            // This let the Tx layer a chance to combine ACK with useful data
            // After we flush immediatly
            if (delayedAck_ < MIN_UNACKED_PACKET) {
                ackTime_ = Clock::now() + ACK_DELAY;
                doAck_ = true;
                return;
            }
        }

        flushDelayedAck();
    } else if (ack_seq_delta > 1) {
        // non-contiguous sequences

        // flush delayed ACK
        if (delayedAck_ > 0)
            flushDelayedAck();

        // reset ack state
        updateTxAckState(pkt_seq, 0);

        // acknowledge the current state immediately (if not a ACK packet)
        if (!ack_ack)
            sendAck(pkt_seq, 0);
    } else { // ack_seq_delta < 0 (OoO packet)
        // flush delayed ACK
        if (delayedAck_ > 0)
            flushDelayedAck();

        // acknowledge this packet immediately
        if (!ack_ack)
            sendAck(pkt_seq, 0);
    }
}

void
DataConnection::flushDelayedAck()
{
    delayedAck_ = 0;
    doAck_ = false;
    sendAck(txAckSeq_, txAckCt_);
}

void
DataConnection::onAckSeq(SeqNum base_seq, Clock::time_point tp, unsigned count)
{
    std::list<std::shared_ptr<TxPacket>> acked;

    {
        std::lock_guard<std::mutex> lk {ackWaitMapMutex_};
        do {
            auto seq = base_seq - (--count);
            //RING_WARN("ack %lx", seq);
            auto pair = ackWaitMap_.find(seq);
            if (pair == ackWaitMap_.cend())
                continue;
            //RING_WARN("ack %lx", seq);
            auto pkt = pair->second;
            if (pkt->stream) {
                pkt->time_ack = tp;
                acked.emplace_back(pkt); // defer stream ack out of locked region
            }
            ackWaitMap_.erase(seq);
        } while (count);
    }

    for (auto pkt : acked)
        pkt->stream->onAck(pkt);
}

void
DataConnection::onExpiredSeq(SeqNum last_expired, Clock::time_point tp)
{
    std::list<std::shared_ptr<TxPacket>> expired;

    {
        std::lock_guard<std::mutex> lk {ackWaitMapMutex_};
        for (auto& pair : ackWaitMap_) {
            if (pair.first > last_expired)
                break;
            auto pkt = pair.second;
            if (pkt->stream) {
                pkt->time_ack = tp;
                expired.emplace_back(pkt);
            }
            ackWaitMap_.erase(pair.first);
        }
    }

    for (auto pkt : expired)
        pkt->stream->onExpire(pkt);
}

void
DataConnection::attachStream(std::shared_ptr<DataStream> stream)
{
    std::lock_guard<std::mutex> lk {txMtx_};
    rxQueue_->attachStream(stream);
    stream->setDataConnection(this);
}

void
DataConnection::detachStream(std::shared_ptr<DataStream> stream)
{
    std::lock_guard<std::mutex> lk {txMtx_};
    rxQueue_->detachStream(stream);
    stream->setDataConnection(nullptr);
}

void
DataConnection::waitWrite()
{
    std::unique_lock<std::mutex> lk {txMtx_};
    ioCv_.wait(lk, [this]{ return !txBuffer_ or txBuffer_->available() > 0; });
}

//----- Statistics ---------------------------------------------------------------------------------

// Return a SI prefixed value
static std::string
getPrefix(std::size_t n) noexcept
{
    std::ostringstream os;
    os << std::setprecision(3);

    if (n < 1000ul)
        return to_string(n);
    if (n < 1000000ul) {
        os << (n / 1e3) << 'K';
        return os.str();
    }
    if (n < 1000000000ul) {
        os << (n / 1e6) << 'M';
        return os.str();
    }
    if (n < 1000000000000ul) {
        os << (n / 1e9) << 'G';
        return os.str();
    }
    if (n < 1000000000000ul) {
        os << (n / 1e12) << 'T';
        return os.str();
    }
    os << (n / 1e16) << 'P';
    return os.str();
}

void
DataConnection::logStatistics()
{
#if 0
    if (protocol_) {
        // Send Heartbeat
        // TODO: put it into TxScheduler
        auto now = Clock::now();
        if (now - lastHeartbeat_ >= HEARTBEAT_DELAY) {
            lastHeartbeat_ = now;
            sendHeartbeat();
        } else if (doAck_ and (now - ackTime_) >= ACK_DELAY) {
            doAck_ = false;
            if (delayedAck_ > 0)
                flushDelayedAck();
        }
    }
#endif

    auto cc = getCC();
    auto now = Clock::now();
    auto usdelay = cast_to_us(now - lastStats_).count();
    lastStats_ = now;

    RING_WARN("\n[TX] Bytes: %sB, RTT (ms): %lu, RTT var.: %lu, PPS: %u, Bw: %sb/s"
              "\n[RX] Bytes: %sB, Bad: %zu, Loss/OoO: %zu, Dup: %zu"
              , getPrefix(cc.txBytes_).c_str(), cc.cumRtt_.count() / 1000
              , cc.cumRttVar_.count() / 1000, cc.cumPps_
              , getPrefix((cc.txBytes_ - prevTxBytes_) * 8000000 / usdelay).c_str()
              , getPrefix(cc.rxBytes_).c_str(), cc.rxBadPkt_, cc.rxLossPkt_, cc.rxDupPkt_);

    pktSnd_ = pktLate_ = pktForced_ = 0;
    prevTxBytes_ = cc.txBytes_;

    if (!disconnected()) {
        auto shared = sharedPtr();
        Manager::instance().scheduleTask([shared]{ shared->logStatistics(); }, STATISTIC_DELAY);
    }
}

//----- Tests --------------------------------------------------------------------------------------

void
DataConnection::testChannelFlood()
{
    // This test floods the channel #1 in transmission
    // Used to see network capacity and CPU usage
    auto pkt = std::make_shared<TxPacket>();
    // Create the biggest frame packet (zero initialized)
    uint8_t sid_size, off_size;
    uint16_t data_length = maxPayload_ - Packet::getStreamFrameHeaderSize(sid_size, off_size, 0, 1, 0, 0xffff);
    Packet::packStreamFrame(pkt->bytes, 0, 1, 0, data_length - PACKET_HEADER_BYTES);
    if (!pushPacket(pkt))
        waitWrite();
}

void
DataConnection::testAppSendData()
{
    RING_WARN("testAppSendDataJob started");
    std::array<uint8_t, 1024 * 400> buf;
    auto stream = std::make_shared<DataStream>(1); // SID = 1
    attachStream(stream);
    std::size_t cumBytes = 0;
    Clock::duration cumTime;

    while (testLoop_->isRunning()) {
        auto t1 = Clock::now();
        if (stream->sendData(buf.data(), buf.size()) < 0) {
            RING_ERR("sendData() error, errno=%s", strerror(errno));
            break;
        }
        cumTime += Clock::now() - t1;
        cumBytes += buf.size();
        auto delay = cast_to_s(cumTime).count();
        if (delay >= 5) {
            RING_ERR("%.3fMb/s", 8. * cumBytes / cast_to_us(cumTime).count());
            cumBytes = 0;
            cumTime = Clock::duration::zero();
        }
    }
    RING_WARN("test finished");
    testLoop_->stop();
}

void
DataConnection::testAppRecvData()
{
    RING_WARN("testAppRecvData started");
    std::array<uint8_t, 1024 * 8> buf;
    auto stream = std::make_shared<DataStream>(1); // SID = 1
    attachStream(stream);
    std::size_t cumBytes = 0;
    Clock::duration cumTime;

    while (testLoop_->isRunning()) {
        auto t1 = Clock::now();
        auto res = stream->recvData(buf.data(), buf.size());
        if (res < 0) {
            RING_ERR("sendData() error, errno=%s", strerror(errno));
            break;
        }
        cumTime += Clock::now() - t1;
        cumBytes += res;
        auto delay = cast_to_s(cumTime).count();
        if (delay >= 5) {
            RING_ERR("%.3fMb/s", 8. * cumBytes / cast_to_us(cumTime).count());
            cumBytes = 0;
            cumTime = Clock::duration::zero();
        }
    }
    RING_WARN("test finished");
    testLoop_->stop();
}

//==================================================================================================

DataStream::~DataStream()
{
    // unlock applications
    std::lock_guard<std::mutex> lk {mtx_};
    dc_ = nullptr;
    flags_.set(FlagsBit::ACK);
    flags_.set(FlagsBit::BYTES_AVAILABLE);
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
DataStream::sendData(const void* buffer, std::size_t buffer_size)
{
    std::unique_lock<std::mutex> lk {mtx_};
    if (!dc_) {
        errno = ENOTCONN;
        return -1;
    }

    std::size_t max_size = dc_->maxPayload_ - MAX_FRAME_HEADER_BYTES;

    // Segment application buffer into transmit packets
    RING_DBG("[MST] tx %zu\n>>>>>>>>%s<<<<<<<<\n", buffer_size, std::string((char*)buffer, buffer_size).c_str());
    std::size_t bytes_send = 0;
    auto src = static_cast<const uint8_t*>(buffer);
    while (buffer_size) {
        if (!dc_) {
            errno = EINTR;
            return -1;
        }

        ackwait_.emplace_back(std::make_shared<TxPacket>());
        auto pkt = ackwait_.back();
        pkt->stream = this;
        pkt->ack = true;

        auto bytes_to_send = std::min(max_size, buffer_size);
        uint8_t extra_flags = 0;
        if (bytes_to_send == buffer_size)
            extra_flags |= Packet::FrameExtraFlags::FRAMEXF_PUSH;
        Packet::packStreamFrame(pkt->bytes, extra_flags, sid_, offset_, bytes_to_send, src);

        // Try to wait for empty tx slot and push the packet
        do {
            lk.unlock();
            dc_->waitWrite();
            if (dc_->disconnected()) {
                errno = ENOTCONN;
                return -1;
            }
            lk.lock();
        } while (!dc_->pushPacket(pkt));

        src += bytes_to_send;
        buffer_size -= bytes_to_send;
        offset_ += bytes_to_send;
        bytes_send += bytes_to_send;
    }

    // Wait ack's for all pushed packet
    //RING_DBG("wait acks (%zu)+", ackwait_.size());
    cv_.wait(lk, [this]{ return ackwait_.empty(); });
    //RING_DBG("wait acks-");

    return bytes_send;
}

ssize_t
DataStream::recvData(void* buffer, std::size_t buffer_size)
{
    std::unique_lock<std::mutex> lk {mtx_};

    // Wait for timeout or received contiguous blocks
    if (!cv_.wait_for(lk, std::chrono::seconds(60), [this] {
                return !dc_ or flags_[FlagsBit::BYTES_AVAILABLE];
            })) {
        errno = EINTR;
        return -1;
    }

    if (!dc_) {
        errno = ENOTCONN;
        return -1;
    }

    if (buffer_size == 0)
        return 0;

    auto dst = static_cast<uint8_t*>(buffer);

    // Loop on offset-ordered received segments until a discontinuity
    auto it = reorderBuffer_.cbegin();
    auto next_off = it->first;
    while (it != reorderBuffer_.cend() and it->first <= next_off) {
        auto seg_off = it->first;
        auto& segment = it->second;

        // Stop on output buffer full
        std::size_t bytes_read = dst - static_cast<uint8_t*>(buffer);
        if (bytes_read >= buffer_size) {
            RING_WARN("stop, app buffer full");
            break;
        }

        auto buf_max = buffer_size - bytes_read;
        auto bytes_to_copy = std::min(segment.size(), buf_max);
        RING_DBG("buf(%zu) <<< %zu B <<< segment[%lu]", buf_max, segment.size(), seg_off);
        dst = std::copy_n(segment.data(), bytes_to_copy, dst);
        next_off = seg_off + bytes_to_copy;

        // Re-insert non-consumed data
        if (bytes_to_copy < segment.size())
            reorderBuffer_.emplace(next_off, std::vector<uint8_t>(segment.cbegin() + bytes_to_copy,
                                                                  segment.cend()));

        it = reorderBuffer_.erase(it);
    }

    RING_DBG("next_off=%lu, gapOffset_=%lu", next_off, gapOffset_);
    if (next_off > gapOffset_)
        gapOffset_ = next_off;

    auto bytes_read = dst - static_cast<uint8_t*>(buffer);
    readOffset_ += bytes_read;

    if (reorderBuffer_.empty())
        flags_.reset(FlagsBit::BYTES_AVAILABLE);
    return bytes_read;
}

bool
DataStream::canRead() const
{
    std::lock_guard<std::mutex> lk {mtx_};
    return dc_ and flags_[FlagsBit::BYTES_AVAILABLE];
}

void
DataStream::onAck(std::shared_ptr<TxPacket> pkt)
{
    std::lock_guard<std::mutex> lk {mtx_};
    ackwait_.remove(pkt);
    //RING_WARN("ack %lx (%zu)", pkt->full_seq, ackwait_.size());
    if (ackwait_.empty())
        cv_.notify_one();
}

void
DataStream::onExpire(std::shared_ptr<TxPacket>)
{}

void
DataStream::onCloseStream()
{
    // Note: Called in the context of RxQueue's thread
}

void
DataStream::onRxPayload(const uint8_t* buffer, std::size_t length, uint64_t offset)
{
    std::lock_guard<std::mutex> lk {mtx_};

    // Drop already pushed bytes
    if (offset < readOffset_)
        return;

    // Insert data by copy only if vector has been inserted
    auto& segment = reorderBuffer_[offset];
    if (segment.empty()) {
        segment.reserve(length);
        segment.resize(length);
        std::copy_n(buffer, length, segment.data());
        RING_WARN("reorderBuffer(%zu, %lu) <<< %zu B @ %lu", reorderBuffer_.size()-1, gapOffset_
                  , segment.size(), offset);
    }

    // Wake-up application when continuity change
    if (offset == gapOffset_) {
        flags_.set(FlagsBit::BYTES_AVAILABLE);
        cv_.notify_one();
    }
}

}} // namespace ring::ReliableSocket
