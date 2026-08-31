/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
#include "client/jami_signal.h"

#include <algorithm>
#include <mutex>
#include <cstdlib> // mkstemp
#include <filesystem>
#include <limits>

#include <opendht/rng.h>
#include <opendht/thread_pool.h>

namespace jami {

namespace {

bool
isExpectedFile(const std::filesystem::path& path, const std::string& sha3sum, std::size_t total)
{
    std::error_code ec;
    return std::filesystem::file_size(path, ec) == total && fileutils::sha3File(path) == sha3sum;
}

bool
isMissingPath(const std::filesystem::file_status& status, const std::error_code& ec)
{
    return status.type() == std::filesystem::file_type::not_found
           || ec == std::errc::no_such_file_or_directory;
}

} // namespace

libjami::DataTransferId
generateUID(std::mt19937_64& engine)
{
    return std::uniform_int_distribution<libjami::DataTransferId> {1, JAMI_ID_MAX_VAL}(engine);
}

std::string
getFileId(const std::string& commitId, const std::string& tid, const std::string& displayName)
{
    auto extension = fileutils::getFileExtension(displayName);
    if (extension.empty())
        return fmt::format("{}_{}", commitId, tid);
    return fmt::format("{}_{}.{}", commitId, tid, extension);
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
        dht::ThreadPool::io().run([channel = std::move(channel_)] { channel->shutdown(); });
        return;
    }
    stream_.open(fpath, std::ios::binary | std::ios::in);
    if (!stream_ || !stream_.is_open()) {
        dht::ThreadPool::io().run([channel = std::move(channel_)] { channel->shutdown(); });
        return;
    }
}

OutgoingFile::~OutgoingFile()
{
    if (stream_ && stream_.is_open())
        stream_.close();
    if (channel_) {
        dht::ThreadPool::io().run([channel = std::move(channel_)] { channel->shutdown(); });
    }
}

void
OutgoingFile::process()
{
    if (!channel_ or !stream_ or !stream_.is_open())
        return;
    auto correct = false;
    stream_.seekg(static_cast<long>(start_), std::ios::beg);
    try {
        std::vector<char> buffer(UINT16_MAX, 0);
        std::error_code ec;
        auto pos = start_;
        while (!stream_.eof()) {
            stream_.read(buffer.data(),
                         end_ > start_ ? static_cast<long>(std::min(end_ - pos, buffer.size()))
                                       : static_cast<long>(buffer.size()));
            auto gcount = stream_.gcount();
            pos += gcount;
            channel_->write(reinterpret_cast<const uint8_t*>(buffer.data()), gcount, ec);
            if (ec)
                break;
        }
        if (!ec)
            correct = true;
        stream_.close();
    } catch (const std::exception& e) {
        JAMI_WARNING("Failed to read from stream: {}", e.what());
    }
    if (!isUserCancelled_) {
        // NOTE: emit(code) MUST be changed to improve handling of multiple destinations
        // But for now, we can just avoid to emit errors to the client, because for outgoing
        // transfer in a swarm, for outgoingFiles, we know that the file is ok. And the peer
        // will retry the transfer if they need, so we don't need to show errors.
        if (!interactionId_.empty() && !correct)
            return;
        auto code = correct ? libjami::DataTransferEventCode::finished : libjami::DataTransferEventCode::closed_by_peer;
        emit(code);
    }
}

void
OutgoingFile::cancel()
{
    // Remove link, not original file
    auto path = fileutils::get_data_dir() / "conversation_data" / info_.accountId / info_.conversationId / fileId_;
    if (std::filesystem::is_symlink(path))
        dhtnet::fileutils::remove(path);
    isUserCancelled_ = true;
    emit(libjami::DataTransferEventCode::closed_by_host);
}

IncomingFile::IncomingFile(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                           const libjami::DataTransferInfo& info,
                           const std::string& fileId,
                           const std::string& interactionId,
                           const std::string& sha3Sum,
                           const std::filesystem::path& temporaryPath,
                           bool replaceInvalid)
    : FileInfo(channel, fileId, interactionId, info)
    , sha3Sum_(sha3Sum)
    , path_(temporaryPath)
    , replaceInvalid_(replaceInvalid)
{
    stream_.open(path_, std::ios::binary | std::ios::out | std::ios::app);
    if (!stream_)
        return;

    emit(libjami::DataTransferEventCode::ongoing);
}

IncomingFile::~IncomingFile()
{
    {
        std::lock_guard<std::mutex> lk(streamMtx_);
        if (stream_ && stream_.is_open())
            stream_.close();
    }
    if (channel_)
        dht::ThreadPool::io().run([channel = std::move(channel_)] { channel->shutdown(); });
}

void
IncomingFile::cancel()
{
    isUserCancelled_ = true;
    {
        std::lock_guard<std::mutex> lk(streamMtx_);
        if (stream_.is_open())
            stream_.close();
    }
    std::error_code ec;
    std::filesystem::remove(path_, ec);
    if (ec)
        JAMI_WARNING("Unable to remove canceled partial file {}: {}", path_, ec.message());
    emit(libjami::DataTransferEventCode::closed_by_peer);
    if (channel_)
        dht::ThreadPool::io().run([channel = std::move(channel_)] { channel->shutdown(); });
}

void
IncomingFile::process()
{
    if (!stream_.is_open()) {
        emit(libjami::DataTransferEventCode::invalid_pathname);
        if (channel_)
            dht::ThreadPool().io().run([channel = std::move(channel_)] { channel->shutdown(); });
        return;
    }
    channel_->setOnRecv([w = weak_from_this()](const uint8_t* buf, size_t len) {
        if (auto shared = w.lock()) {
            std::lock_guard<std::mutex> lk(shared->streamMtx_);
            if (!shared->stream_.is_open())
                return -1;
            shared->stream_.write(reinterpret_cast<const char*>(buf), static_cast<long>(len));
            if (!shared->stream_)
                return -1;
            shared->info_.bytesProgress = shared->stream_.tellp();
            return static_cast<int>(len);
        }
        // Data received after destruction
        JAMI_ERROR("{} bytes received after IncomingFile destruction.", len);
        return -1;
    });
    channel_->onShutdown([w = weak_from_this()](const std::error_code& /*error_code*/) {
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
            } else if (shared->info_.bytesProgress < shared->info_.totalSize) {
                JAMI_WARNING("Channel for {} shut down before transfer was complete (progress: {}/{})",
                             shared->info_.path,
                             shared->info_.bytesProgress,
                             shared->info_.totalSize);
            } else if (shared->info_.totalSize != 0 && shared->info_.bytesProgress > shared->info_.totalSize) {
                JAMI_WARNING("Removing {} larger than announced: {}/{}",
                             shared->path_,
                             shared->info_.bytesProgress,
                             shared->info_.totalSize);
                std::filesystem::remove(shared->path_, ec);
            } else {
                auto sha3Sum = fileutils::sha3File(shared->path_);
                if (shared->sha3Sum_ == sha3Sum) {
                    JAMI_LOG("New file received: {}", shared->info_.path);
                    correct = true;
                } else {
                    JAMI_WARNING(
                        "Removing {} with expected size ({} bytes) but invalid sha3sum (expected: {}, actual: {})",
                        shared->path_,
                        shared->info_.totalSize,
                        shared->sha3Sum_,
                        sha3Sum);
                    std::filesystem::remove(shared->path_, ec);
                }
            }
            if (ec) {
                JAMI_ERROR("Failed to remove file {}: {}", shared->path_, ec.message());
            }
        }
        auto invalidDestination = false;
        if (correct) {
            const std::filesystem::path destination(shared->info_.path);
            std::lock_guard fileLock(dhtnet::fileutils::getFileLock(destination));
            auto installDownloadedFile = true;
            auto destinationStatus = std::filesystem::symlink_status(destination, ec);
            auto destinationMissing = isMissingPath(destinationStatus, ec);
            if (ec && !destinationMissing) {
                JAMI_ERROR("Unable to inspect destination file {}: {}", destination, ec.message());
                correct = false;
                invalidDestination = true;
            } else if (!destinationMissing) {
                if (isExpectedFile(destination, shared->sha3Sum_, shared->info_.totalSize)) {
                    installDownloadedFile = false;
                    std::error_code removeError;
                    std::filesystem::remove(shared->path_, removeError);
                    if (removeError)
                        JAMI_WARNING("Unable to remove redundant temporary file {}: {}",
                                     shared->path_,
                                     removeError.message());
                } else if (!shared->replaceInvalid_ || destinationStatus.type() != std::filesystem::file_type::symlink) {
                    JAMI_WARNING("Refusing to overwrite existing file {}", destination);
                    std::error_code removeError;
                    std::filesystem::remove(shared->path_, removeError);
                    if (removeError)
                        JAMI_WARNING("Unable to remove temporary file {}: {}", shared->path_, removeError.message());
                    correct = false;
                    invalidDestination = true;
                }
            }
            if (correct && installDownloadedFile) {
                std::filesystem::rename(shared->path_, destination, ec);
                if (ec) {
                    JAMI_ERROR("Failed to rename file from {} to {}: {}", shared->path_, destination, ec.message());
                    correct = false;
                    invalidDestination = true;
                }
            }
            if (correct && shared->finalizeCb_ && !shared->finalizeCb_()) {
                if (installDownloadedFile) {
                    std::error_code removeError;
                    std::filesystem::remove(destination, removeError);
                    if (removeError)
                        JAMI_WARNING("Unable to roll back unindexed file {}: {}", destination, removeError.message());
                }
                correct = false;
                invalidDestination = true;
            }
        }
        if (shared->isUserCancelled_)
            return;
        auto code = correct              ? libjami::DataTransferEventCode::finished
                    : invalidDestination ? libjami::DataTransferEventCode::invalid_pathname
                                         : libjami::DataTransferEventCode::closed_by_host;
        shared->emit(code);
        dht::ThreadPool::io().run([s = std::move(shared)] {});
    });
}

//==============================================================================

class TransferManager::Impl
{
public:
    Impl(const std::string& accountId, const std::string& accountUri, const std::string& to, const std::mt19937_64& rand)
        : accountId_(accountId)
        , accountUri_(accountUri)
        , to_(to)
        , rand_(rand)
    {
        if (!to_.empty()) {
            conversationDataPath_ = fileutils::get_data_dir() / accountId_ / "conversation_data" / to_;
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
        for (auto& [channel, _] : outgoings_) {
            dht::ThreadPool::io().run([c = std::move(channel)] { c->shutdown(); });
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
            auto changed = false;
            for (auto it = waitingIds_.begin(); it != waitingIds_.end();) {
                const auto& request = it->second;
                auto fileIdPath = std::filesystem::path(request.fileId);
                auto destination = std::filesystem::path(request.path);
                if (it->first != request.fileId || request.fileId.empty() || fileIdPath.filename() != fileIdPath
                    || (!destination.empty() && destination.is_relative())) {
                    it = waitingIds_.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }
            if (changed)
                saveWaiting();
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
    auto [outFile, _] = pimpl_->outgoings_.emplace(channel, std::move(f));
    dht::ThreadPool::io().run([w = std::weak_ptr<OutgoingFile>(outFile->second)] {
        if (auto of = w.lock())
            of->process();
    });
}

bool
TransferManager::cancel(const std::string& fileId)
{
    std::shared_ptr<IncomingFile> incoming;
    std::filesystem::path partialPath;
    auto canceled = false;
    {
        std::lock_guard lk {pimpl_->mapMutex_};
        // Remove from waiting, this avoid auto-download
        auto itW = pimpl_->waitingIds_.find(fileId);
        if (itW != pimpl_->waitingIds_.end()) {
            auto destination = std::filesystem::path(itW->second.path);
            if (destination.empty())
                partialPath = temporaryPath(fileId, path(fileId));
            else if (destination.is_absolute())
                partialPath = temporaryPath(fileId, destination);
            pimpl_->waitingIds_.erase(itW);
            JAMI_LOG("Cancel {}", fileId);
            pimpl_->saveWaiting();
            canceled = true;
        }
        auto itC = pimpl_->incomings_.find(fileId);
        if (itC != pimpl_->incomings_.end())
            incoming = itC->second;
    }
    if (incoming) {
        incoming->cancel();
        return true;
    }
    if (!partialPath.empty()) {
        std::lock_guard fileLock(dhtnet::fileutils::getFileLock(partialPath));
        std::error_code ec;
        std::filesystem::remove(partialPath, ec);
        if (ec)
            JAMI_WARNING("Unable to remove canceled partial file {}: {}", partialPath, ec.message());
    }
    return canceled;
}

bool
TransferManager::info(const std::string& fileId, std::string& path, int64_t& total, int64_t& progress) const noexcept
{
    std::unique_lock lk {pimpl_->mapMutex_};
    if (pimpl_->to_.empty())
        return false;

    auto itI = pimpl_->incomings_.find(fileId);
    auto itW = pimpl_->waitingIds_.find(fileId);
    std::filesystem::path transferPath;
    try {
        transferPath = this->path(fileId);
        path = transferPath.string();
    } catch (const std::filesystem::filesystem_error& e) {
        JAMI_WARNING("Unable to resolve transfer path for {}: {}", fileId, e.what());
        progress = 0;
        return false;
    }
    if (itI != pimpl_->incomings_.end()) {
        total = itI->second->info().totalSize;
        progress = itI->second->info().bytesProgress;
        return true;
    }

    std::error_code ec;
    if (std::filesystem::is_regular_file(transferPath, ec)) {
        auto fileSize = std::filesystem::file_size(transferPath, ec);
        if (ec) {
            JAMI_WARNING("Unable to read transfer file size for {}: {}", path, ec.message());
            progress = 0;
            return false;
        }
        progress = static_cast<int64_t>(
            std::min<uintmax_t>(fileSize, static_cast<uintmax_t>(std::numeric_limits<int64_t>::max())));
        if (itW != pimpl_->waitingIds_.end()) {
            total = static_cast<int64_t>(itW->second.totalSize);
        } else {
            // If not waiting it's finished
            total = progress;
        }
        return true;
    }
    if (ec && ec != std::errc::no_such_file_or_directory) {
        JAMI_WARNING("Unable to inspect transfer path {}: {}", path, ec.message());
    }
    if (itW != pimpl_->waitingIds_.end()) {
        total = static_cast<int64_t>(itW->second.totalSize);
        progress = 0;
        return true;
    }
    // Else we don't know infos there.
    progress = 0;
    return false;
}

bool
TransferManager::verifyAndIndexFile(const std::string& fileId,
                                    const std::filesystem::path& candidate,
                                    const std::string& sha3sum,
                                    std::size_t total,
                                    bool independent)
{
    auto canonicalPath = path(fileId);
    {
        std::lock_guard fileLock(dhtnet::fileutils::getFileLock(canonicalPath));
        std::error_code ec;
        auto canonicalStatus = std::filesystem::symlink_status(canonicalPath, ec);
        auto canonicalMissing = isMissingPath(canonicalStatus, ec);
        auto canonicalIsValid = !canonicalMissing && !ec && isExpectedFile(canonicalPath, sha3sum, total);
        auto needsIndependentStorage = independent && canonicalIsValid
                                       && canonicalStatus.type() == std::filesystem::file_type::symlink;
        if (!canonicalIsValid || needsIndependentStorage) {
            if (candidate == canonicalPath || !isExpectedFile(candidate, sha3sum, total))
                return false;
            if (!canonicalMissing
                && (ec || canonicalStatus.type() != std::filesystem::file_type::symlink)) {
                JAMI_WARNING("Refusing to replace existing file transfer index {}", canonicalPath);
                return false;
            }
            libjami::DataTransferId stagingId;
            {
                std::lock_guard lk {pimpl_->mapMutex_};
                stagingId = generateUID(pimpl_->rand_);
            }
            auto stagedPath = canonicalPath.parent_path() / fmt::format(".jami-index-{}-{}.tmp", fileId, stagingId);
            auto stagedStatus = std::filesystem::symlink_status(stagedPath, ec);
            if (!isMissingPath(stagedStatus, ec)) {
                JAMI_WARNING("Refusing to replace occupied file transfer staging path {}", stagedPath);
                return false;
            }
            if (independent) {
                std::filesystem::create_hard_link(candidate, stagedPath, ec);
                if (ec) {
                    JAMI_WARNING("Unable to hard-link file transfer {} at {}, copying it: {}",
                                 fileId,
                                 stagedPath,
                                 ec.message());
                    ec.clear();
                    std::filesystem::copy_file(candidate, stagedPath, ec);
                    if (ec) {
                        JAMI_ERROR("Unable to copy completed file transfer {} to {}: {}",
                                   fileId,
                                   stagedPath,
                                   ec.message());
                        std::error_code removeError;
                        std::filesystem::remove(stagedPath, removeError);
                        return false;
                    }
                }
            } else {
                std::filesystem::create_symlink(candidate, stagedPath, ec);
                if (ec) {
                    ec.clear();
                    std::filesystem::copy_file(candidate, stagedPath, ec);
                    if (ec) {
                        JAMI_ERROR("Unable to index completed file transfer {} at {}: {}",
                                   fileId,
                                   stagedPath,
                                   ec.message());
                        std::error_code removeError;
                        std::filesystem::remove(stagedPath, removeError);
                        return false;
                    }
                }
            }
            if (!isExpectedFile(stagedPath, sha3sum, total)) {
                JAMI_ERROR("Unable to verify staged file transfer {} at {}", fileId, stagedPath);
                std::filesystem::remove(stagedPath, ec);
                return false;
            }
            std::filesystem::rename(stagedPath, canonicalPath, ec);
            if (ec) {
                JAMI_ERROR("Unable to install file transfer index {} at {}: {}", fileId, canonicalPath, ec.message());
                std::error_code removeError;
                std::filesystem::remove(stagedPath, removeError);
                return false;
            }
        }
    }

    std::lock_guard lk {pimpl_->mapMutex_};
    if (pimpl_->waitingIds_.erase(fileId) != 0)
        pimpl_->saveWaiting();
    return true;
}

TransferManager::WaitResult
TransferManager::waitForTransfer(const std::string& fileId,
                                 const std::string& interactionId,
                                 const std::string& sha3sum,
                                 const std::string& path,
                                 std::size_t total)
{
    if (!path.empty() && std::filesystem::path(path).is_relative())
        return WaitResult::conflict;
    auto canonicalPath = this->path(fileId);
    std::lock_guard fileLock(dhtnet::fileutils::getFileLock(canonicalPath));
    auto canonicalIsValid = isExpectedFile(canonicalPath, sha3sum, total);
    std::unique_lock lk(pimpl_->mapMutex_);
    if (canonicalIsValid) {
        if (pimpl_->waitingIds_.erase(fileId) != 0)
            pimpl_->saveWaiting();
        return WaitResult::complete;
    }
    auto itW = pimpl_->waitingIds_.find(fileId);
    if (itW != pimpl_->waitingIds_.end()) {
        auto matches = itW->second.interactionId == interactionId && itW->second.sha3sum == sha3sum
                       && std::filesystem::path(itW->second.path).lexically_normal()
                              == std::filesystem::path(path).lexically_normal()
                       && itW->second.totalSize == total;
        return matches ? WaitResult::waiting : WaitResult::conflict;
    }
    auto destination = path.empty() ? canonicalPath : std::filesystem::path(path);
    std::error_code ec;
    auto partialStatus = std::filesystem::symlink_status(temporaryPath(fileId, destination), ec);
    if (!isMissingPath(partialStatus, ec)) {
        return WaitResult::conflict;
    }
    pimpl_->waitingIds_[fileId] = {fileId, interactionId, sha3sum, path, total};
    pimpl_->saveWaiting();
    return WaitResult::waiting;
}

void
TransferManager::onIncomingFileTransfer(const std::string& fileId,
                                        const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                                        size_t start)
{
    std::unique_lock lk(pimpl_->mapMutex_);
    // Check if not already an incoming file for this id and that we are waiting this file
    auto itC = pimpl_->incomings_.find(fileId);
    if (itC != pimpl_->incomings_.end()) {
        dht::ThreadPool().io().run([channel] { channel->shutdown(); });
        return;
    }
    auto itW = pimpl_->waitingIds_.find(fileId);
    if (itW == pimpl_->waitingIds_.end()) {
        dht::ThreadPool().io().run([channel] { channel->shutdown(); });
        return;
    }

    libjami::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;
    info.path = itW->second.path;
    info.totalSize = static_cast<int64_t>(itW->second.totalSize);
    info.bytesProgress = static_cast<int64_t>(start);

    // Generate the file path within the conversation data directory using the file id
    // if no path has been specified. For an external destination, install the index link
    // only after the incoming file has been verified.
    auto filePath = path(fileId);
    if (info.path.empty()) {
        info.path = filePath.string();
    }
    const std::filesystem::path destinationPath(info.path);
    const auto replacesCanonical = destinationPath.lexically_normal() == filePath.lexically_normal();
    const auto partialPath = temporaryPath(fileId, destinationPath);
    const auto expectedSize = itW->second.totalSize;
    const auto expectedSha3 = itW->second.sha3sum;

    auto ifile = std::make_shared<IncomingFile>(std::move(channel),
                                                info,
                                                fileId,
                                                itW->second.interactionId,
                                                itW->second.sha3sum,
                                                partialPath,
                                                replacesCanonical);
    auto res = pimpl_->incomings_.emplace(fileId, std::move(ifile));
    if (res.second) {
        if (!replacesCanonical) {
            res.first->second->onFinalize([w = weak(), fileId, destinationPath, expectedSize, expectedSha3] {
                if (auto sthis = w.lock())
                    return sthis->verifyAndIndexFile(fileId, destinationPath, expectedSha3, expectedSize);
                return false;
            });
        }
        res.first->second->onFinished([w = weak(), fileId](uint32_t code) {
            if (auto sthis = w.lock()) {
                auto& pimpl = sthis->pimpl_;
                std::lock_guard lk {pimpl->mapMutex_};
                pimpl->incomings_.erase(fileId);
                if ((code == uint32_t(libjami::DataTransferEventCode::finished)
                     || code == uint32_t(libjami::DataTransferEventCode::invalid_pathname))
                    && pimpl->waitingIds_.erase(fileId) != 0) {
                    pimpl->saveWaiting();
                }
            }
        });
        auto incoming = res.first->second;
        lk.unlock();
        incoming->process();
    }
}

std::filesystem::path
TransferManager::path(const std::string& fileId) const
{
    return pimpl_->conversationDataPath_ / fileId;
}

std::filesystem::path
TransferManager::temporaryPath(const std::string& fileId, const std::filesystem::path& destination) const
{
    return destination.parent_path() / fmt::format(".jami-{}-{}-{}.tmp", pimpl_->accountId_, pimpl_->to_, fileId);
}

void
TransferManager::onIncomingProfile(const std::shared_ptr<dhtnet::ChannelSocket>& channel, const std::string& sha3Sum)
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
        dht::ThreadPool().io().run([channel] { channel->shutdown(); });
        return;
    }

    auto tid = generateUID(pimpl_->rand_);
    libjami::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;

    auto recvDir = fileutils::get_cache_dir() / pimpl_->accountId_ / "vcard";
    dhtnet::fileutils::recursive_mkdir(recvDir);
    info.path = (recvDir / fmt::format("{:s}_{:s}_{}", deviceId, uri, tid)).string();

    auto ifile = std::make_shared<IncomingFile>(std::move(channel),
                                                info,
                                                "profile.vcf",
                                                "",
                                                sha3Sum,
                                                std::filesystem::path(info.path + ".tmp"));
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
                        emitSignal<libjami::ConfigurationSignal::ProfileReceived>(accountId, uri, destPath.string());
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
