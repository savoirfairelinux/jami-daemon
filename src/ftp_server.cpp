/*
 *  Copyright (C) 2017-2018 Savoir-faire Linux Inc.
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

#include "ftp_server.h"

#include "logger.h"
#include "string_utils.h"
#include "data_transfer.h"
#include "manager.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <iterator>
#include <cstdlib> // strtoull

namespace ring {

//==============================================================================

FtpServer::FtpServer(const std::string& account_id,
                     const std::string& peer_uri)
    : Stream()
    , accountId_ {account_id}
    , peerUri_ {peer_uri}
{}

DRing::DataTransferId
FtpServer::getId() const
{
    return 0;
}

void
FtpServer::close() noexcept
{
    closeCurrentFile();
    RING_WARN() << "[FTP] server closed";
}

bool
FtpServer::startNewFile()
{
    // Request filename from client (WARNING: synchrone call!)
    DRing::DataTransferInfo info {};
    info.accountId = accountId_;
    info.peer = peerUri_;
    info.displayName = displayName_;
    info.totalSize = fileSize_;
    info.bytesProgress = 0;
    out_ = Manager::instance().dataTransfers->onIncomingFileRequest(info);
    return true;
}

void
FtpServer::closeCurrentFile()
{
    if (out_) {
        out_->close();
        out_.reset();
    }
}

bool
FtpServer::write(const std::vector<uint8_t>& buffer)
{
    switch (state_) {
        case FtpState::PARSE_HEADERS:
            if (parseStream(buffer)) {
                if (!startNewFile()) {
                    headerStream_.clear();
                    headerStream_.str({}); // reset
                    return true;
                }
                state_ = FtpState::READ_DATA;
                while (headerStream_) {
                    headerStream_.read(&line_[0], line_.size());
                    auto count = headerStream_.gcount();
                    if (!count)
                        continue;
                    if (out_)
                        out_->write(reinterpret_cast<const uint8_t*>(&line_[0]), count);
                    rx_ += count;
                    if (rx_ >= fileSize_) {
                        closeCurrentFile();
                        state_ = FtpState::PARSE_HEADERS;
                        break;
                    }
                }
                headerStream_.clear();
                headerStream_.str({}); // reset
            }
            break;

        case FtpState::READ_DATA:
            if (out_)
                out_->write(&buffer[0], buffer.size());
            rx_ += buffer.size();
            if (rx_ >= fileSize_) {
                closeCurrentFile();
                state_ = FtpState::PARSE_HEADERS;
            }
            break;

        default: break;
    }

    return true; // server always alive
}

bool
FtpServer::parseStream(const std::vector<uint8_t>& buffer)
{
    headerStream_ << std::string(std::begin(buffer), std::end(buffer));

    // Simple line stream parser
    while (headerStream_.getline(&line_[0], line_.size())) {
        if (parseLine(std::string(&line_[0], headerStream_.gcount()-1)))
            return true; // headers EOF, data may remain in headerStream_
    }

    if (headerStream_.fail())
        throw std::runtime_error("[FTP] header parsing error");

    headerStream_.clear();
    return false; // need more data
}

bool
FtpServer::parseLine(const std::string& line)
{
    if (line.empty())
        return true;

    // Valid line found, parse it as "key: value" and store until end of headers detection
    const auto& sep_pos = line.find(':');
    if (sep_pos == std::string::npos)
        throw std::runtime_error("[FTP] stream protocol error: bad format");

    handleHeader(trim(line.substr(0, sep_pos)), trim(line.substr(sep_pos+1)));
    return false;
}

void
FtpServer::handleHeader(const std::string& key, const std::string& value)
{
    RING_DBG() << "[FTP] header: '" << key << "' = '"<< value << "'";

    if (key == "Content-Length") {
        fileSize_ = std::strtoull(&value[0], nullptr, 10);
    } else if (key == "Display-Name") {
        displayName_ = value;
    }
}

} // namespace ring
