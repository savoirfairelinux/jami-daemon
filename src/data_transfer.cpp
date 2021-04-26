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

#include "data_transfer.h"

#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "peer_connection.h"
#include "fileutils.h"
#include "string_utils.h"
#include "map_utils.h"
#include "client/ring_signal.h"

#include <thread>
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

#include <opendht/rng.h>
#include <opendht/thread_pool.h>

namespace jami {

DRing::DataTransferId
generateUID()
{
    thread_local dht::crypto::random_device rd;
    return std::uniform_int_distribution<DRing::DataTransferId> {1, DRING_ID_MAX_VAL}(rd);
}

class TransferManager::Impl
{
public:
    Impl(const std::string& accountId, const std::string& to, bool isConversation)
        : accountId_(accountId)
        , to_(to)
        , isConversation_(isConversation)
    {
        waitingPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId_
                       + DIR_SEPARATOR_STR + "conversation_data" + DIR_SEPARATOR_STR + to_
                       + DIR_SEPARATOR_STR + "waiting";
        loadWaiting();
    }

    void loadWaiting()
    {
        try {
            // read file
            auto file = fileutils::loadFile(waitingPath_);
            // load values
            msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
            std::lock_guard<std::mutex> lk {mapMutex_};
            oh.get().convert(waitingIds_);
        } catch (const std::exception& e) {
            return;
        }
    }
    void saveWaiting()
    {
        std::ofstream file(waitingPath_, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, waitingIds_);
    }

    std::string accountId_ {};
    std::string to_ {};
    bool isConversation_ {true};
    std::string waitingPath_ {};

    std::mutex mapMutex_ {};
    std::map<DRing::DataTransferId, WaitingRequest> waitingIds_ {};
    struct IncomingFile
    {
        std::shared_ptr<ChannelSocket> channel;
        std::shared_ptr<std::ofstream> stream;
    };
    std::map<DRing::DataTransferId, IncomingFile> incomingChannels_ {};
};

TransferManager::TransferManager(const std::string& accountId,
                                 const std::string& to,
                                 bool isConversation)
    : pimpl_ {std::make_unique<Impl>(accountId, to, isConversation)}
{}

TransferManager::~TransferManager() {}

bool
TransferManager::cancel(const DRing::DataTransferId& id)
{
    std::shared_ptr<ChannelSocket> channel;
    std::unique_lock<std::mutex> lk {pimpl_->mapMutex_};
    if (pimpl_->isConversation_) {
        auto itC = pimpl_->incomingChannels_.find(id);
        if (itC == pimpl_->incomingChannels_.end())
            return false;
        channel = itC->second.channel;
        lk.unlock();
        if (channel)
            channel->shutdown();
        return true;
    }
    return false;
}

void
TransferManager::waitForTransfer(const DRing::DataTransferId& id,
                                 const std::string& interactionId,
                                 const std::string& sha3sum,
                                 const std::string& path)
{
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    auto itW = pimpl_->waitingIds_.find(id);
    if (itW != pimpl_->waitingIds_.end())
        return;
    pimpl_->waitingIds_[id] = {interactionId, sha3sum, path};
    if (pimpl_->isConversation_)
        pimpl_->saveWaiting();
}

bool
TransferManager::acceptIncomingChannel(const DRing::DataTransferId& id) const
{
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    auto itW = pimpl_->waitingIds_.find(id);
    if (itW == pimpl_->waitingIds_.end())
        return false;
    auto itC = pimpl_->incomingChannels_.find(id);
    return itC == pimpl_->incomingChannels_.end();
}

void
TransferManager::handleChannel(const DRing::DataTransferId& id,
                               const std::shared_ptr<ChannelSocket>& channel)
{
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    auto itC = pimpl_->incomingChannels_.find(id);
    if (itC != pimpl_->incomingChannels_.end()) {
        channel->shutdown();
        return;
    }
    auto itW = pimpl_->waitingIds_.find(id);
    if (itW == pimpl_->waitingIds_.end()) {
        channel->shutdown();
        return;
    }
    auto path = itW->second.path;
    auto wantedSha3 = itW->second.sha3sum;
    TransferManager::Impl::IncomingFile ifile;
    ifile.stream = std::make_shared<std::ofstream>();
    ifile.channel = channel;
    fileutils::openStream(*ifile.stream, path);
    if (!ifile.stream) {
        channel->shutdown();
        return;
    }

    // TODO send info to client
    channel->setOnRecv(
        [wFile = std::weak_ptr<std::ofstream>(ifile.stream)](const uint8_t* buf, size_t len) {
            if (auto file = wFile.lock())
                if (file->is_open())
                    *file << std::string_view((const char*) buf, len);
            return len;
        });
    channel->onShutdown([this, id]() {
        // TODO move in cb like before
        std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
        auto itC = pimpl_->incomingChannels_.find(id);
        if (itC == pimpl_->incomingChannels_.end())
            return;
        if (itC->second.stream && itC->second.stream->is_open())
            itC->second.stream->close();
        auto it = pimpl_->waitingIds_.find(id);
        bool correct = false;
        if (it != pimpl_->waitingIds_.end()) {
            auto sha3sum = fileutils::sha3File(it->second.path);
            if (it->second.sha3sum == sha3sum) {
                JAMI_INFO() << "New file received: " << it->second.path;
                correct = true;
                pimpl_->waitingIds_.erase(it);
                pimpl_->saveWaiting();
            } else {
                JAMI_WARN() << "Remove file, invalid sha3sum detected for " << it->second.path;
                fileutils::remove(it->second.path, true);
            }
        }
        // TODO closed by host if cancelled.
        auto code = correct ? DRing::DataTransferEventCode::finished
                            : DRing::DataTransferEventCode::closed_by_peer;
        emitSignal<DRing::DataTransferSignal::DataTransferEvent>(pimpl_->accountId_,
                                                                 pimpl_->to_,
                                                                 id,
                                                                 uint32_t(code));
        pimpl_->incomingChannels_.erase(itC);
    });

    pimpl_->incomingChannels_.emplace(id, std::move(ifile));
    emitSignal<DRing::DataTransferSignal::DataTransferEvent>(
        pimpl_->accountId_, pimpl_->to_, id, uint32_t(DRing::DataTransferEventCode::ongoing));
}

std::vector<WaitingRequest>
TransferManager::waitingRequests() const
{
    std::vector<WaitingRequest> res;
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    for (const auto& [id, req] : pimpl_->waitingIds_) {
        auto itC = pimpl_->incomingChannels_.find(id);
        if (itC == pimpl_->incomingChannels_.end())
            res.emplace_back(req);
    }
    return res;
}

} // namespace jami
