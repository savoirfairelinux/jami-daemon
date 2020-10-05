/*
 *  Copyright (C) 2020 Savoir-faire Linux Inc.
 *
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "channeled_transfers.h"

#include "ftp_server.h"
#include "multiplexed_socket.h"

#include <opendht/thread_pool.h>

#include "jamiaccount.h"

namespace jami {

ChanneledOutgoingTransfer::ChanneledOutgoingTransfer(const std::shared_ptr<ChannelSocket>& channel,
                                                     OnStateChangedCb&& cb)
    : channel_(channel)
    , stateChangedCb_(cb)
{}

ChanneledOutgoingTransfer::~ChanneledOutgoingTransfer()
{
    channel_->setOnRecv({});
    file_->setOnRecv({});
    channel_->shutdown();
    file_->close();
}

std::string
ChanneledOutgoingTransfer::peer() const
{
    return channel_ ? "" : channel_->deviceId().toString();
}

void
ChanneledOutgoingTransfer::linkTransfer(const std::shared_ptr<Stream>& file)
{
    if (!file)
        return;
    file_ = file;
    channel_->setOnRecv([this](const uint8_t* buf, size_t len) {
        dht::ThreadPool::io().run(
            [rx = std::vector<uint8_t>(buf, buf + len), file = std::weak_ptr<Stream>(file_)] {
                if (auto f = file.lock())
                    f->write(rx);
            });
        return len;
    });
    file_->setOnRecv(
        [channel = std::weak_ptr<ChannelSocket>(channel_)](std::vector<uint8_t>&& data) {
            if (auto c = channel.lock()) {
                std::error_code ec;
                c->write(data.data(), data.size(), ec);
            }
        });
    file_->setOnStateChangedCb(stateChangedCb_);
}

ChanneledIncomingTransfer::ChanneledIncomingTransfer(const std::shared_ptr<ChannelSocket>& channel,
                                                     const std::shared_ptr<FtpServer>& ftp,
                                                     OnStateChangedCb&& cb)
    : ftp_(ftp)
    , channel_(channel)
{
    channel_->setOnRecv([this](const uint8_t* buf, size_t len) {
        dht::ThreadPool::io().run(
            [rx = std::vector<uint8_t>(buf, buf + len), ftp = std::weak_ptr<FtpServer>(ftp_)] {
                if (auto f = ftp.lock())
                    f->write(rx);
            });
        return len;
    });
    ftp_->setOnRecv([channel = std::weak_ptr<ChannelSocket>(channel_)](std::vector<uint8_t>&& data) {
        if (auto c = channel.lock()) {
            std::error_code ec;
            c->write(data.data(), data.size(), ec);
        }
    });
    ftp_->setOnStateChangedCb(cb);
}

ChanneledIncomingTransfer::~ChanneledIncomingTransfer()
{
    channel_->setOnRecv({});
    channel_->shutdown();
    if (ftp_)
        ftp_->close();
}

} // namespace jami