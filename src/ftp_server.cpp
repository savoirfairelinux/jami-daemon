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

#include "ftp_server.h"

#include "logger.h"
#include "string_utils.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"

#include <opendht/thread_pool.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <iterator>
#include <charconv>

using namespace std::literals;

namespace jami {

//==============================================================================

FtpServer::FtpServer(const DRing::DataTransferInfo& info,
                     const DRing::DataTransferId& id,
                     const InternalCompletionCb& cb)
    : Stream()
    , info_ {info}
    , transferId_(id)
    , cb_(cb)
{}

DRing::DataTransferId
FtpServer::getId() const
{
    // Because FtpServer is just the protocol on the top of a stream so the id
    // of the stream is the id of out_.
    if (isTreatingRequest_)
        return transferId_;
    return out_.id;
}

void
FtpServer::close() noexcept
{
    closeCurrentFile();
    JAMI_WARN() << "[FTP] server closed";
}

void
FtpServer::startNewFile()
{
    // Request filename from client (WARNING: synchrone call!)
    info_.totalSize = fileSize_;
    info_.bytesProgress = 0;
    rx_ = 0;
    isTreatingRequest_ = true;

    auto to = info_.conversationId;
    if (to.empty())
        to = info_.peer;

    if (auto acc = Manager::instance().getAccount<JamiAccount>(info_.accountId)) {
        acc->dataTransfer()->onIncomingFileRequest(
            info_,
            transferId_,
            [w = weak()](const IncomingFileInfo& fileInfo) {
                auto shared = w.lock();
                if (!shared)
                    return;
                shared->out_ = fileInfo;
                shared->isTreatingRequest_ = false;
                if (!shared->out_.stream) {
                    JAMI_DBG() << "[FTP] transfer aborted by client";
                    shared->closed_ = true; // send NOK msg at next read()
                } else {
                    if (shared->tmpOnStateChangedCb_)
                        shared->out_.stream->setOnStateChangedCb(
                            std::move(shared->tmpOnStateChangedCb_));
                    shared->go_ = true;
                }

                if (shared->onRecvCb_) {
                    shared->onRecvCb_(shared->go_ ? "GO\n"sv : "NGO\n"sv);
                }

                if (shared->out_.stream) {
                    shared->state_ = FtpState::READ_DATA;
                    while (shared->headerStream_) {
                        shared->headerStream_.read(&shared->line_[0], shared->line_.size());
                        std::size_t count = shared->headerStream_.gcount();
                        if (!count)
                            break;
                        auto size_needed = shared->fileSize_ - shared->rx_;
                        count = std::min(count, size_needed);
                        shared->out_.stream->write(std::string_view(shared->line_.data(), count));
                        shared->rx_ += count;
                        if (shared->rx_ == shared->fileSize_) {
                            shared->closeCurrentFile();
                            shared->state_ = FtpState::PARSE_HEADERS;
                            return;
                        }
                    }
                }
                shared->headerStream_.clear();
                shared->headerStream_.str({}); // reset
            },
            std::move(cb_));
    }
}

void
FtpServer::closeCurrentFile()
{
    if (out_.stream && not closed_.exchange(true)) {
        out_.stream->close();
        out_.stream.reset();
    }
}

bool
FtpServer::write(std::string_view buffer)
{
    switch (state_) {
    case FtpState::WAIT_ACCEPTANCE:
        // Receiving data while waiting, this is incorrect, because we didn't accept yet
        closeCurrentFile();
        state_ = FtpState::PARSE_HEADERS;
        break;
    case FtpState::PARSE_HEADERS:
        if (parseStream(buffer)) {
            state_ = FtpState::WAIT_ACCEPTANCE;
            startNewFile();
        }
        break;

    case FtpState::READ_DATA: {
        if (out_.stream)
            out_.stream->write(buffer);
        auto size_needed = fileSize_ - rx_;
        auto read_size = std::min(buffer.size(), size_needed);
        rx_ += read_size;
        if (rx_ == fileSize_) {
            closeCurrentFile();
            // data may remains into the buffer: copy into the header stream for next header parsing
            if (read_size < buffer.size())
                headerStream_.write((const char*) (buffer.data() + read_size),
                                    buffer.size() - read_size);
            state_ = FtpState::PARSE_HEADERS;
        }
    } break;

    default:
        break;
    }

    return true; // server always alive
}

bool
FtpServer::parseStream(std::string_view buffer)
{
    headerStream_ << buffer;

    // Simple line stream parser
    while (headerStream_.getline(&line_[0], line_.size())) {
        if (parseLine(std::string_view(line_.data(), headerStream_.gcount() - 1)))
            return true; // headers EOF, data may remain in headerStream_
    }

    if (headerStream_.fail())
        throw std::runtime_error("[FTP] header parsing error");

    headerStream_.clear();
    return false; // need more data
}

bool
FtpServer::parseLine(std::string_view line)
{
    if (line.empty())
        return true;

    // Valid line found, parse it as "key: value" and store until end of headers detection
    const auto& sep_pos = line.find(':');
    if (sep_pos == std::string_view::npos)
        throw std::runtime_error("[FTP] stream protocol error: bad format");

    handleHeader(trim(line.substr(0, sep_pos)), trim(line.substr(sep_pos + 1)));
    return false;
}

void
FtpServer::handleHeader(std::string_view key, std::string_view value)
{
    JAMI_DBG() << "[FTP] header: '" << key << "' = '" << value << "'";

    if (key == "Content-Length") {
        auto [p, ec] = std::from_chars(value.data(), value.data() + value.size(), fileSize_);
        if (ec != std::errc()) {
            throw std::runtime_error("[FTP] header parsing error");
        }
    } else if (key == "Display-Name") {
        info_.displayName = value;
    }
}

} // namespace jami
