/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
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

#include <algorithm>
#include <array>
#include <stdexcept>
#include <iterator>
#include <cstdlib> // strtoull, mkstemp

namespace ring {

//==============================================================================

FtpServer::FtpServer()
    : Stream()
{
    char filename[] = "/tmp/ring_XXXXXX";
    if (::mkstemp(filename) < 0)
        throw std::system_error(errno, std::system_category());
    out_.open(&filename[0], std::ios::binary);
    if (!out_)
        throw std::system_error(errno, std::system_category());
    RING_WARN() << "Receiving file " << filename;
}

DRing::DataTransferId
FtpServer::getId() const
{
    return 0;
}

void
FtpServer::close() noexcept
{
    RING_WARN() << "Output closed, " << rx_ << " bytes received";
    out_.close();
}

bool
FtpServer::write(const std::vector<char>& buffer)
{
    switch (state_) {
        case FtpState::PARSE_HEADERS: {
            if (parseStream(buffer)) {
                state_ = FtpState::READ_DATA;
                while (headerStream_) {
                    headerStream_.read(&line_[0], line_.size());
                    out_.write(&line_[0], headerStream_.gcount());
                    rx_ += headerStream_.gcount();
                    RING_DBG() << "rx: " << rx_;
                    if (rx_ >= fileSize_)
                        return false; // EOF
                }
            }
            break;
        }

        case FtpState::READ_DATA:
            if (rx_ >= fileSize_)
                return false; // EOF
            out_.write(&buffer[0], buffer.size());
            rx_ += buffer.size();
            RING_DBG() << "rx: " << rx_;
            break;

        default: break;
    }

    return true; // need more data
}

bool
FtpServer::parseStream(const std::vector<char>& buffer)
{
    headerStream_ << std::string(std::begin(buffer), std::end(buffer));
    RING_DBG() << "FTP steam: '" << headerStream_.str() << "'[EOF]";

    // Simple line stream parser
    while (headerStream_.getline(&line_[0], line_.size())) {
        if (parseLine(std::string(&line_[0], headerStream_.gcount()-1)))
            return true; // headers EOF, data may remain in headerStream_
    }

    if (headerStream_.fail()) {
        throw std::runtime_error("FTP header parsing error");
    }

    headerStream_.clear();
    return false; // need more data
}

bool
FtpServer::parseLine(const std::string& line)
{
    RING_DBG() << "FTP line: '" << line << "'";
    if (line.empty())
        return true;

    // Valid line found, parse it as "key: value" and store until end of headers detection
    const auto& sep_pos = line.find(':');
    if (sep_pos == std::string::npos)
        throw std::runtime_error("stream protocol error: bad format");

    handleHeader(trim(line.substr(0, sep_pos)), trim(line.substr(sep_pos+1)));
    return false;
}

void
FtpServer::handleHeader(const std::string& key, const std::string& value)
{
    RING_DBG() << "FTP header: '" << key << "' = '"<< value << "'";

    if (key == "Content-Length") {
        fileSize_ = ::strtoull(&value[0], nullptr, 10);
        RING_DBG() << "FTP file size: " << fileSize_;
    }
}

} // namespace ring
