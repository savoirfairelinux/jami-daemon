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

#include "peer_connection.h"

#include <vector>
#include <array>
#include <sstream>
#include <memory>

namespace jami {

class FtpServer final : public Stream
{
public:
    FtpServer(const std::string& account_id, const std::string& peer_uri);

    bool read(std::vector<uint8_t>& buffer) const override;
    bool write(const std::vector<uint8_t>& buffer) override;
    DRing::DataTransferId getId() const override;
    void close() noexcept override;

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
    std::shared_ptr<Stream> out_;
    std::size_t fileSize_ {0};
    std::size_t rx_ {0};
    std::stringstream headerStream_;
    std::string displayName_;
    std::array<char, 1000> line_;
    mutable bool closed_ {false};
    mutable bool go_ {false};
    FtpState state_ {FtpState::PARSE_HEADERS};
};

} // namespace jami
