/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

#include <mutex>
#include <queue>
#include <condition_variable>
#include <stdexcept>
#include "logger.h"

///
/// \file Channel is a synchronized queue to share data between threads.
///
/// This is a C++11-ish class that mimic Python "queue" module and/or Go "Channel" type.
///

namespace jami
{

class ChannelEmpty : public std::exception{
public:
    const char* what() const noexcept { return "channel empty"; }
};

class ChannelFull : public std::exception {
public:
    const char* what() const noexcept { return "channel full"; }
};

namespace detail {

template <typename T, std::size_t N=0>
class _ChannelBase {
public:
    using value_type = T;
    const std::size_t max_size = N; ///< maximal size of the Channel, 0 means no size limits

    // Pop operations

    template <typename Duration>
    T receive(Duration timeout) {
        std::unique_lock<std::mutex> lk {mutex_};
        if (!cv_.wait_for(lk, timeout, [this]{ return !queue_.empty(); }))
            throw ChannelEmpty();
        auto value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    template <typename Duration>
    void receive(T& value, Duration timeout) {
        value = receive(timeout);
    }

    T receive() {
        std::unique_lock<std::mutex> lk {mutex_};
        if (queue_.empty())
            throw ChannelEmpty();
        auto value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    T receive_wait() {
        std::unique_lock<std::mutex> lk {mutex_};
        cv_.wait(lk, [this]{ return !queue_.empty(); });
        auto value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    void receive_wait(T& value) {
        value = receive_wait();
    }

    void wait() {
        std::unique_lock<std::mutex> lk {mutex_};
        cv_.wait(lk, [this]{ return !queue_.empty(); });
    }

    void operator >>(T& value) {
        receive_wait(value);
    }

    std::queue<T> flush() {
        std::unique_lock<std::mutex> lk {mutex_};
        std::queue<T> result;
        std::swap(queue_, result);
        return result;
    }

    // Capacity operation

    std::size_t size() const {
        std::lock_guard<std::mutex> lk {mutex_};
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk {mutex_};
        return queue_.empty();
    }

protected:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
};

} // namespace detail

///
/// Generic implementation
///
template <typename T, std::size_t N=0>
class Channel : public detail::_ChannelBase<T, N> {
public:
    using base = detail::_ChannelBase<T, N>;
    using base::mutex_;
    using base::cv_;
    using base::queue_;

    template <typename U>
    void send(U&& value) {
        std::lock_guard<std::mutex> lk {mutex_};
        if (queue_.size() < N) {
            queue_.push(std::forward<U>(value));
            cv_.notify_one();
            return;
        }
        throw ChannelFull();
    }

    template <typename... Args>
    void send_emplace(Args&&... args) {
        std::lock_guard<std::mutex> lk {mutex_};
        if (queue_.size() < N) {
            queue_.emplace(std::forward<Args>(args)...);
            cv_.notify_one();
            return;
        }
        throw ChannelFull();
    }

    template <typename U>
    void operator <<(U&& value) {
        send(std::forward<U>(value));
    }
};

///
/// Optimized implementations for unlimited channel (N=0)
///
template <typename T>
class Channel<T> : public detail::_ChannelBase<T> {
public:
    using base = detail::_ChannelBase<T>;
    using base::mutex_;
    using base::cv_;
    using base::queue_;

    template <typename U>
    void send(U&& value) {
        std::lock_guard<std::mutex> lk {mutex_};
        queue_.push(std::forward<U>(value));
        cv_.notify_one();
    }

    template <typename... Args>
    void send_emplace(Args&&... args) {
        std::lock_guard<std::mutex> lk {mutex_};
        queue_.emplace(std::forward<Args>(args)...);
        cv_.notify_one();
    }

    /// \note This method exists only for unlimited channel
    void send(const T* data, std::size_t len) {
        std::lock_guard<std::mutex> lk {mutex_};
        while (len > 0) {
            queue_.push(*(data++));
            --len;
        }
        cv_.notify_one();
    }

    template <typename U>
    void operator <<(U&& value) {
        send(std::forward<U>(value));
    }
};

} // namespace jami
