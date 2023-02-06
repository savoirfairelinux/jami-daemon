/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
    ~PeerChannel() { stop(); }
    PeerChannel(PeerChannel&& o)
    {
        std::lock_guard<std::mutex> lk(o.mutex_);
        stream_ = std::move(o.stream_);
        stop_ = o.stop_;
        o.cv_.notify_all();
    }

    template<typename Duration>
    ssize_t wait(Duration timeout, std::error_code& ec)
    {
        std::unique_lock<std::mutex> lk {mutex_};
        cv_.wait_for(lk, timeout, [this] { return stop_ or not stream_.empty(); });
        if (stop_) {
            ec = std::make_error_code(std::errc::broken_pipe);
            return -1;
        }
        ec.clear();
        return stream_.size();
    }

    ssize_t read(char* output, std::size_t size, std::error_code& ec)
    {
        std::unique_lock<std::mutex> lk {mutex_};
        cv_.wait(lk, [this] { return stop_ or not stream_.empty(); });
        if (stream_.size()) {
            auto toRead = std::min(size, stream_.size());
            if (toRead) {
                auto endIt = stream_.begin() + toRead;
                std::copy(stream_.begin(), endIt, output);
                stream_.erase(stream_.begin(), endIt);
            }
            ec.clear();
            return toRead;
        }
        if (stop_) {
            ec.clear();
            return 0;
        }
        ec = std::make_error_code(std::errc::resource_unavailable_try_again);
        return -1;
    }

    ssize_t write(const char* data, std::size_t size, std::error_code& ec)
    {
        std::lock_guard<std::mutex> lk {mutex_};
        if (stop_) {
            ec = std::make_error_code(std::errc::broken_pipe);
            return -1;
        }
        stream_.insert(stream_.end(), data, data + size);
        cv_.notify_all();
        ec.clear();
        return size;
    }

    void stop() noexcept
    {
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
};

} // namespace jami
