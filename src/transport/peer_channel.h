/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
 *  Authors: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *           Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <mutex>
#include <condition_variable>
#include <deque>
#include <algorithm>
#include <chrono>

namespace jami {

class PeerChannel
{
public:
    PeerChannel() {}
    ~PeerChannel() { stop(); }
    PeerChannel(PeerChannel&& o)
    {
        std::lock_guard<std::mutex> lk(o.mutex_);
        stream_ = std::move(o.stream_);
        stop_ = o.stop_;
        o.cv_.notify_all();
    }

    ssize_t wait(std::chrono::steady_clock::duration timeout, std::error_code& ec);
    ssize_t read(char* output, std::size_t size, std::error_code& ec);
    ssize_t write(const char* data, std::size_t size, std::error_code& ec);
    void stop() noexcept;

private:
    PeerChannel(const PeerChannel& o) = delete;
    PeerChannel& operator=(const PeerChannel& o) = delete;
    PeerChannel& operator=(PeerChannel&& o) = delete;

    std::mutex mutex_ {};
    std::condition_variable cv_ {};
    std::deque<char> stream_;
    bool stop_ {false};
};

} // namespace jami
