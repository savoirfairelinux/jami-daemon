/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <dhtnet/channel_socket.h>

#include <memory>
#include <utility>

namespace jami {

/**
 * Exclusive ownership of a git channel.
 *
 * Letting go of a shared_ptr to a channel closes nothing: the channel stays
 * registered in its multiplexed socket, and the peer keeps a GitServer running
 * for it, until one side sends EOF. Storing a channel in this holder makes the
 * slot holding it its owner, so a channel cannot be forgotten without being
 * closed.
 */
class GitSocket
{
public:
    GitSocket() = default;
    GitSocket(std::shared_ptr<dhtnet::ChannelSocket> socket) noexcept
        : socket_(std::move(socket))
    {}
    GitSocket(GitSocket&&) noexcept = default;
    GitSocket& operator=(GitSocket&& other) noexcept
    {
        if (this != &other) {
            // Re-taking ownership of the channel we already hold must not close it.
            if (socket_ != other.socket_)
                reset();
            socket_ = std::move(other.socket_);
        }
        return *this;
    }
    GitSocket(const GitSocket&) = delete;
    GitSocket& operator=(const GitSocket&) = delete;
    ~GitSocket() { reset(); }

    explicit operator bool() const noexcept { return static_cast<bool>(socket_); }
    dhtnet::ChannelSocket* operator->() const noexcept { return socket_.get(); }
    const std::shared_ptr<dhtnet::ChannelSocket>& get() const noexcept { return socket_; }

    /** Close the channel and let go of it. */
    void reset()
    {
        if (auto socket = std::move(socket_))
            socket->shutdown();
    }

    /** Hand the channel over to another owner, leaving it open. */
    std::shared_ptr<dhtnet::ChannelSocket> release() noexcept
    {
        return std::exchange(socket_, nullptr);
    }

private:
    std::shared_ptr<dhtnet::ChannelSocket> socket_ {};
};

} // namespace jami
