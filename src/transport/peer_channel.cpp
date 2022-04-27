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

#include "peer_channel.h"

namespace jami {

ssize_t
PeerChannel::wait(std::chrono::steady_clock::duration timeout, std::error_code& ec)
{
    std::unique_lock<std::mutex> lk {mutex_};
    cv_.wait_for(lk, timeout, [this] { return stop_ or not stream_.empty(); });
    if (stop_) {
        ec = std::make_error_code(std::errc::interrupted);
        return -1;
    }
    ec.clear();
    return stream_.size();
}

ssize_t
PeerChannel::read(char* output, std::size_t size, std::error_code& ec)
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

ssize_t
PeerChannel::write(const char* data, std::size_t size, std::error_code& ec)
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

void
PeerChannel::stop() noexcept
{
    std::lock_guard<std::mutex> lk {mutex_};
    if (stop_)
        return;
    stop_ = true;
    cv_.notify_all();
}

} // namespace jami
