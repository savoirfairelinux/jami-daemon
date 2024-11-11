/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "data_transfer.h"

#include "base64.h"
#include "fileutils.h"
#include "manager.h"
#include "client/ring_signal.h"

#include <mutex>
#include <cstdlib> // mkstemp
#include <filesystem>

#include <opendht/rng.h>
#include <opendht/thread_pool.h>

namespace jami {

libjami::DataTransferId
generateUID(std::mt19937_64& engine)
{
    return std::uniform_int_distribution<libjami::DataTransferId> {1, JAMI_ID_MAX_VAL}(engine);
}

FileInfo::FileInfo(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                   const std::string& fileId,
                   const std::string& interactionId,
                   const libjami::DataTransferInfo& info)
    : fileId_(fileId)
    , interactionId_(interactionId)
    , info_(info)
    , channel_(channel)
{}

void
FileInfo::emit(libjami::DataTransferEventCode code)
{
    if (finishedCb_ && code >= libjami::DataTransferEventCode::finished)
        finishedCb_(uint32_t(code));
    if (interactionId_ != "") {
        // Else it's an internal transfer
        runOnMainThread([info = info_, iid = interactionId_, fid = fileId_, code]() {
            emitSignal<libjami::DataTransferSignal::DataTransferEvent>(info.accountId,
                                                                       info.conversationId,
                                                                       iid,
                                                                       fid,
                                                                       uint32_t(code));
        });
    }
}

OutgoingFile::OutgoingFile(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                           const std::string& fileId,
                           const std::string& interactionId,
                           const libjami::DataTransferInfo& info,
                           size_t start,
                           size_t end)
    : FileInfo(channel, fileId, interactionId, info)
    , start_(start)
    , end_(end)
{
    std::filesystem::path fpath(info_.path);
    if (!std::filesystem::is_regular_file(fpath)) {
        channel_->shutdown();
        return;
    }
    stream_.open(fpath, std::ios::binary | std::ios::in);
    if (!stream_ || !stream_.is_open()) {
        channel_->shutdown();
        return;
    }
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
    if (!channel_ or !stream_ or !stream_.is_open())
        return;
    auto correct = false;
    stream_.seekg(start_, std::ios::beg);
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
        // NOTE: emit(code) MUST be changed to improve handling of multiple destinations
        // But for now, we can just avoid to emit errors to the client, because for outgoing
        // transfer in a swarm, for outgoingFiles, we know that the file is ok. And the peer
        // will retry the transfer if they need, so we don't need to show errors.
        if (!interactionId_.empty() && !correct)
            return;
        auto code = correct ? libjami::DataTransferEventCode::finished
                            : libjami::DataTransferEventCode::closed_by_peer;
        emit(code);
    }
}

void
OutgoingFile::cancel()
{
    // Remove link, not original file
    auto path = fileutils::get_data_dir() / "conversation_data" / info_.accountId
                / info_.conversationId / fileId_;
    if (std::filesystem::is_symlink(path))
        dhtnet::fileutils::remove(path);
    isUserCancelled_ = true;
    emit(libjami::DataTransferEventCode::closed_by_host);
}

IncomingFile::IncomingFile(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                           const libjami::DataTransferInfo& info,
                           const std::string& fileId,
                           const std::string& interactionId,
                           const std::string& sha3Sum)
    : FileInfo(channel, fileId, interactionId, info)
    , sha3Sum_(sha3Sum)
    , path_(info.path + ".tmp")
{
    stream_.open(path_,
                 std::ios::binary | std::ios::out | std::ios::app);
    if (!stream_)
        return;

    emit(libjami::DataTransferEventCode::ongoing);
}

IncomingFile::~IncomingFile()
{
    if (channel_)
        channel_->setOnRecv({});
    {
        std::lock_guard<std::mutex> lk(streamMtx_);
        if (stream_ && stream_.is_open())
            stream_.close();
    }
    if (channel_)
        channel_->shutdown();
}

void
IncomingFile::cancel()
{
    isUserCancelled_ = true;
    emit(libjami::DataTransferEventCode::closed_by_peer);
    if (channel_)
        channel_->shutdown();
}

void
IncomingFile::process()
{
    channel_->setOnRecv([w = weak_from_this()](const uint8_t* buf, size_t len) {
        if (auto shared = w.lock()) {
            // No need to lock, setOnRecv is resetted before closing
            if (shared->stream_.is_open())
                shared->stream_.write(reinterpret_cast<const char*>(buf), len);
            shared->info_.bytesProgress = shared->stream_.tellp();
        }
        return len;
    });
    channel_->onShutdown([w = weak_from_this()] {
        auto shared = w.lock();
        if (!shared)
            return;
        {
            std::lock_guard<std::mutex> lk(shared->streamMtx_);
            if (shared->stream_ && shared->stream_.is_open())
                shared->stream_.close();
        }
        auto correct = shared->sha3Sum_.empty();
        std::error_code ec;
        if (!correct) {
            if (shared->isUserCancelled_) {
                std::filesystem::remove(shared->path_, ec);
            } else {
                auto sha3Sum = fileutils::sha3File(shared->path_);
                if (shared->sha3Sum_ == sha3Sum) {
                    JAMI_LOG("New file received: {}", shared->info_.path);
                    correct = true;
                } else {
                    if (shared->info_.totalSize != 0
                        && shared->info_.totalSize < shared->info_.bytesProgress) {
                        JAMI_WARNING("Removing {} larger than announced: {}/{}", shared->info_.path, shared->info_.bytesProgress, shared->info_.totalSize);
                        std::filesystem::remove(shared->path_, ec);
                    } else {
                        JAMI_WARNING("Invalid sha3sum detected for {}, incomplete file: {}/{}", shared->info_.path, shared->info_.bytesProgress, shared->info_.totalSize);
                    }
                }
            }
            if (ec) {
                JAMI_ERROR("Failed to remove file {}: {}", shared->path_, ec.message());
            }
        }
        if (correct) {
            std::filesystem::rename(shared->path_, shared->info_.path, ec);
            if (ec) {
                JAMI_ERROR("Failed to rename file from {} to {}: {}", shared->path_, shared->info_.path, ec.message());
                correct = false;
            }
        }
        if (shared->isUserCancelled_)
            return;
        auto code = correct ? libjami::DataTransferEventCode::finished
                            : libjami::DataTransferEventCode::closed_by_host;
        shared->emit(code);
    });
}

//==============================================================================

class TransferManager::Impl
{
public:
    Impl(const std::string& accountId,
         const std::string& accountUri,
         const std::string& to,
         const std::mt19937_64& rand)
        : accountId_(accountId)
        , accountUri_(accountUri)
        , to_(to)
        , rand_(rand)
    {
        if (!to_.empty()) {
            conversationDataPath_ = fileutils::get_data_dir() / accountId_ / "conversation_data"
                                    / to_;
            dhtnet::fileutils::check_dir(conversationDataPath_);
            waitingPath_ = conversationDataPath_ / "waiting";
        }
        profilesPath_ = fileutils::get_data_dir() / accountId_ / "profiles";
        accountProfilePath_ = fileutils::get_data_dir() / accountId / "profile.vcf";
        loadWaiting();
    }

    ~Impl()
    {
        std::lock_guard lk {mapMutex_};
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
            std::lock_guard lk {mapMutex_};
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
    std::string accountUri_ {};
    std::string to_ {};
    std::filesystem::path waitingPath_ {};
    std::filesystem::path profilesPath_ {};
    std::filesystem::path accountProfilePath_ {};
    std::filesystem::path conversationDataPath_ {};

    std::mutex mapMutex_ {};
    std::map<std::string, WaitingRequest> waitingIds_ {};
    std::map<std::shared_ptr<dhtnet::ChannelSocket>, std::shared_ptr<OutgoingFile>> outgoings_ {};
    std::map<std::string, std::shared_ptr<IncomingFile>> incomings_ {};
    std::map<std::pair<std::string, std::string>, std::shared_ptr<IncomingFile>> vcards_ {};

    std::mt19937_64 rand_;
};

TransferManager::TransferManager(const std::string& accountId,
                                 const std::string& accountUri,
                                 const std::string& to,
                                 const std::mt19937_64& rand)
    : pimpl_ {std::make_unique<Impl>(accountId, accountUri, to, rand)}
{}

TransferManager::~TransferManager() {}

void
TransferManager::transferFile(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                              const std::string& fileId,
                              const std::string& interactionId,
                              const std::string& path,
                              size_t start,
                              size_t end,
                              OnFinishedCb onFinished)
{
    std::lock_guard lk {pimpl_->mapMutex_};
    if (pimpl_->outgoings_.find(channel) != pimpl_->outgoings_.end())
        return;
    libjami::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;
    info.path = path;
    auto f = std::make_shared<OutgoingFile>(channel, fileId, interactionId, info, start, end);
    f->onFinished([w = weak(), channel, onFinished = std::move(onFinished)](uint32_t code) {
        if (code == uint32_t(libjami::DataTransferEventCode::finished) && onFinished) {
            onFinished();
        }
        // schedule destroy outgoing transfer as not needed
        dht::ThreadPool().computation().run([w, channel] {
            if (auto sthis_ = w.lock()) {
                auto& pimpl = sthis_->pimpl_;
                std::lock_guard lk {pimpl->mapMutex_};
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
TransferManager::cancel(const std::string& fileId)
{
    std::lock_guard lk {pimpl_->mapMutex_};
    // Remove from waiting, this avoid auto-download
    auto itW = pimpl_->waitingIds_.find(fileId);
    if (itW != pimpl_->waitingIds_.end()) {
        pimpl_->waitingIds_.erase(itW);
        JAMI_DBG() << "Cancel " << fileId;
        pimpl_->saveWaiting();
    }
    auto itC = pimpl_->incomings_.find(fileId);
    if (itC == pimpl_->incomings_.end())
        return false;
    itC->second->cancel();
    return true;
}

bool
TransferManager::info(const std::string& fileId,
                      std::string& path,
                      int64_t& total,
                      int64_t& progress) const noexcept
{
    std::unique_lock lk {pimpl_->mapMutex_};
    if (pimpl_->to_.empty())
        return false;

    auto itI = pimpl_->incomings_.find(fileId);
    auto itW = pimpl_->waitingIds_.find(fileId);
    path = this->path(fileId).string();
    if (itI != pimpl_->incomings_.end()) {
        total = itI->second->info().totalSize;
        progress = itI->second->info().bytesProgress;
        return true;
    } else if (std::filesystem::is_regular_file(path)) {
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
    // Else we don't know infos there.
    progress = 0;
    return false;
}

void
TransferManager::waitForTransfer(const std::string& fileId,
                                 const std::string& interactionId,
                                 const std::string& sha3sum,
                                 const std::string& path,
                                 std::size_t total)
{
    std::unique_lock lk(pimpl_->mapMutex_);
    auto itW = pimpl_->waitingIds_.find(fileId);
    if (itW != pimpl_->waitingIds_.end())
        return;
    pimpl_->waitingIds_[fileId] = {fileId, interactionId, sha3sum, path, total};
    pimpl_->saveWaiting();
}

void
TransferManager::onIncomingFileTransfer(const std::string& fileId,
                                        const std::shared_ptr<dhtnet::ChannelSocket>& channel)
{
    std::lock_guard lk(pimpl_->mapMutex_);
    // Check if not already an incoming file for this id and that we are waiting this file
    auto itC = pimpl_->incomings_.find(fileId);
    if (itC != pimpl_->incomings_.end()) {
        channel->shutdown();
        return;
    }
    auto itW = pimpl_->waitingIds_.find(fileId);
    if (itW == pimpl_->waitingIds_.end()) {
        channel->shutdown();
        return;
    }

    libjami::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;
    info.path = itW->second.path;
    info.totalSize = itW->second.totalSize;

    // Generate the file path within the conversation data directory
    // using the file id if no path has been specified, otherwise create
    // a symlink(Note: this will not work on Windows).
    auto filePath = path(fileId);
    if (info.path.empty()) {
        info.path = filePath.string();
    } else {
        // We don't need to check if this is an existing symlink here, as
        // the attempt to create one should report the error string correctly.
        fileutils::createFileLink(filePath, info.path);
    }
    info.bytesProgress = fileutils::size(info.path);
    if (info.bytesProgress < 0)
        info.bytesProgress = 0;

    auto ifile = std::make_shared<IncomingFile>(std::move(channel),
                                                info,
                                                fileId,
                                                itW->second.interactionId,
                                                itW->second.sha3sum);
    auto res = pimpl_->incomings_.emplace(fileId, std::move(ifile));
    if (res.second) {
        res.first->second->onFinished([w = weak(), fileId](uint32_t code) {
            // schedule destroy transfer as not needed
            dht::ThreadPool().computation().run([w, fileId, code] {
                if (auto sthis_ = w.lock()) {
                    auto& pimpl = sthis_->pimpl_;
                    std::lock_guard lk {pimpl->mapMutex_};
                    auto itO = pimpl->incomings_.find(fileId);
                    if (itO != pimpl->incomings_.end())
                        pimpl->incomings_.erase(itO);
                    if (code == uint32_t(libjami::DataTransferEventCode::finished)) {
                        auto itW = pimpl->waitingIds_.find(fileId);
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

std::filesystem::path
TransferManager::path(const std::string& fileId) const
{
    return pimpl_->conversationDataPath_ / fileId;
}

void
TransferManager::onIncomingProfile(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                                   const std::string& sha3Sum)
{
    if (!channel)
        return;

    auto chName = channel->name();
    std::string_view name = chName;
    auto sep = name.find_last_of('?');
    if (sep != std::string::npos)
        name = name.substr(0, sep);

    auto lastSep = name.find_last_of('/');
    auto fileId = name.substr(lastSep + 1);

    auto deviceId = channel->deviceId().toString();
    auto cert = channel->peerCertificate();
    if (!cert || !cert->issuer || fileId.find(".vcf") == std::string::npos)
        return;

    auto uri = fileId == "profile.vcf" ? cert->issuer->getId().toString()
                                       : std::string(fileId.substr(0, fileId.size() - 4 /*.vcf*/));

    std::lock_guard lk(pimpl_->mapMutex_);
    auto idx = std::make_pair(deviceId, uri);
    // Check if not already an incoming file for this id and that we are waiting this file
    auto itV = pimpl_->vcards_.find(idx);
    if (itV != pimpl_->vcards_.end()) {
        channel->shutdown();
        return;
    }

    auto tid = generateUID(pimpl_->rand_);
    libjami::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;

    auto recvDir = fileutils::get_cache_dir() / pimpl_->accountId_ / "vcard";
    dhtnet::fileutils::recursive_mkdir(recvDir);
    info.path = (recvDir / fmt::format("{:s}_{:s}_{}", deviceId, uri, tid)).string();

    auto ifile = std::make_shared<IncomingFile>(std::move(channel), info, "profile.vcf", "", sha3Sum);
    auto res = pimpl_->vcards_.emplace(idx, std::move(ifile));
    if (res.second) {
        res.first->second->onFinished([w = weak(),
                                       uri = std::move(uri),
                                       deviceId = std::move(deviceId),
                                       accountId = pimpl_->accountId_,
                                       cert = std::move(cert),
                                       path = info.path](uint32_t code) {
            dht::ThreadPool().computation().run([w,
                                                 uri = std::move(uri),
                                                 deviceId = std::move(deviceId),
                                                 accountId = std::move(accountId),
                                                 path = std::move(path),
                                                 code] {
                if (auto sthis_ = w.lock()) {
                    auto& pimpl = sthis_->pimpl_;

                    auto destPath = sthis_->profilePath(uri);
                    try {
                        // Move profile to destination path
                        std::lock_guard lock(dhtnet::fileutils::getFileLock(destPath));
                        dhtnet::fileutils::recursive_mkdir(destPath.parent_path());
                        std::filesystem::rename(path, destPath);
                        if (!pimpl->accountUri_.empty() && uri == pimpl->accountUri_) {
                            // If this is the account profile, link or copy it to the account profile path
                            if (!fileutils::createFileLink(pimpl->accountProfilePath_, destPath)) {
                                std::error_code ec;
                                std::filesystem::copy_file(destPath, pimpl->accountProfilePath_, ec);
                            }
                        }
                    } catch (const std::exception& e) {
                        JAMI_ERROR("{}", e.what());
                    }

                    std::lock_guard lk {pimpl->mapMutex_};
                    auto itO = pimpl->vcards_.find({deviceId, uri});
                    if (itO != pimpl->vcards_.end())
                        pimpl->vcards_.erase(itO);
                    if (code == uint32_t(libjami::DataTransferEventCode::finished)) {
                        emitSignal<libjami::ConfigurationSignal::ProfileReceived>(accountId,
                                                                                  uri,
                                                                                  destPath.string());
                    }
                }
            });
        });
        res.first->second->process();
    }
}

std::filesystem::path
TransferManager::profilePath(const std::string& contactId) const
{
    return pimpl_->profilesPath_ / fmt::format("{}.vcf", base64::encode(contactId));
}

std::vector<WaitingRequest>
TransferManager::waitingRequests() const
{
    std::vector<WaitingRequest> res;
    std::lock_guard lk(pimpl_->mapMutex_);
    for (const auto& [fileId, req] : pimpl_->waitingIds_) {
        auto itC = pimpl_->incomings_.find(fileId);
        if (itC == pimpl_->incomings_.end())
            res.emplace_back(req);
    }
    return res;
}

bool
TransferManager::isWaiting(const std::string& fileId) const
{
    std::lock_guard lk(pimpl_->mapMutex_);
    return pimpl_->waitingIds_.find(fileId) != pimpl_->waitingIds_.end();
}

} // namespace jami
