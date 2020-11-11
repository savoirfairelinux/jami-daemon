/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#include "data_transfer.h"
#include "peer_connection.h"

#include <vector>
#include <array>
#include <sstream>
#include <memory>

namespace jami {

using RecvCb = std::function<void(std::string_view buf)>;

class FtpServer final : public Stream, public std::enable_shared_from_this<FtpServer>
{
public:
    FtpServer(const DRing::DataTransferInfo& info,
              const DRing::DataTransferId& id,
              const InternalCompletionCb& cb = {});

    bool write(std::string_view data) override;
    DRing::DataTransferId getId() const override;
    void close() noexcept override;

    void setOnRecv(RecvCb&& cb) override { onRecvCb_ = cb; }
    void setOnStateChangedCb(const OnStateChangedCb& cb) override
    {
        // If out_ is not attached, store the callback
        // inside a temporary object. Will be linked when out_.stream
        // will be attached
        if (out_.stream)
            out_.stream->setOnStateChangedCb(std::move(cb));
        else
            tmpOnStateChangedCb_ = std::move(cb);
    }

private:
    bool parseStream(std::string_view);
    bool parseLine(std::string_view);
    void handleHeader(std::string_view key, std::string_view value);
    void startNewFile();
    void closeCurrentFile();

    enum class FtpState {
        PARSE_HEADERS,
        WAIT_ACCEPTANCE,
        READ_DATA,
    };

    DRing::DataTransferInfo info_;
    InternalCompletionCb cb_ {};
    std::atomic_bool isVCard_ {false};
    std::atomic_bool isTreatingRequest_ {false};
    DRing::DataTransferId transferId_ {0};
    IncomingFileInfo out_ {0, nullptr};
    std::size_t fileSize_ {0};
    std::size_t rx_ {0};
    std::stringstream headerStream_;
    std::array<char, 1024> line_;
    mutable std::atomic_bool closed_ {false};
    mutable bool go_ {false};
    FtpState state_ {FtpState::PARSE_HEADERS};

    RecvCb onRecvCb_ {};
    OnStateChangedCb tmpOnStateChangedCb_ {};

    std::shared_ptr<FtpServer> shared()
    {
        return std::static_pointer_cast<FtpServer>(shared_from_this());
    }
    std::shared_ptr<FtpServer const> shared() const
    {
        return std::static_pointer_cast<FtpServer const>(shared_from_this());
    }
    std::weak_ptr<FtpServer> weak()
    {
        return std::static_pointer_cast<FtpServer>(shared_from_this());
    }
    std::weak_ptr<FtpServer const> weak() const
    {
        return std::static_pointer_cast<FtpServer const>(shared_from_this());
    }
};

} // namespace jami
