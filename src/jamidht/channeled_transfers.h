/*
 *  Copyright (C)2020-2021 Savoir-faire Linux Inc.
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

#pragma once

#include <string>
#include <memory>

#include "dring/datatransfer_interface.h"
#include "data_transfer.h"

namespace jami {

class ChannelSocket;
class Stream;
class FtpServer;

class ChanneledOutgoingTransfer
{
public:
    ChanneledOutgoingTransfer(const std::shared_ptr<ChannelSocket>& channel, OnStateChangedCb&& cb);
    ~ChanneledOutgoingTransfer();
    void linkTransfer(const std::shared_ptr<Stream>& file);
    std::string peer() const;

private:
    OnStateChangedCb stateChangedCb_ {};
    std::shared_ptr<ChannelSocket> channel_ {};
    std::shared_ptr<Stream> file_;
};

class ChanneledIncomingTransfer
{
public:
    ChanneledIncomingTransfer(const std::shared_ptr<ChannelSocket>& channel,
                              const std::shared_ptr<FtpServer>& ftp,
                              OnStateChangedCb&& cb);
    ~ChanneledIncomingTransfer();
    std::string peer() const;

private:
    std::shared_ptr<FtpServer> ftp_;
    std::shared_ptr<ChannelSocket> channel_;
};

} // namespace jami
