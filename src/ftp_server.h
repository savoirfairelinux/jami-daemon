/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

using RecvCb = std::function<void(std::vector<uint8_t>&& buf)>;

class FtpServer final : public Stream
{
public:
    FtpServer(const std::string& account_id,
              const std::string& peer_uri,
              const DRing::DataTransferId& outId = 0,
              InternalCompletionCb&& cb = {});

    bool read(std::vector<uint8_t>& buffer) const override;
    bool write(const std::vector<uint8_t>& buffer) override;
    DRing::DataTransferId getId() const override;
    void close() noexcept override;

    void setOnRecv(RecvCb&& cb) { onRecvCb_ = cb; }
    void setOnStateChangedCb(const OnStateChangedCb& cb)
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
    bool parseStream(const std::vector<uint8_t>&);
    bool parseLine(const std::string&);
    void handleHeader(const std::string&, const std::string&);
    bool startNewFile();
    void closeCurrentFile();

    enum class FtpState {
        PARSE_HEADERS,
        READ_DATA,
    };

    const std::string accountId_;
    const std::string peerUri_;
    std::atomic_bool isVCard_ {false};
    std::atomic_bool isTreatingRequest_ {false};
    DRing::DataTransferId transferId_ {0};
    IncomingFileInfo out_ {0, nullptr};
    DRing::DataTransferId outId_ {0};
    std::size_t fileSize_ {0};
    std::size_t rx_ {0};
    std::stringstream headerStream_;
    std::string displayName_;
    std::array<char, 1000> line_;
    mutable std::atomic_bool closed_ {false};
    mutable bool go_ {false};
    FtpState state_ {FtpState::PARSE_HEADERS};

    RecvCb onRecvCb_ {};
    InternalCompletionCb cb_ {};
    OnStateChangedCb tmpOnStateChangedCb_ {};
};

} // namespace jami
