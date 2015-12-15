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

namespace ring {

class FileServer {
public:
    using SocketType = IceSocket;

    FileServer() : loop_([]{ return true; }, [this]{ process(); }, []{}) {};

    ~FileServer() {
        loop_.join(); // must be done before clearing socket map
    }

    void setPeerConnection(const std::string& peer, SocketType&& socket) {
        std::lock_guard<std::mutex> lk {socketMapMutex_};
        socketMap_.erase(peer);
        socket.setOnRecv([this, peer](uint8_t*, size_t len) {
                RING_WARN("rx %zuB", len);
                return len;
            });
        socketMap_.emplace(peer, std::move(socket));
    }

    void disconnectPeer(const std::string& peer) {
        std::lock_guard<std::mutex> lk {socketMapMutex_};
        socketMap_.erase(peer);
    }

    std::string newFileTransfer(const std::string& peer, std::ifstream&&) {
        return peer;
    }

    void process() {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (socketMap_.empty())
            return;

        auto& socket = std::begin(socketMap_)->second;
        std::string msg {"foobar"};
        socket.send(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.size());
    }

private:
    NON_COPYABLE(FileServer);
    std::mutex socketMapMutex_;
    std::map<std::string, SocketType> socketMap_; // map a peer contact to a socket
    ThreadLoop loop_;
};

} // namespace ring
