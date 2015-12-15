/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
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

// Project
#include "logger.h"
#include "intrin.h"
#include "threadloop.h"
#include "udt.h"
#include "noncopyable.h"

#include "ice_socket.h"

// Std
#include <string>
#include <map>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstdint>

namespace ring {

using SocketType = IceSocket;
using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

constexpr std::chrono::seconds operator ""_s(unsigned long long s)
{
    return std::chrono::seconds(s);
}

constexpr std::chrono::milliseconds operator ""_ms(unsigned long long s)
{
    return std::chrono::milliseconds(s);
}

// Common packet structure:
//
// This structure is based on SCTP packet (rfc4960)
// with modifications for our environment:
// -> packet are passed through a managed/secure socket (ICE+TLS)
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                        validation tag                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           chunk #1                            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                              ...                              |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           chunk #n                            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// Chunk structure:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    Chunk type   |    Chunk flags    |    Chunk length         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         Chunk data                            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//

class Packet : public std::vector<uint8_t> {
public:
    Packet(const uint8_t* ptr, size_t length) {
        // minimal lenght = 17 bytes
        if (length < 17)
            throw std::runtime_error("too short packet data");
        std::copy_n(ptr, length, data());
    }

    bool validPacket(uint32_t tag) {
        auto header = reinterpret_cast<uint32_t*>(data());
        return header[0] == (tag & 0x80000000);
    }
};

#if 0
class FileTransfer {
public:
    FileTransfer(std::ifstream&& stream) : stream_(std::move(stream)) {}

private:
    std::ifstream stream_;
};

static constexpr auto SYN = 100ms;

class PeerConnection {
    static constexpr auto N_EXP = 3; // EXP = N_EXP * NAK
    static constexpr auto MIN_EXP = 1s; // Minimal value of EXP timer

public:
    PeerConnection() : loop_([]{ return true; }, [this]{ process(); }, []{}) {
        resetExpCount();
        loop_.start();
    }

    ~RxQueue() {
        loop_.join(); // must be done before clearing socket map
    }

    void resetTimers() {
        std::lock_guard<std::mutex> lk{dataMutex_};
        const auto now = Clock::now();
        tACK_ = now + SYN; // TODO: comes from CCC
        const auto NAK = SYN; // TODO: RTT_ + 4 * RTTVar + SYN
        tNAK_ = now + NAK;
        tEXP_ = now + std::min(N_EXP * NAK, MIN_EXP); // TODO: N * NAK
        // FIXME: how to handle time wrapping?
    }

    void resetExpCount() {
        expCount_ = 1;
        lastExpCountReset_ = Clock::now();
    }

    bool waitOnData() {
        // Wait on closest time_point or any events
        std::unique_lock<std::mutex> lk{dataMutex_};
        return cv_.wait_until(lk, std::min({tACK_, tNAK_, tEXP_}), [](){ return dataReady_; });
    }

    void process() {
        if (waitOnData()) {
            handleNextPacket();
            expCount_ = 1;
        } else {
            // Process timers
            const auto now = Clock::now();
            if (now >= nextACK_)
                onACKTimeout();
            if (now >= nextNAK_)
                onNAKTimeout();
            if (now >= nextEXP_)
                onEXPTimeout();
        }
    }

    void onRxData(uint8_t*, size_t len) {
    }

    Packet nextPacket() {
        std::lock_guard<std::mutex> lk{dataMutex_};
        auto pkt = std::move(packetQueue_.front());
        packetQueue_.pop();
        return pkt;
    }

    void handleNextPacket() {
        resetExpCount();

        auto pkt = nextPacket();

        if (pkt.isData()}
            processDataPacket(pkt);
        else
            processControlPacket(pkt);
    }

    void processDataPacket() {
    }

    void processControlPacket() {
    }

    void onACKTimeout() {
        resetExpCount();
    }

    void onNAKTimeout() {
        resetExpCount();
    }

    void onEXPTimeout() {
        // Move unacknowledged packet into lost packet queue
        {
            std::lock_guard<std::mutex> lk{dataMutex_};
            auto item = std::begin(packetQueue_);
            const auto end = std::end(packQueue_)
                while (item != end) {
                    lostPktQueue_.push(std::move(*item));
                    ++item;
                }
            packetQueue_.clear();
        }

        // No packet from peer since long, is it dead?
        const auto now = Clock::now();
        if ((expCount_ > 16 and now > (lastExpCountReset_ + 5s)))
            deadPeer(peer);
    }

private:
    NON_COPYABLE(RxQueue);

    std::shared_ptr<SocketType> socket_;

    std::mutex dataMutex_;
    std::condition_variable cv_;
    bool dataReady_ {false};

    std::queue<UDTPacket> packetQueue_;
    std::priority_queue<UDTPacket> lostPktQueue_;

    unsigned RTT_ {10 * SYN};
    unsigned RTTVar {RTT_ / 2};

    TimePoint tACK_;
    TimePoint tNAK_;
    TimePoint tEXP_;

    TimePoint lastPktRxTime_;
    unsigned expCount_ {1};

    Threadloop loop_;
};

class PeerConnector {
public:
    PeerConnector(const std::string& peer, SocketType&& socket)
        : peer_(peer) {
        socket.setOnRecv([this](uint8_t*, size_t len) {
                RING_WARN("rx %zuB", len);
                return len;
            });
    }

private:
    const std::string peer_;
    RxQueue rxQ_;
};
#endif

class FileServer {
public:
    FileServer() {}
    ~FileServer() {}

    void setPeerConnection(const std::string& peer, SocketType&& socket) {
        std::lock_guard<std::mutex> lk {cnxMapMutex_};
        if (cnxMap_.find(peer) != cnxMap_.cend())
            cnxMap_.erase(peer);
        //cnxMap_.emplace(peer, PeerConnector(peer, std::move(socket)));
    }

    void disconnectPeer(const std::string& peer) {
        std::lock_guard<std::mutex> lk {cnxMapMutex_};
        if (cnxMap_.find(peer) != cnxMap_.cend())
            cnxMap_.erase(peer);
    }

    std::string newFileTransfer(const std::string& peer, std::ifstream&&) {
        return peer;
    }

private:
    NON_COPYABLE(FileServer);
    std::mutex cnxMapMutex_;
    std::map<std::string, int> cnxMap_;
};

} // namespace ring
