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

    virtual std::streamsize bytesProgress() const {
        std::lock_guard<std::mutex> lk {infoMutex_};
        return info_.bytesProgress;
    }

    DRing::DataTransferInfo info() const {
        bytesProgress();
        std::lock_guard<std::mutex> lk {infoMutex_};
        return info_;
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
    info_.isOutgoing = true;

    // File size?
    input_.seekg(0, std::ios_base::end);
    info_.totalSize = input_.tellg();
    input_.seekg(0, std::ios_base::beg);

    emit(DRing::DataTransferEventCode::created);
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
    IncomingFileTransfer(DRing::DataTransferId, const DRing::DataTransferInfo&,
                         std::atomic<std::size_t>&);

    bool start() override;

    void close() noexcept override;

    std::streamsize bytesProgress() const override;

    std::string requestFilename();

    void accept(const std::string&, std::size_t offset) override;

private:
    IncomingFileTransfer() = delete;

    std::atomic<std::size_t>& progressStorage_;
    std::promise<void> filenamePromise_;
};

IncomingFileTransfer::IncomingFileTransfer(DRing::DataTransferId tid,
                                           const DRing::DataTransferInfo& info,
                                           std::atomic<std::size_t>& progress_storage)
    : DataTransfer(tid)
    , progressStorage_ {progress_storage}
{
    RING_WARN() << "[FTP] incoming transfert of " << info.totalSize << " byte(s): " << info.displayName;

    info_ = info;
    info_.isOutgoing = false;

    emit(DRing::DataTransferEventCode::created);
}

std::streamsize
IncomingFileTransfer::bytesProgress() const
{
    std::lock_guard<std::mutex> lk {infoMutex_};
    info_.bytesProgress = progressStorage_.load();
    return info_.bytesProgress;
}

std::string
IncomingFileTransfer::requestFilename()
{
    emit(DRing::DataTransferEventCode::wait_host_acceptance);

#if 0
    // Now wait for DataTransferFacade::acceptFileTransfer() call
    filenamePromise_.get_future().wait();
    return info_.path;
#else
    // For DEBUG only
    char filename[] = "/tmp/ring_XXXXXX";
    if (::mkstemp(filename) < 0)
        throw std::system_error(errno, std::generic_category());
    return filename;
#endif
}

bool
IncomingFileTransfer::start()
{
    if (DataTransfer::start()) {
        emit(DRing::DataTransferEventCode::ongoing);
        return true;
    }
    return false;
}

void
IncomingFileTransfer::close() noexcept
{
    filenamePromise_.set_value();
}

void
IncomingFileTransfer::accept(const std::string& filename, std::size_t offset)
{
    // TODO: offset?
    (void)offset;

    info_.path = filename;
    filenamePromise_.set_value();
    start();
}

//==============================================================================

class DataTransferFacade::Impl
{
public:
    mutable std::mutex mapMutex_;
    std::unordered_map<DRing::DataTransferId, std::shared_ptr<DataTransfer>> map_;

    std::shared_ptr<DataTransfer> createFileTransfer(const DRing::DataTransferInfo& info);
    std::shared_ptr<IncomingFileTransfer> createIncomingFileTransfer(const DRing::DataTransferInfo& info,
                                                                     std::atomic<std::size_t>& progress_storage);

    std::shared_ptr<DataTransfer> getTransfer(const DRing::DataTransferId& id);

    void cancel(DataTransfer& transfer);

    void onConnectionRequestReply(const DRing::DataTransferId& id, PeerConnection* connection);
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
DataTransferFacade::Impl::createFileTransfer(const DRing::DataTransferInfo& info)
{
    auto tid = generateUID();
    auto transfer = std::make_shared<FileTransfer>(tid, info);
    std::lock_guard<std::mutex> lk {mapMutex_};
    map_.emplace(tid, transfer);
    return transfer;
}

std::shared_ptr<IncomingFileTransfer>
DataTransferFacade::Impl::createIncomingFileTransfer(const DRing::DataTransferInfo& info,
                                                     std::atomic<std::size_t>& progress_storage)
{
    auto tid = generateUID();
    auto transfer = std::make_shared<IncomingFileTransfer>(tid, info, progress_storage);
    std::lock_guard<std::mutex> lk {mapMutex_};
    map_.emplace(tid, transfer);
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
    RING_WARN("facade created, pimpl @%p", pimpl_.get());
}

DataTransferFacade::~DataTransferFacade()
{
    RING_WARN("facade destroy, pimpl @%p", pimpl_.get());
};

std::vector<DRing::DataTransferId>
DataTransferFacade::list() const
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    return map_utils::extractKeys(pimpl_->map_);
}

DRing::DataTransferId
DataTransferFacade::sendFile(const std::string& account_id, const std::string& peer_uri,
                             const std::string& file_path, const std::string& display_name)
{
    auto account = Manager::instance().getAccount<RingAccount>(account_id);
    if (!account)
        throw std::invalid_argument("unknown account id");

    if (!fileutils::isFile(file_path))
        throw std::invalid_argument("invalid input file");

    DRing::DataTransferInfo info;
    info.accountId = account_id;
    info.peer = peer_uri;
    info.displayName = display_name;
    info.path = file_path;
    // remaining fields are overwritten

    auto transfer = pimpl_->createFileTransfer(info);
    auto tid = transfer->getId();
    // IMPLEMENTATION NOTE: requestPeerConnection() may call the given callback a multiple time.
    // This happen when multiple agents handle communications of the given peer for the given account.
    // Example: Ring account supports multi-devices, each can answer to the request.
    account->requestPeerConnection(
        peer_uri,
        [this, tid] (PeerConnection* connection) {
            pimpl_->onConnectionRequestReply(tid, connection);
        });

    return tid;
}

void
DataTransferFacade::acceptAsFile(const DRing::DataTransferId& id,
                                 const std::string& file_path,
                                 std::size_t offset)
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    const auto& iter = pimpl_->map_.find(id);
    if (iter == std::end(pimpl_->map_))
        throw std::invalid_argument("not existing DataTransferId");

    iter->second->accept(file_path, offset);
}

void
DataTransferFacade::cancel(const DRing::DataTransferId& id)
{
    if (auto transfer = pimpl_->getTransfer(id))
        pimpl_->cancel(*transfer);
    else
        throw std::invalid_argument("not existing DataTransferId");
}

std::streamsize
DataTransferFacade::bytesProgress(const DRing::DataTransferId& id) const
{
    if (auto transfer = pimpl_->getTransfer(id))
        return transfer->bytesProgress();
    throw std::invalid_argument("not existing DataTransferId");
}

DRing::DataTransferInfo
DataTransferFacade::info(const DRing::DataTransferId& id) const
{
    if (auto transfer = pimpl_->getTransfer(id))
        return transfer->info();
    throw std::invalid_argument("not existing DataTransferId");
}

std::string
DataTransferFacade::onIncomingFileRequest(const std::string& account_id,
                                          const std::string& peer_uri,
                                          const std::string& display_name,
                                          std::size_t total_size,
                                          std::size_t offset,
                                          std::atomic<std::size_t>& progress_storage)
{
    DRing::DataTransferInfo info;
    info.accountId = account_id;
    info.peer = peer_uri;
    info.displayName = display_name;
    info.totalSize = total_size;
    info.bytesProgress = offset;
    // remaining fields are overwritten

    auto transfer = pimpl_->createIncomingFileTransfer(info, progress_storage);
    auto filename = transfer->requestFilename();
    if (!filename.empty())
        transfer->start(); // TODO: bad place, call only if file can be open
    return filename;
}

} // namespace ring
