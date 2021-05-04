/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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
#include "fileutils.h"
#include "string_utils.h"
#include "map_utils.h"
#include "client/ring_signal.h"

#include <thread>
#include <stdexcept>
#include <mutex>
#include <future>
#include <charconv> // std::from_chars
#include <cstdlib>  // mkstemp

#include <opendht/rng.h>
#include <opendht/thread_pool.h>

namespace jami {

DRing::DataTransferId
generateUID()
{
    thread_local dht::crypto::random_device rd;
    return std::uniform_int_distribution<DRing::DataTransferId> {1, DRING_ID_MAX_VAL}(rd);
}

FileInfo::FileInfo(const std::shared_ptr<ChannelSocket>& channel,
                   DRing::DataTransferId tid,
                   const DRing::DataTransferInfo& info)
    : tid_(tid)
    , info_(info)
    , channel_(channel)
{}

void
FileInfo::emit(DRing::DataTransferEventCode code)
{
    if (tid_ != 0) {
        // Else it's an internal transfer
        emitSignal<DRing::DataTransferSignal::DataTransferEvent>(info_.accountId,
                                                                 info_.conversationId,
                                                                 tid_,
                                                                 uint32_t(code));
    }
    if (finishedCb_ && code >= DRing::DataTransferEventCode::finished)
        finishedCb_(uint32_t(code));
}

OutgoingFile::OutgoingFile(const std::shared_ptr<ChannelSocket>& channel,
                           DRing::DataTransferId tid,
                           const DRing::DataTransferInfo& info,
                           size_t start,
                           size_t end)
    : FileInfo(channel, tid, info)
    , start_(start)
    , end_(end)
{
    if (!fileutils::isFile(info_.path)) {
        channel_->shutdown();
        return;
    }
    fileutils::openStream(stream_, info_.path);
    if (!stream_) {
        channel_->shutdown();
        return;
    }
    stream_.seekg(start, std::ios::beg);
}

OutgoingFile::~OutgoingFile()
{
    if (stream_ && stream_.is_open())
        stream_.close();
    if (channel_)
        channel_->shutdown();
}

void
OutgoingFile::process()
{
    if (!channel_ or !stream_)
        return;
    auto correct = false;
    try {
        std::vector<char> buffer(UINT16_MAX, 0);
        std::error_code ec;
        auto pos = start_;
        while (!stream_.eof()) {
            stream_.read(buffer.data(),
                         end_ > start_ ? std::min(end_ - pos, buffer.size()) : buffer.size());
            auto gcount = stream_.gcount();
            pos += gcount;
            channel_->write(reinterpret_cast<const uint8_t*>(buffer.data()), gcount, ec);
            if (ec)
                break;
        }
        if (!ec)
            correct = true;
        stream_.close();
    } catch (...) {
    }
    if (!isUserCancelled_) {
        auto code = correct ? DRing::DataTransferEventCode::finished
                            : DRing::DataTransferEventCode::closed_by_peer;
        emit(code);
    }
}

void
OutgoingFile::cancel()
{
    // Remove link, not original file
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + info_.accountId + DIR_SEPARATOR_STR
                + "conversation_data" + DIR_SEPARATOR_STR + info_.conversationId + DIR_SEPARATOR_STR
                + std::to_string(tid_);
    if (fileutils::isSymLink(path))
        fileutils::remove(path);
    isUserCancelled_ = true;
    emit(DRing::DataTransferEventCode::closed_by_host);
}

IncomingFile::IncomingFile(const std::shared_ptr<ChannelSocket>& channel,
                           const DRing::DataTransferInfo& info,
                           DRing::DataTransferId tid,
                           const std::string& sha3Sum)
    : FileInfo(channel, tid, info)
    , sha3Sum_(sha3Sum)
{
    fileutils::openStream(stream_, info_.path);
    if (!stream_)
        return;

    emit(DRing::DataTransferEventCode::ongoing);
}

IncomingFile::~IncomingFile()
{
    if (channel_)
        channel_->setOnRecv({});
    if (stream_ && stream_.is_open())
        stream_.close();
    if (channel_)
        channel_->shutdown();
}

void
IncomingFile::cancel()
{
    isUserCancelled_ = true;
    emit(DRing::DataTransferEventCode::closed_by_peer);
    if (channel_)
        channel_->shutdown();
}

void
IncomingFile::process()
{
    channel_->setOnRecv([this](const uint8_t* buf, size_t len) {
        if (stream_.is_open())
            stream_ << std::string_view((const char*) buf, len);
        info_.bytesProgress = stream_.tellp();
        return len;
    });
    channel_->onShutdown([this] {
        auto correct = sha3Sum_.empty();
        if (!correct) {
            // Verify shaSum
            auto sha3Sum = fileutils::sha3File(info_.path);
            if (sha3Sum_ == sha3Sum) {
                JAMI_INFO() << "New file received: " << info_.path;
                correct = true;
            } else {
                JAMI_WARN() << "Remove file, invalid sha3sum detected for " << info_.path;
                fileutils::remove(info_.path, true);
            }
        }
        if (isUserCancelled_)
            return;
        auto code = correct ? DRing::DataTransferEventCode::finished
                            : DRing::DataTransferEventCode::closed_by_host;
        emit(code);
    });
}

//==============================================================================

class TransferManager::Impl
{
public:
    Impl(const std::string& accountId, const std::string& to)
        : accountId_(accountId)
        , to_(to)
    {
        waitingPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId_
                       + DIR_SEPARATOR_STR + "conversation_data" + DIR_SEPARATOR_STR + to_
                       + DIR_SEPARATOR_STR + "waiting";
        loadWaiting();
    }

    ~Impl()
    {
        std::lock_guard<std::mutex> lk {mapMutex_};
        for (const auto& [channel, _of] : outgoings_) {
            channel->shutdown();
        }
        outgoings_.clear();
        incomings_.clear();
        vcards_.clear();
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
    std::string waitingPath_ {};

    std::mutex mapMutex_ {};
    std::map<DRing::DataTransferId, WaitingRequest> waitingIds_ {};
    std::map<std::shared_ptr<ChannelSocket>, std::shared_ptr<OutgoingFile>> outgoings_ {};
    std::map<DRing::DataTransferId, std::shared_ptr<IncomingFile>> incomings_ {};
    std::map<std::string, std::shared_ptr<IncomingFile>> vcards_ {};
};

TransferManager::TransferManager(const std::string& accountId, const std::string& to)
    : pimpl_ {std::make_unique<Impl>(accountId, to)}
{}

TransferManager::~TransferManager() {}

void
TransferManager::transferFile(const std::shared_ptr<ChannelSocket>& channel,
                              const std::string& tid,
                              const std::string& path,
                              size_t start,
                              size_t end)
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    if (pimpl_->outgoings_.find(channel) != pimpl_->outgoings_.end())
        return;
    DRing::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;
    info.path = path;
    DRing::DataTransferId tid_value = 0;
    if (tid != "profile.vcf") {
        std::from_chars(tid.data(), tid.data() + tid.size(), tid_value);
    }
    auto f = std::make_shared<OutgoingFile>(channel, tid_value, info, start, end);
    f->onFinished([w = weak(), channel](uint32_t) {
        // schedule destroy outgoing transfer as not needed
        dht::ThreadPool().computation().run([w, channel] {
            if (auto sthis_ = w.lock()) {
                auto& pimpl = sthis_->pimpl_;
                std::lock_guard<std::mutex> lk {pimpl->mapMutex_};
                auto itO = pimpl->outgoings_.find(channel);
                if (itO != pimpl->outgoings_.end())
                    pimpl->outgoings_.erase(itO);
            }
        });
    });
    pimpl_->outgoings_.emplace(channel, f);
    dht::ThreadPool::io().run([w = std::weak_ptr<OutgoingFile>(f)] {
        if (auto of = w.lock())
            of->process();
    });
}

bool
TransferManager::cancel(const DRing::DataTransferId& id)
{
    std::shared_ptr<ChannelSocket> channel;
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    // Note: For now, there is no cancel for outgoings.
    // The client can just remove the file.
    auto itC = pimpl_->incomings_.find(id);
    if (itC == pimpl_->incomings_.end())
        return false;
    itC->second->cancel();
    return true;
}

bool
TransferManager::info(const DRing::DataTransferId& id, DRing::DataTransferInfo& info) const noexcept
{
    std::unique_lock<std::mutex> lk {pimpl_->mapMutex_};
    // Check current state
    auto itI = pimpl_->incomings_.find(id);
    auto itW = pimpl_->waitingIds_.find(id);
    if (itI != pimpl_->incomings_.end()) {
        info = itI->second->info();
    } else if (itW != pimpl_->waitingIds_.find(id)) {
        info.lastEvent = DRing::DataTransferEventCode::ongoing;
    } else {
        auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + pimpl_->accountId_
                    + DIR_SEPARATOR_STR + "conversation_data" + DIR_SEPARATOR_STR + pimpl_->to_
                    + DIR_SEPARATOR_STR + std::to_string(id);
        if (fileutils::isFile(path))
            info.lastEvent = DRing::DataTransferEventCode::finished;
        else
            info.lastEvent = DRing::DataTransferEventCode::wait_host_acceptance;
    }
    lk.unlock();
    return true;
}

bool
TransferManager::bytesProgress(const DRing::DataTransferId& id,
                               int64_t& total,
                               int64_t& progress) const noexcept
{
    std::unique_lock<std::mutex> lk {pimpl_->mapMutex_};
    auto itI = pimpl_->incomings_.find(id);
    auto itW = pimpl_->waitingIds_.find(id);
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + pimpl_->accountId_
                + DIR_SEPARATOR_STR + "conversation_data" + DIR_SEPARATOR_STR + pimpl_->to_
                + DIR_SEPARATOR_STR + std::to_string(id);
    if (itI != pimpl_->incomings_.end()) {
        progress = itI->second->info().totalSize;
        progress = itI->second->info().bytesProgress;
        return true;
    } else if (fileutils::isFile(path)) {
        std::ifstream transfer(path, std::ios::binary);
        transfer.seekg(0, std::ios::end);
        progress = transfer.tellg();
        if (itW != pimpl_->waitingIds_.end()) {
            total = itW->second.totalSize;
        } else {
            // If not waiting it's finished
            total = progress;
        }
        return true;
    } else if (itW != pimpl_->waitingIds_.end()) {
        total = itW->second.totalSize;
        progress = 0;
        return true;
    }
    // No incoming/outgoing file
    return false;
}

void
TransferManager::waitForTransfer(const DRing::DataTransferId& id,
                                 const std::string& interactionId,
                                 const std::string& sha3sum,
                                 const std::string& path,
                                 std::size_t total)
{
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    auto itW = pimpl_->waitingIds_.find(id);
    if (itW != pimpl_->waitingIds_.end())
        return;
    pimpl_->waitingIds_[id] = {interactionId, sha3sum, path, total};
    pimpl_->saveWaiting();
}

bool
TransferManager::onFileChannelRequest(const DRing::DataTransferId& id) const
{
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    auto itW = pimpl_->waitingIds_.find(id);
    if (itW == pimpl_->waitingIds_.end())
        return false;
    auto itC = pimpl_->incomings_.find(id);
    return itC == pimpl_->incomings_.end();
}

void
TransferManager::onIncomingFileTransfer(const DRing::DataTransferId& id,
                                        const std::shared_ptr<ChannelSocket>& channel)
{
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    // Check if not already an incoming file for this id and that we are waiting this file
    auto itC = pimpl_->incomings_.find(id);
    if (itC != pimpl_->incomings_.end()) {
        channel->shutdown();
        return;
    }
    auto itW = pimpl_->waitingIds_.find(id);
    if (itW == pimpl_->waitingIds_.end()) {
        channel->shutdown();
        return;
    }

    DRing::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;
    info.path = itW->second.path;
    info.totalSize = itW->second.totalSize;

    // Create symlink for future transfers
    auto symlinkPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + info.accountId
                       + DIR_SEPARATOR_STR + "conversation_data" + DIR_SEPARATOR_STR
                       + info.conversationId + DIR_SEPARATOR_STR + std::to_string(id);
    if (info.path != symlinkPath && !fileutils::isSymLink(symlinkPath)) {
        fileutils::createSymLink(symlinkPath, info.path);
    }

    auto ifile = std::make_shared<IncomingFile>(std::move(channel), info, id, itW->second.sha3sum);
    auto res = pimpl_->incomings_.emplace(id, std::move(ifile));
    if (res.second) {
        res.first->second->onFinished([w = weak(), id](uint32_t code) {
            // schedule destroy transfer as not needed
            dht::ThreadPool().computation().run([w, id, code] {
                if (auto sthis_ = w.lock()) {
                    auto& pimpl = sthis_->pimpl_;
                    std::lock_guard<std::mutex> lk {pimpl->mapMutex_};
                    auto itO = pimpl->incomings_.find(id);
                    if (itO != pimpl->incomings_.end())
                        pimpl->incomings_.erase(itO);
                    if (code == uint32_t(DRing::DataTransferEventCode::finished)) {
                        auto itW = pimpl->waitingIds_.find(id);
                        if (itW != pimpl->waitingIds_.end()) {
                            pimpl->waitingIds_.erase(itW);
                            pimpl->saveWaiting();
                        }
                    }
                }
            });
        });
        res.first->second->process();
    }
}

void
TransferManager::onIncomingProfile(const std::shared_ptr<ChannelSocket>& channel)
{
    if (!channel)
        return;
    auto deviceId = channel->deviceId().toString();
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    // Check if not already an incoming file for this id and that we are waiting this file
    auto itV = pimpl_->vcards_.find(deviceId);
    if (itV != pimpl_->vcards_.end()) {
        channel->shutdown();
        return;
    }

    DRing::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;
    info.path = fileutils::get_cache_dir() + DIR_SEPARATOR_STR + pimpl_->accountId_
                + DIR_SEPARATOR_STR + "vcard" + DIR_SEPARATOR_STR + deviceId;
    ;

    auto ifile = std::make_shared<IncomingFile>(std::move(channel), info);
    auto res = pimpl_->vcards_.emplace(deviceId, std::move(ifile));
    if (res.second) {
        res.first->second->onFinished([w = weak(),
                                       deviceId = std::move(deviceId),
                                       accountId = pimpl_->accountId_,
                                       path = info.path](uint32_t code) {
            // schedule destroy transfer as not needed
            dht::ThreadPool().computation().run([w,
                                                 deviceId = std::move(deviceId),
                                                 accountId = std::move(accountId),
                                                 path = std::move(path),
                                                 code] {
                if (auto sthis_ = w.lock()) {
                    auto& pimpl = sthis_->pimpl_;
                    std::lock_guard<std::mutex> lk {pimpl->mapMutex_};
                    auto itO = pimpl->vcards_.find(deviceId);
                    if (itO != pimpl->vcards_.end())
                        pimpl->vcards_.erase(itO);
                    if (code == uint32_t(DRing::DataTransferEventCode::finished)) {
                        auto cert = tls::CertificateStore::instance().getCertificate(deviceId);
                        if (!cert)
                            return emitSignal<DRing::ConfigurationSignal::ProfileReceived>(
                                accountId, cert->getIssuerUID(), path);
                    }
                }
            });
        });
        res.first->second->process();
    }
}

std::vector<WaitingRequest>
TransferManager::waitingRequests() const
{
    std::vector<WaitingRequest> res;
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    for (const auto& [id, req] : pimpl_->waitingIds_) {
        auto itC = pimpl_->incomings_.find(id);
        if (itC == pimpl_->incomings_.end())
            res.emplace_back(req);
    }
    return res;
}

} // namespace jami
