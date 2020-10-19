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

#include "ftp_server.h"

#include "logger.h"
#include "string_utils.h"
#include "manager.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <iterator>
#include <cstdlib> // strtoull
#include <opendht/thread_pool.h>

namespace jami {

//==============================================================================

FtpServer::FtpServer(const std::string& account_id,
                     const std::string& peer_uri,
                     const DRing::DataTransferId& outId,
                     InternalCompletionCb&& cb)
    : Stream()
    , accountId_ {account_id}
    , peerUri_ {peer_uri}
    , outId_ {outId}
    , cb_ {std::move(cb)}
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
    DRing::DataTransferInfo info {};
    info.accountId = accountId_;
    info.peer = peerUri_;
    info.displayName = displayName_;
    info.totalSize = fileSize_;
    info.bytesProgress = 0;
    rx_ = 0;
    transferId_ = Manager::instance()
                      .dataTransfers->createIncomingTransfer(info,
                                                             outId_,
                                                             cb_); // return immediately
    isTreatingRequest_ = true;
    Manager::instance().dataTransfers->onIncomingFileRequest(
        transferId_, [w = weak()](const IncomingFileInfo& fileInfo) {
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
                std::vector<uint8_t> buffer;
                if (shared->go_) {
                    buffer.resize(3);
                    buffer[0] = 'G';
                    buffer[1] = 'O';
                    buffer[2] = '\n';
                } else {
                    buffer.resize(4);
                    buffer[0] = 'N';
                    buffer[1] = 'G';
                    buffer[2] = 'O';
                    buffer[3] = '\n';
                }
                shared->onRecvCb_(std::move(buffer));
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
                    shared->out_.stream->write(reinterpret_cast<const uint8_t*>(
                                                &shared->line_[0]),
                                            count);
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
        });
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
FtpServer::read(std::vector<uint8_t>& buffer) const
{
    if (!out_.stream) {
        if (closed_.exchange(false)) {
            if (rx_ < fileSize_) {
                buffer.resize(4);
                buffer[0] = 'N';
                buffer[1] = 'G';
                buffer[2] = 'O';
                buffer[3] = '\n';
                JAMI_DBG() << "[FTP] sending NGO (cancel) order";
                return true;
            }
        }
        buffer.resize(0);
    } else if (go_) {
        go_ = false;
        buffer.resize(3);
        buffer[0] = 'G';
        buffer[1] = 'O';
        buffer[2] = '\n';
        JAMI_DBG() << "[FTP] sending GO order";
    } else {
        // Nothing to send. Avoid to have an useless buffer filled with 0.
        buffer.resize(0);
    }
    return true;
}

bool
FtpServer::write(const std::vector<uint8_t>& buffer)
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
            out_.stream->write(&buffer[0], buffer.size());
        auto size_needed = fileSize_ - rx_;
        auto read_size = std::min(buffer.size(), size_needed);
        rx_ += read_size;
        if (rx_ == fileSize_) {
            closeCurrentFile();
            // data may remains into the buffer: copy into the header stream for next header parsing
            if (read_size < buffer.size())
                headerStream_ << std::string(std::begin(buffer) + read_size, std::end(buffer));
            state_ = FtpState::PARSE_HEADERS;
        }
    } break;

    default:
        break;
    }

    return true; // server always alive
}

bool
FtpServer::parseStream(const std::vector<uint8_t>& buffer)
{
    headerStream_ << std::string(std::begin(buffer), std::end(buffer));

    // Simple line stream parser
    while (headerStream_.getline(&line_[0], line_.size())) {
        if (parseLine(std::string(&line_[0], headerStream_.gcount() - 1)))
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

    handleHeader(trim(line.substr(0, sep_pos)), trim(line.substr(sep_pos + 1)));
    return false;
}

void
FtpServer::handleHeader(const std::string& key, const std::string& value)
{
    JAMI_DBG() << "[FTP] header: '" << key << "' = '" << value << "'";

    if (key == "Content-Length") {
        fileSize_ = std::strtoull(&value[0], nullptr, 10);
    } else if (key == "Display-Name") {
        displayName_ = value;
    }
}

} // namespace jami
