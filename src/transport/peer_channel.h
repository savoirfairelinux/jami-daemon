/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
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

namespace jami {

class PeerChannel
{
public:
    PeerChannel() {}
    ~PeerChannel() {
        stop();
    }
    PeerChannel(PeerChannel&& o) {
        std::lock_guard<std::mutex> lk(o.mutex_);
        stream_ = std::move(o.stream_);
        stop_ = o.stop_;
    }

    ssize_t isDataAvailable() {
        std::lock_guard<std::mutex> lk{mutex_};
        return getDataAvailable();
    }

    template <typename Duration>
    ssize_t wait(Duration timeout) {
        std::unique_lock<std::mutex> lk {mutex_};
        cv_.wait_for(lk, timeout, [this]{ return stop_ or not stream_.empty(); });
        return getDataAvailable();
    }

    std::size_t read(char* output, std::size_t size) {
        std::unique_lock<std::mutex> lk {mutex_};
        cv_.wait(lk, [&, this]{
                return stop_ or not stream_.empty();
            });
        if (stop_)
            return 0;
        auto toRead = std::min(size, stream_.size());
        auto endIt = stream_.begin()+toRead;
        std::copy(stream_.begin(), endIt, output);
        stream_.erase(stream_.begin(), endIt);
        return toRead;
    }

    void write(const char* data, std::size_t size) {
        std::lock_guard<std::mutex> lk {mutex_};
        stream_.insert(stream_.end(), data, data+size);
        cv_.notify_one();
    }

    void stop() noexcept {
        std::lock_guard<std::mutex> lk {mutex_};
        if (stop_)
            return;
        stop_ = true;
        cv_.notify_all();
    }

private:
    PeerChannel(const PeerChannel& o) = delete;
    PeerChannel& operator=(const PeerChannel& o) = delete;
    PeerChannel& operator=(PeerChannel&& o) = delete;

    std::mutex mutex_ {};
    std::condition_variable cv_ {};
    std::deque<char> stream_;
    bool stop_ {false};

    ssize_t getDataAvailable() {
        return stream_.size();
    }
};

}
