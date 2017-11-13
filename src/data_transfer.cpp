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

#include "data_transfer.h"

#include "manager.h"
#include "ringdht/ringaccount.h"
#include "peer_connection.h"
#include "fileutils.h"
#include "string_utils.h"
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

    virtual void start() = 0;

    virtual std::streamsize bytesSent() const {
        std::lock_guard<std::mutex> lk {infoMutex_};
        return info_.bytesProgress;
    }

    DRing::DataTransferInfo info() const {
        std::lock_guard<std::mutex> lk {infoMutex_};
        return info_;
    }

    void emit(DRing::DataTransferEventCode code) const;

    const DRing::DataTransferId id;

protected:
    mutable std::mutex infoMutex_;
    mutable DRing::DataTransferInfo info_;
};

void
DataTransfer::emit(DRing::DataTransferEventCode code) const
{
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        info_.lastEvent = code;
    }
    emitSignal<DRing::DataTransferSignal::DataTransferEvent>(id, int(code));
}

//==============================================================================

class FileTransfer final : public DataTransfer
{
public:
    FileTransfer(DRing::DataTransferId id, const std::string&, const std::string&);

    void start() override;

    void close() noexcept override;

    bool read(std::vector<char>&) const override;

private:
    FileTransfer() = delete;

    mutable std::ifstream input_;
    mutable std::size_t tx_ {0};
    mutable bool headerSent_ {false};
    const std::string peerUri_;
};

FileTransfer::FileTransfer(DRing::DataTransferId id,
                           const std::string& file_path,
                           const std::string& display_name)
    : DataTransfer(id)
{
    input_.open(file_path, std::ios::binary);
    if (!input_)
        throw std::runtime_error("input file open failed");

    info_.isOutgoing = true;
    info_.displayName = display_name;
    info_.path = file_path;

    // File size?
    input_.seekg(0, std::ios_base::end);
    info_.totalSize = input_.tellg();
    input_.seekg(0, std::ios_base::beg);

    emit(DRing::DataTransferEventCode::CREATED);
}

void
FileTransfer::start()
{
    emit(DRing::DataTransferEventCode::ONGOING);
}

void
FileTransfer::close() noexcept
{
    RING_DBG("closing");
    input_.close();
    if (info_.lastEvent < DRing::DataTransferEventCode::FINISHED)
        emit(DRing::DataTransferEventCode::CLOSED_BY_HOST);
}

bool
FileTransfer::read(std::vector<char>& buf) const
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

    input_.read(&buf[0], buf.size());
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
        emit(DRing::DataTransferEventCode::FINISHED);
        return false;
    } else {
        // TODO: handle errors
        RING_ERR("fstream.read() exception");
    }

    return true;
}

//==============================================================================

class IncomingFileTransfer final : public DataTransfer
{
public:
    IncomingFileTransfer(DRing::DataTransferId id, const std::string&, std::size_t);

    void start() override;

    void close() noexcept override;

    bool read(std::vector<char>&) const override;

    std::string requestFilename();

    void accept(const std::string&, std::size_t offset) override;

private:
    IncomingFileTransfer() = delete;

    std::promise<void> filenamePromise_;
};

IncomingFileTransfer::IncomingFileTransfer(DRing::DataTransferId id,
                                           const std::string& display_name,
                                           std::size_t offset)
    : DataTransfer(id)
{
    (void)offset;

    info_.isOutgoing = false;
    info_.displayName = display_name;
    // TODO: use offset?

    emit(DRing::DataTransferEventCode::CREATED);
}

std::string
IncomingFileTransfer::requestFilename()
{
    emit(DRing::DataTransferEventCode::WAIT_HOST_ACCEPTANCE);
    // Now wait for DataTransferFacade::acceptFileTransfer() call

#if 0
    filenamePromise_.get_future().wait();
    return info_.path;
#else
    // DEBUG
    char filename[] = "/tmp/ring_XXXXXX";
    if (::mkstemp(filename) < 0)
        throw std::system_error(errno, std::system_category());
    return filename;
#endif
}

void
IncomingFileTransfer::start()
{
    emit(DRing::DataTransferEventCode::ONGOING);
}

void
IncomingFileTransfer::close() noexcept
{}

void
IncomingFileTransfer::accept(const std::string& filename, std::size_t offset)
{
    // TODO: offset?
    (void)offset;

    info_.path = filename;
    filenamePromise_.set_value();
}

bool
IncomingFileTransfer::read(std::vector<char>&) const
{
    return true;
}

//==============================================================================

class DataTransferFacade::Impl
{
public:
    mutable std::mutex mapMutex_;
    std::unordered_map<DRing::DataTransferId, std::shared_ptr<DataTransfer>> map_;

    std::shared_ptr<DataTransfer> createFileTransfer(DRing::DataTransferId id,
                                                     const std::string& file_path,
                                                     const std::string& display_name);
    std::shared_ptr<IncomingFileTransfer> createIncomingFileTransfer(DRing::DataTransferId id,
                                                                     const std::string& display_name,
                                                                     std::size_t offset);
};

std::shared_ptr<DataTransfer>
DataTransferFacade::Impl::createFileTransfer(DRing::DataTransferId id,
                                             const std::string& file_path,
                                             const std::string& display_name)
{
    std::lock_guard<std::mutex> lk {mapMutex_};
    auto transfer = std::make_shared<FileTransfer>(id, file_path, display_name);
    map_.emplace(id, transfer);
    return transfer;
}

std::shared_ptr<IncomingFileTransfer>
DataTransferFacade::Impl::createIncomingFileTransfer(DRing::DataTransferId id,
                                                     const std::string& display_name,
                                                     std::size_t offset)
{
    std::lock_guard<std::mutex> lk {mapMutex_};
    auto transfer = std::make_shared<IncomingFileTransfer>(id, display_name, offset);
    map_.emplace(id, transfer);
    return transfer;
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

DRing::DataTransferId
DataTransferFacade::sendFile(const std::string& account_id, const std::string& peer_uri,
                             const std::string& file_path, const std::string& display_name)
{
    auto account = Manager::instance().getAccount<RingAccount>(account_id);
    if (!account)
        throw std::invalid_argument("unknown account id");

    if (!fileutils::isFile(file_path))
        throw std::invalid_argument("invalid input file");

    auto id = generateUID();
    auto transfer = pimpl_->createFileTransfer(id, file_path, display_name);

    // Helper structure to let the first responsive connection win the transfer
    struct X {
        std::atomic_bool win {false};
        std::shared_ptr<DataTransfer> transfer;
    };
    auto x = std::make_shared<X>();
    x->transfer = transfer;

    account->requestPeerConnection(
        peer_uri,
        [this, id, x] (PeerConnection* connection) {
            if (connection) {
                bool expected = false;
                if (x->win.compare_exchange_weak(expected, true)) {
                    x->transfer->start();
                    connection->attachInputStream(x->transfer);
                }
            } else {
                x->transfer->emit(DRing::DataTransferEventCode::UNJOINABLE_PEER);
                cancel(id);
            }
        });

    return id;
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
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    const auto& transfer_it = pimpl_->map_.find(id);
    if (transfer_it == std::end(pimpl_->map_))
        throw std::invalid_argument("not existing DataTransferId");

    transfer_it->second->close();
    pimpl_->map_.erase(transfer_it);
}

std::streamsize
DataTransferFacade::bytesSent(const DRing::DataTransferId& id) const
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    const auto& iter = pimpl_->map_.find(id);
    if (iter == std::end(pimpl_->map_))
        throw std::invalid_argument("not existing DataTransferId");

    return iter->second->bytesSent();;
}

DRing::DataTransferInfo
DataTransferFacade::info(const DRing::DataTransferId& id) const
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    const auto& iter = pimpl_->map_.find(id);
    if (iter == std::end(pimpl_->map_))
        throw std::invalid_argument("not existing DataTransferId");

    return iter->second->info();
}

std::string
DataTransferFacade::onIncomingFileRequest(const std::string& display_name, std::size_t offset)
{
    auto transfer = pimpl_->createIncomingFileTransfer(generateUID(), display_name, offset);
    return transfer->requestFilename();
}

} // namespace ring
