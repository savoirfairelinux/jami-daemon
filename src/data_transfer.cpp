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

#include "data_transfer.h"

#include "manager.h"
#include "ringdht/ringaccount.h"
#include "peer_connection.h"
#include "fileutils.h"
#include "string_utils.h"
#include "map_utils.h"
#include "client/ring_signal.h"

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <ios>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <future>
#include <atomic>
#include <cstdlib> // mkstemp

namespace ring {

static DRing::DataTransferId
generateUID()
{
    static DRing::DataTransferId lastId = 0;
    return lastId++;
}

//==============================================================================

class DataTransfer : public Stream
{
public:
    DataTransfer(DRing::DataTransferId id) : Stream(), id {id} {}

    virtual ~DataTransfer() = default;

    DRing::DataTransferId getId() const override {
        return id;
    }

    virtual void accept(const std::string&, std::size_t) {};

    virtual bool start() {
        bool expected = false;
        return started_.compare_exchange_strong(expected, true);
    }

    void close() noexcept override {
        started_ = false;
    }

    void bytesProgress(int64_t& total, int64_t& progress) const {
        std::lock_guard<std::mutex> lk {infoMutex_};
        total = info_.totalSize;
        progress = info_.bytesProgress;
    }

    void info(DRing::DataTransferInfo& info) const {
        std::lock_guard<std::mutex> lk {infoMutex_};
        info = info_;
    }

    void emit(DRing::DataTransferEventCode code) const;

    const DRing::DataTransferId id;

protected:
    mutable std::mutex infoMutex_;
    mutable DRing::DataTransferInfo info_;
    std::atomic_bool started_ {false};
};

void
DataTransfer::emit(DRing::DataTransferEventCode code) const
{
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        info_.lastEvent = code;
    }
    emitSignal<DRing::DataTransferSignal::DataTransferEvent>(id, uint32_t(code));
}

//==============================================================================

class FileTransfer final : public DataTransfer
{
public:
    FileTransfer(DRing::DataTransferId tid, const DRing::DataTransferInfo& info);

    bool start() override;

    void close() noexcept override;

    bool read(std::vector<uint8_t>&) const override;

private:
    FileTransfer() = delete;

    mutable std::ifstream input_;
    mutable std::size_t tx_ {0};
    mutable bool headerSent_ {false};
    const std::string peerUri_;
};

FileTransfer::FileTransfer(DRing::DataTransferId tid, const DRing::DataTransferInfo& info)
    : DataTransfer(tid)
{
    input_.open(info.path, std::ios::binary);
    if (!input_)
        throw std::runtime_error("input file open failed");

    info_ = info;
    info_.flags &= ~((uint32_t)1 << int(DRing::DataTransferFlags::direction)); // outgoing

    // File size?
    input_.seekg(0, std::ios_base::end);
    info_.totalSize = input_.tellg();
    input_.seekg(0, std::ios_base::beg);
}

bool
FileTransfer::start()
{
    if (DataTransfer::start()) {
        emit(DRing::DataTransferEventCode::ongoing);
        return true;
    }
    return false;
}

void
FileTransfer::close() noexcept
{
    DataTransfer::close();
    input_.close();
    if (info_.lastEvent < DRing::DataTransferEventCode::finished)
        emit(DRing::DataTransferEventCode::closed_by_host);
}

bool
FileTransfer::read(std::vector<uint8_t>& buf) const
{
    if (!headerSent_) {
        std::stringstream ss;
        ss << "Content-Length: " << info_.totalSize << '\n'
           << "Display-Name: " << info_.displayName << '\n'
           << "Offset: 0\n"
           << '\n';

        auto header = ss.str();
        buf.resize(header.size());
        std::copy(std::begin(header), std::end(header), std::begin(buf));

        headerSent_ = true;
        return true;
    }

    input_.read(reinterpret_cast<char*>(&buf[0]), buf.size());
    auto n = input_.gcount();
    buf.resize(n);
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        info_.bytesProgress += n;
    }

    if (n)
        return true;

    if (input_.eof()) {
        RING_DBG() << "FTP#" << getId() << ": sent " << info_.bytesProgress << " bytes";
        emit(DRing::DataTransferEventCode::finished);
        return false;
    } else {
        throw std::runtime_error("FileTransfer IO read failed"); // TODO: better exception?
    }

    return true;
}

//==============================================================================

class IncomingFileTransfer final : public DataTransfer
{
public:
    IncomingFileTransfer(DRing::DataTransferId, const DRing::DataTransferInfo&);

    bool start() override;

    void close() noexcept override;

    std::string requestFilename();

    void accept(const std::string&, std::size_t offset) override;

    bool write(const uint8_t* buffer, std::size_t length) override;

private:
    IncomingFileTransfer() = delete;

    std::ofstream fout_;
    std::promise<void> filenamePromise_;
};

IncomingFileTransfer::IncomingFileTransfer(DRing::DataTransferId tid,
                                           const DRing::DataTransferInfo& info)
    : DataTransfer(tid)
{
    RING_WARN() << "[FTP] incoming transfert of " << info.totalSize << " byte(s): " << info.displayName;

    info_ = info;
    info_.flags |= (uint32_t)1 << int(DRing::DataTransferFlags::direction); // incoming
}

std::string
IncomingFileTransfer::requestFilename()
{
    emit(DRing::DataTransferEventCode::wait_host_acceptance);

#if 1
    // Now wait for DataTransferFacade::acceptFileTransfer() call
    filenamePromise_.get_future().wait();
#else
    // For DEBUG only
    char filename[] = "/tmp/ring_XXXXXX";
    if (::mkstemp(filename) < 0)
        throw std::system_error(errno, std::generic_category());
    info_.path = filename;
#endif
    return info_.path;
}

bool
IncomingFileTransfer::start()
{
    if (!DataTransfer::start())
        return false;

    fout_.open(&info_.path[0], std::ios::binary);
    if (!fout_) {
        RING_ERR() << "[FTP] Can't open file " << info_.path;
        return false;
    }

    emit(DRing::DataTransferEventCode::ongoing);
    return true;
}

void
IncomingFileTransfer::close() noexcept
{
    DataTransfer::close();

    try {
        filenamePromise_.set_value();
    } catch (...) {}

    fout_.close();

    RING_DBG() << "[FTP] file closed, rx " << info_.bytesProgress
               << " on " << info_.totalSize;
    if (info_.bytesProgress >= info_.totalSize)
        emit(DRing::DataTransferEventCode::finished);
    else
        emit(DRing::DataTransferEventCode::closed_by_host);
}

void
IncomingFileTransfer::accept(const std::string& filename, std::size_t offset)
{
    // TODO: offset?
    (void)offset;

    info_.path = filename;
    filenamePromise_.set_value();
}

bool
IncomingFileTransfer::write(const uint8_t* buffer, std::size_t length)
{
    if (!length)
        return true;
    fout_.write(reinterpret_cast<const char*>(buffer), length);
    if (!fout_)
        return false;
    std::lock_guard<std::mutex> lk {infoMutex_};
    info_.bytesProgress += length;
    return true;
}

//==============================================================================

class DataTransferFacade::Impl
{
public:
    mutable std::mutex mapMutex_;
    std::unordered_map<DRing::DataTransferId, std::shared_ptr<DataTransfer>> map_;

    std::shared_ptr<DataTransfer> createFileTransfer(const DRing::DataTransferInfo& info,
                                                     DRing::DataTransferId& tid);
    std::shared_ptr<IncomingFileTransfer> createIncomingFileTransfer(const DRing::DataTransferInfo&);
    std::shared_ptr<DataTransfer> getTransfer(const DRing::DataTransferId&);
    void cancel(DataTransfer&);
    void onConnectionRequestReply(const DRing::DataTransferId&, PeerConnection*);
};

void DataTransferFacade::Impl::cancel(DataTransfer& transfer)
{
    transfer.close();
    map_.erase(transfer.getId());
}

std::shared_ptr<DataTransfer>
DataTransferFacade::Impl::getTransfer(const DRing::DataTransferId& id)
{
    std::lock_guard<std::mutex> lk {mapMutex_};
    const auto& iter = map_.find(id);
    if (iter == std::end(map_))
        return {};
    return iter->second;
}

std::shared_ptr<DataTransfer>
DataTransferFacade::Impl::createFileTransfer(const DRing::DataTransferInfo& info,
                                             DRing::DataTransferId& tid)
{
    tid = generateUID();
    auto transfer = std::make_shared<FileTransfer>(tid, info);
    {
        std::lock_guard<std::mutex> lk {mapMutex_};
        map_.emplace(tid, transfer);
    }
    transfer->emit(DRing::DataTransferEventCode::created);
    return transfer;
}

std::shared_ptr<IncomingFileTransfer>
DataTransferFacade::Impl::createIncomingFileTransfer(const DRing::DataTransferInfo& info)
{
    auto tid = generateUID();
    auto transfer = std::make_shared<IncomingFileTransfer>(tid, info);
    {
        std::lock_guard<std::mutex> lk {mapMutex_};
        map_.emplace(tid, transfer);
    }
    transfer->emit(DRing::DataTransferEventCode::created);
    return transfer;
}

void
DataTransferFacade::Impl::onConnectionRequestReply(const DRing::DataTransferId& id,
                                                   PeerConnection* connection)
{
    if (auto transfer = getTransfer(id)) {
        if (connection) {
            if (transfer->start()) {
                connection->attachInputStream(transfer);
            }
        } else {
            transfer->emit(DRing::DataTransferEventCode::unjoinable_peer);
            cancel(*transfer);
        }
    }
}

//==============================================================================

DataTransferFacade::DataTransferFacade() : pimpl_ {std::make_unique<Impl>()}
{
    RING_WARN("[XFER] facade created, pimpl @%p", pimpl_.get());
}

DataTransferFacade::~DataTransferFacade()
{
    RING_WARN("[XFER] facade destroy, pimpl @%p", pimpl_.get());
};

std::vector<DRing::DataTransferId>
DataTransferFacade::list() const noexcept
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    return map_utils::extractKeys(pimpl_->map_);
}

DRing::DataTransferError
DataTransferFacade::sendFile(const DRing::DataTransferInfo& info,
                             DRing::DataTransferId& tid) noexcept
{
    auto account = Manager::instance().getAccount<RingAccount>(info.accountId);
    if (!account) {
        RING_ERR() << "[XFER] unknown id " << tid;
        return DRing::DataTransferError::invalid_argument;
    }

    if (!fileutils::isFile(info.path)) {
        RING_ERR() << "[XFER] invalid filename '" << info.path << "'";
        return DRing::DataTransferError::invalid_argument;
    }

    try {
        pimpl_->createFileTransfer(info, tid);
    } catch (const std::exception& ex) {
        RING_ERR() << "[XFER] exception during createFileTransfer(): " << ex.what();
        return DRing::DataTransferError::io;
    }

    try {
        // IMPLEMENTATION NOTE: requestPeerConnection() may call the given callback a multiple time.
        // This happen when multiple agents handle communications of the given peer for the given account.
        // Example: Ring account supports multi-devices, each can answer to the request.
        account->requestPeerConnection(
            info.peer,
            [this, tid] (PeerConnection* connection) {
                pimpl_->onConnectionRequestReply(tid, connection);
            });
        return DRing::DataTransferError::success;
    } catch (const std::exception& ex) {
        RING_ERR() << "[XFER] exception during sendFile(): " << ex.what();
        return DRing::DataTransferError::unknown;
    }
}

DRing::DataTransferError
DataTransferFacade::acceptAsFile(const DRing::DataTransferId& id,
                                 const std::string& file_path,
                                 int64_t offset) noexcept
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    const auto& iter = pimpl_->map_.find(id);
    if (iter == std::end(pimpl_->map_))
        return DRing::DataTransferError::invalid_argument;;
    iter->second->accept(file_path, offset);
    return DRing::DataTransferError::success;
}

DRing::DataTransferError
DataTransferFacade::cancel(const DRing::DataTransferId& id) noexcept
{
    if (auto transfer = pimpl_->getTransfer(id)) {
        pimpl_->cancel(*transfer);
        return DRing::DataTransferError::success;
    }
    return DRing::DataTransferError::invalid_argument;
}

DRing::DataTransferError
DataTransferFacade::bytesProgress(const DRing::DataTransferId& id,
                                  int64_t& total, int64_t& progress) const noexcept
{
    try {
        if (auto transfer = pimpl_->getTransfer(id)) {
            transfer->bytesProgress(total, progress);
            return DRing::DataTransferError::success;
        }
        return DRing::DataTransferError::invalid_argument;
    } catch (const std::exception& ex) {
        RING_ERR() << "[XFER] exception during bytesProgress(): " << ex.what();
    }
    return DRing::DataTransferError::unknown;
}

DRing::DataTransferError
DataTransferFacade::info(const DRing::DataTransferId& id,
                         DRing::DataTransferInfo& info) const noexcept
{
    try {
        if (auto transfer = pimpl_->getTransfer(id)) {
            transfer->info(info);
            return DRing::DataTransferError::success;
        }
        return DRing::DataTransferError::invalid_argument;
    } catch (const std::exception& ex) {
        RING_ERR() << "[XFER] exception during info(): " << ex.what();
    }
    return DRing::DataTransferError::unknown;
}

std::shared_ptr<Stream>
DataTransferFacade::onIncomingFileRequest(const DRing::DataTransferInfo& info)
{
    auto transfer = pimpl_->createIncomingFileTransfer(info);
    auto filename = transfer->requestFilename();
    if (!filename.empty())
        if (transfer->start())
            return std::static_pointer_cast<Stream>(transfer);
    return {};
}

} // namespace ring
