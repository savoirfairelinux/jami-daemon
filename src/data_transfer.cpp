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

#include <stdexcept>
#include <fstream>
#include <ios>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <future>
#include <atomic>

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

    virtual void start() = 0;

    virtual void connected() = 0;

    virtual void accept(std::ostream&&) = 0;

    virtual std::streamsize bytesSent() const {
        std::lock_guard<std::mutex> lk {infoMutex_};
        return info_.bytesProgress;
    }

    DRing::DataTransferInfo info() const {
        std::lock_guard<std::mutex> lk {infoMutex_};
        return info_;
    }

    DRing::DataTransferId getId() const override {
        return id;
    }

    const DRing::DataTransferId id;

protected:
    mutable std::mutex infoMutex_;
    mutable DRing::DataTransferInfo info_;
};

//==============================================================================

class FileTransfer final : public DataTransfer
{
public:
    FileTransfer(DRing::DataTransferId id, const std::string&, const std::string&);

    void start() override;

    void close() noexcept override;

    void connected() override;

    void accept(std::ostream&&) override;

    bool read(std::vector<char>&) const override;

    DRing::DataTransferId getId() const override {
        return id;
    }

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
    info_.displayName = display_name;
}

void
FileTransfer::start()
{}

void
FileTransfer::close() noexcept
{
    RING_DBG() << "Sent " << info_.bytesProgress << " bytes";
    input_.close();
}

void
FileTransfer::connected()
{}

void
FileTransfer::accept(std::ostream&&)
{}

bool
FileTransfer::read(std::vector<char>& buf) const
{
    bool res = input_.good();
    if (headerSent_) {
        if (res) {
            try {
                input_.read(&buf[0], buf.size());
            } catch (...) {
                // TODO: handle errors, now support it's always like an EOF
                RING_ERR("fstream.read() exception");
                return false;
            }
            if (auto n = input_.gcount()) {
                buf.resize(n);
                {
                    std::lock_guard<std::mutex> lk {infoMutex_};
                    info_.bytesProgress += n;
                }
            } else
                res = false;
        }
    } else {
        input_.seekg(0, std::ios_base::end);
        auto size = to_string(input_.tellg()) + '\n';
        input_.seekg(0, std::ios_base::beg);
        buf.insert(std::begin(buf), std::begin(size), std::end(size));
        headerSent_ = true;
    }

    return res;
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
                if (x->win.compare_exchange_weak(expected, true))
                    connection->attachInputStream(x->transfer);
            }
        });

    return id;
}

void
DataTransferFacade::acceptAsFile(const DRing::DataTransferId& id,
                                 const std::string& file_path,
                                 std::size_t offset)
{
    (void)id;
    (void)file_path;
    (void)offset;
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
    // TODO: ring client signaling here
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

} // namespace ring
