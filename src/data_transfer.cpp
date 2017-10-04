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

#include "peer_connection.h"
#include "manager.h"
#include "ringdht/ringaccount.h"

#include <stdexcept>
#include <fstream>
#include <ios>
#include <unordered_map>
#include <mutex>
#include <iostream>

namespace ring {

class DataTransfer
{
public:
    DataTransfer() {};
    DataTransfer(DataTransfer&&) = default;
    virtual ~DataTransfer() = default;

    virtual void start() {}

    virtual void connected(PeerConnection&&) {}

    virtual void accept(std::ostream&&) {}

    virtual void cancel() {}

    virtual std::streamsize bytesSent() const { return 0; }

    DRing::DataTransferInfo info() const {
        return info_;
    }

private:
    DRing::DataTransferInfo info_;
    //PeerConnection connection_;
};

//==============================================================================

class FileTransfer : public DataTransfer
{
public:
    FileTransfer(const std::string& peerUri,
                 const std::string& pathname,
                 const std::string& displayName);

private:
    FileTransfer() = delete;
};


FileTransfer::FileTransfer(const std::string& peerUri,
                           const std::string& pathname,
                           const std::string& displayName)
{
    (void)peerUri;
    (void)pathname;
    (void)displayName;
}

//connection_->newInputStream(std::make_unique<std::stringstream>(std::string(100000, 'X')));

//==============================================================================

class DataTransferFacade::Impl
{
public:
    mutable std::mutex mapMutex_;
    std::unordered_map<DRing::DataTransferId, std::unique_ptr<DataTransfer>> map_;

    DRing::DataTransferId createFileTransfer(const std::string& peerUri,
                                             const std::string& pathname,
                                             const std::string& displayName);
};

static DRing::DataTransferId
generateUID()
{
    static DRing::DataTransferId lastId = 0;
    return lastId++;
}

DRing::DataTransferId
DataTransferFacade::Impl::createFileTransfer(const std::string& peerUri,
                                             const std::string& pathname,
                                             const std::string& displayName)
{
    std::lock_guard<std::mutex> lk {mapMutex_};
    const auto& result = map_.emplace(generateUID(), std::make_unique<FileTransfer>(peerUri, pathname, displayName));
    return result.first->first;
}

//==============================================================================

DataTransferFacade::DataTransferFacade() : pimpl_ {std::make_unique<Impl>()} {}

DataTransferFacade::~DataTransferFacade() = default;

DRing::DataTransferId
DataTransferFacade::sendFile(const std::string& accountId, const std::string& peerUri,
                             const std::string& pathname, const std::string& displayName)
{
    auto account = Manager::instance().getAccount<RingAccount>(accountId);
    if (!account)
        throw std::invalid_argument("unknown account id");

    auto id = pimpl_->createFileTransfer(peerUri, pathname, displayName);
    account->requestPeerConnection(
        peerUri,
        [this, id] (PeerConnection&& connection) {
            std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
            const auto& iter = pimpl_->map_.find(id);
            if (iter != std::end(pimpl_->map_))
                iter->second->connected(std::move(connection));
        },
        [this, id] { cancel(id); });
    return id;
}

void
DataTransferFacade::acceptAsFile(const DRing::DataTransferId& id, const std::string& pathname)
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    const auto& iter = pimpl_->map_.find(id);
    if (iter == std::end(pimpl_->map_))
        throw std::invalid_argument("not existing DataTransferId");

    std::fstream output {pathname, std::ios::binary | std::ios::out};
    iter->second->accept(std::move(output));
}

void
DataTransferFacade::cancel(const DRing::DataTransferId& id)
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    const auto& iter = pimpl_->map_.find(id);
    if (iter == std::end(pimpl_->map_))
        throw std::invalid_argument("not existing DataTransferId");

    iter->second->cancel();
    pimpl_->map_.erase(iter);
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
