/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
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

#include "base64.h"
#include "client/ring_signal.h"
#include "fileutils.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/p2p.h"
#include "manager.h"
#include "map_utils.h"
#include "peer_connection.h"
#include "string_utils.h"

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
#include <cstdlib>  // mkstemp
#include <filesystem>

#include <opendht/rng.h>
#include <opendht/thread_pool.h>

namespace jami {

DRing::DataTransferId
generateUID()
{
    thread_local dht::crypto::random_device rd;
    return std::uniform_int_distribution<DRing::DataTransferId> {1, JAMI_ID_MAX_VAL}(rd);
}

constexpr const uint32_t MAX_BUFFER_SIZE {65534}; /* Channeled max packet size */
//==============================================================================

class DataTransfer : public Stream
{
public:
    DataTransfer(DRing::DataTransferId id, InternalCompletionCb cb = {})
        : Stream()
        , id {id}
        , internalCompletionCb_ {std::move(cb)}
    {}

    virtual ~DataTransfer() = default;

    DRing::DataTransferId getId() const override { return id; }

    virtual void accept(const std::string&, std::size_t) {};

    virtual bool start()
    {
        wasStarted_ = true;
        bool expected = false;
        return started_.compare_exchange_strong(expected, true);
    }

    virtual bool hasBeenStarted() const { return wasStarted_; }

    void close() noexcept override { started_ = false; }

    void bytesProgress(int64_t& total, int64_t& progress) const
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        total = info_.totalSize;
        progress = info_.bytesProgress;
    }

    void setBytesProgress(int64_t progress) const
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        info_.bytesProgress = progress;
    }

    void info(DRing::DataTransferInfo& info) const
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        info = info_;
    }

    bool isFinished() const
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        return info_.lastEvent >= DRing::DataTransferEventCode::finished;
    }

    DRing::DataTransferInfo info() const { return info_; }

    virtual void emit(DRing::DataTransferEventCode code) const;

    const DRing::DataTransferId id;

    virtual void cancel() {}

    void setOnStateChangedCb(const OnStateChangedCb& cb) override;

protected:
    mutable std::mutex infoMutex_;
    mutable DRing::DataTransferInfo info_;
    mutable std::atomic_bool started_ {false};
    std::atomic_bool wasStarted_ {false};
    InternalCompletionCb internalCompletionCb_ {};
    OnStateChangedCb stateChangedCb_ {};
};

void
DataTransfer::emit(DRing::DataTransferEventCode code) const
{
    std::string accountId, to;
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        info_.lastEvent = code;
        accountId = info_.accountId;
        to = info_.conversationId;
        if (to.empty())
            to = info_.peer;
    }
    if (stateChangedCb_)
        stateChangedCb_(id, code);
    if (internalCompletionCb_)
        return; // VCard transfer is just for the daemon
    runOnMainThread([id = id, code, accountId, to]() {
        emitSignal<DRing::DataTransferSignal::DataTransferEvent>(accountId,
                                                                 "",
                                                                 "",
                                                                 std::to_string(id),
                                                                 uint32_t(code));
    });
}

void
DataTransfer::setOnStateChangedCb(const OnStateChangedCb& cb)
{
    stateChangedCb_ = std::move(cb);
}

//==============================================================================

/**
 * This class is used as a sort of buffer between the OutgoingFileTransfer
 * used by clients to represent a transfer between the user and a contact
 * and SubOutgoingFileTransfer representing the transfer between the user and
 * each peer devices. It gives the optimistic view of a transfer (show a failure)
 * only if all related transfer has failed. If one transfer succeed, ignore failures.
 */
class OptimisticMetaOutgoingInfo
{
public:
    OptimisticMetaOutgoingInfo(const DataTransfer* parent, const DRing::DataTransferInfo& info);
    /**
     * Update the DataTransferInfo of the parent if necessary (if the event is more *interesting*
     * for the user)
     * @param info the last modified linked info (for example if a subtransfer is accepted, it will
     * gives as a parameter its info)
     */
    void updateInfo(const DRing::DataTransferInfo& info) const;
    /**
     * Add a subtransfer as a linked transfer
     * @param linked
     */
    void addLinkedTransfer(DataTransfer* linked) const;
    /**
     * Return the optimistic representation of the transfer
     */
    const DRing::DataTransferInfo& info() const;

private:
    const DataTransfer* parent_;
    mutable std::mutex infoMutex_;
    mutable DRing::DataTransferInfo info_;
    mutable std::vector<DataTransfer*> linkedTransfers_;
};

OptimisticMetaOutgoingInfo::OptimisticMetaOutgoingInfo(const DataTransfer* parent,
                                                       const DRing::DataTransferInfo& info)
    : parent_(parent)
    , info_(info)
{}

void
OptimisticMetaOutgoingInfo::updateInfo(const DRing::DataTransferInfo& info) const
{
    bool emitCodeChanged = false;
    bool checkOngoing = false;
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        if (info_.lastEvent > DRing::DataTransferEventCode::timeout_expired) {
            info_.lastEvent = DRing::DataTransferEventCode::invalid;
        }
        if (info.lastEvent >= DRing::DataTransferEventCode::created
            && info.lastEvent <= DRing::DataTransferEventCode::finished
            && info.lastEvent > info_.lastEvent) {
            // Show the more advanced info
            info_.lastEvent = info.lastEvent;
            emitCodeChanged = true;
        }

        if (info.lastEvent >= DRing::DataTransferEventCode::closed_by_host
            && info.lastEvent <= DRing::DataTransferEventCode::timeout_expired
            && info_.lastEvent < DRing::DataTransferEventCode::finished) {
            // if not finished show error if all failed
            // if the transfer was ongoing and canceled, we should go to the best status
            bool isAllFailed = true;
            checkOngoing = info_.lastEvent == DRing::DataTransferEventCode::ongoing;
            DRing::DataTransferEventCode bestEvent {DRing::DataTransferEventCode::invalid};
            for (const auto* transfer : linkedTransfers_) {
                const auto& i = transfer->info();
                if (i.lastEvent >= DRing::DataTransferEventCode::created
                    && i.lastEvent <= DRing::DataTransferEventCode::finished) {
                    isAllFailed = false;
                    if (checkOngoing)
                        bestEvent = bestEvent > i.lastEvent ? bestEvent : i.lastEvent;
                    else
                        break;
                }
            }
            if (isAllFailed) {
                info_.lastEvent = info.lastEvent;
                emitCodeChanged = true;
            } else if (checkOngoing && bestEvent != DRing::DataTransferEventCode::invalid) {
                info_.lastEvent = bestEvent;
                emitCodeChanged = true;
            }
        }

        int64_t bytesProgress {0};
        for (const auto* transfer : linkedTransfers_) {
            const auto& i = transfer->info();
            if (i.bytesProgress > bytesProgress) {
                bytesProgress = i.bytesProgress;
            }
        }
        if (bytesProgress > info_.bytesProgress) {
            info_.bytesProgress = bytesProgress;
            parent_->setBytesProgress(info_.bytesProgress);
        }
        if (checkOngoing && info_.lastEvent != DRing::DataTransferEventCode::invalid) {
            parent_->setBytesProgress(0);
        }
    }

    if (emitCodeChanged) {
        parent_->emit(info_.lastEvent);
    }
}

void
OptimisticMetaOutgoingInfo::addLinkedTransfer(DataTransfer* linked) const
{
    std::lock_guard<std::mutex> lk {infoMutex_};
    linkedTransfers_.emplace_back(linked);
}

const DRing::DataTransferInfo&
OptimisticMetaOutgoingInfo::info() const
{
    return info_;
}

/**
 * Represent a outgoing file transfer between a user and a device
 */
class SubOutgoingFileTransfer final : public DataTransfer
{
public:
    SubOutgoingFileTransfer(DRing::DataTransferId tid,
                            const std::string& peerUri,
                            const InternalCompletionCb& cb,
                            std::shared_ptr<OptimisticMetaOutgoingInfo> metaInfo);
    ~SubOutgoingFileTransfer();

    void close() noexcept override;
    void closeAndEmit(DRing::DataTransferEventCode code) const noexcept;
    bool write(std::string_view) override;
    void emit(DRing::DataTransferEventCode code) const override;
    const std::string& peer() { return peerUri_; }

    void cancel() override
    {
        if (auto account = Manager::instance().getAccount<JamiAccount>(info_.accountId))
            account->closePeerConnection(id);
    }

    void setOnRecv(std::function<void(std::string_view)>&& cb) override
    {
        bool send = false;
        {
            std::lock_guard<std::mutex> lock(onRecvCbMtx_);
            if (cb)
                send = true;
            onRecvCb_ = std::move(cb);
        }
        if (send) {
            sendHeader(); // Pass headers to the new callback
        }
    }

private:
    SubOutgoingFileTransfer() = delete;

    void sendHeader() const
    {
        auto header = fmt::format("Content-Length: {}\n"
                                  "Display-Name: {}\n"
                                  "Offset: 0\n\n",
                                  info_.totalSize,
                                  info_.displayName);
        headerSent_ = true;
        emit(DRing::DataTransferEventCode::wait_peer_acceptance);
        if (onRecvCb_)
            onRecvCb_(header);
    }

    void sendFile() const
    {
        dht::ThreadPool::io().run([this]() {
            std::vector<char> buf;
            while (!input_.eof() && onRecvCb_) {
                buf.resize(MAX_BUFFER_SIZE);

                input_.read(&buf[0], buf.size());
                buf.resize(input_.gcount());
                if (buf.size()) {
                    std::lock_guard<std::mutex> lk {infoMutex_};
                    info_.bytesProgress += buf.size();
                    metaInfo_->updateInfo(info_);
                }
                if (onRecvCb_)
                    onRecvCb_(std::string_view(buf.data(), buf.size()));
            }
            JAMI_DBG() << "FTP#" << getId() << ": sent " << info_.bytesProgress << " bytes";

            if (info_.bytesProgress != info_.totalSize)
                emit(DRing::DataTransferEventCode::closed_by_peer);
            else {
                if (internalCompletionCb_)
                    internalCompletionCb_(info_.path);
                emit(DRing::DataTransferEventCode::finished);
            }
        });
    }

    mutable std::shared_ptr<OptimisticMetaOutgoingInfo> metaInfo_;
    mutable std::ifstream input_;
    mutable bool headerSent_ {false};
    bool peerReady_ {false};
    const std::string peerUri_;
    mutable std::shared_ptr<Task> timeoutTask_;
    std::mutex onRecvCbMtx_;
    std::function<void(std::string_view)> onRecvCb_ {};
};

SubOutgoingFileTransfer::SubOutgoingFileTransfer(DRing::DataTransferId tid,
                                                 const std::string& peerUri,
                                                 const InternalCompletionCb& cb,
                                                 std::shared_ptr<OptimisticMetaOutgoingInfo> metaInfo)
    : DataTransfer(tid, cb)
    , metaInfo_(std::move(metaInfo))
    , peerUri_(peerUri)
{
    info_ = metaInfo_->info();
    fileutils::openStream(input_, info_.path, std::ios::in | std::ios::binary);
    if (!input_)
        throw std::runtime_error("input file open failed");
    metaInfo_->addLinkedTransfer(this);
}

SubOutgoingFileTransfer::~SubOutgoingFileTransfer()
{
    if (timeoutTask_)
        timeoutTask_->cancel();
}

void
SubOutgoingFileTransfer::close() noexcept
{
    closeAndEmit(DRing::DataTransferEventCode::closed_by_host);
}

void
SubOutgoingFileTransfer::closeAndEmit(DRing::DataTransferEventCode code) const noexcept
{
    started_ = false; // NOTE: replace DataTransfer::close(); which is non const
    input_.close();

    if (info_.lastEvent < DRing::DataTransferEventCode::finished)
        emit(code);
}

bool
SubOutgoingFileTransfer::write(std::string_view buffer)
{
    if (buffer.empty())
        return true;
    if (not peerReady_ and headerSent_) {
        // detect GO or NGO msg
        if (buffer.size() == 3 and buffer[0] == 'G' and buffer[1] == 'O' and buffer[2] == '\n') {
            peerReady_ = true;
            emit(DRing::DataTransferEventCode::ongoing);
            if (onRecvCb_)
                sendFile();
        } else {
            // consider any other response as a cancel msg
            JAMI_WARN() << "FTP#" << getId() << ": refused by peer";
            emit(DRing::DataTransferEventCode::closed_by_peer);
            return false;
        }
    }
    return true;
}

void
SubOutgoingFileTransfer::emit(DRing::DataTransferEventCode code) const
{
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        info_.lastEvent = code;
    }
    if (stateChangedCb_)
        stateChangedCb_(id, code);
    metaInfo_->updateInfo(info_);
    if (code == DRing::DataTransferEventCode::wait_peer_acceptance) {
        if (timeoutTask_)
            timeoutTask_->cancel();
        timeoutTask_ = Manager::instance().scheduleTaskIn(
            [this]() {
                JAMI_WARN() << "FTP#" << getId() << ": timeout. Cancel";
                closeAndEmit(DRing::DataTransferEventCode::timeout_expired);
            },
            std::chrono::minutes(10));
    } else if (timeoutTask_) {
        timeoutTask_->cancel();
        timeoutTask_.reset();
    }
}

/**
 * Represent a file transfer between a user and a peer (all of its device)
 */
class OutgoingFileTransfer final : public DataTransfer
{
public:
    OutgoingFileTransfer(DRing::DataTransferId tid,
                         const DRing::DataTransferInfo& info,
                         const InternalCompletionCb& cb = {});
    ~OutgoingFileTransfer() {}

    std::shared_ptr<DataTransfer> startNewOutgoing(const std::string& peer_uri)
    {
        auto newTransfer = std::make_shared<SubOutgoingFileTransfer>(id,
                                                                     peer_uri,
                                                                     internalCompletionCb_,
                                                                     metaInfo_);
        newTransfer->setOnStateChangedCb(stateChangedCb_);
        subtransfer_.emplace_back(newTransfer);
        newTransfer->start();
        return newTransfer;
    }

    bool hasBeenStarted() const override
    {
        // Started if one subtransfer is started
        for (const auto& subtransfer : subtransfer_)
            if (subtransfer->hasBeenStarted())
                return true;
        return false;
    }

    void close() noexcept override;

    bool cancelWithPeer(const std::string& peer)
    {
        auto allFinished = true;
        for (const auto& subtransfer : subtransfer_) {
            if (subtransfer->peer() == peer)
                subtransfer->cancel();
            else if (!subtransfer->isFinished())
                allFinished = false;
        }
        return allFinished;
    }

private:
    OutgoingFileTransfer() = delete;

    mutable std::shared_ptr<OptimisticMetaOutgoingInfo> metaInfo_;
    mutable std::ifstream input_;
    mutable std::vector<std::shared_ptr<SubOutgoingFileTransfer>> subtransfer_;
};

OutgoingFileTransfer::OutgoingFileTransfer(DRing::DataTransferId tid,
                                           const DRing::DataTransferInfo& info,
                                           const InternalCompletionCb& cb)
    : DataTransfer(tid, cb)
{
    fileutils::openStream(input_, info.path, std::ios::binary);
    if (!input_)
        throw std::runtime_error("input file open failed");

    info_ = info;
    info_.flags &= ~((uint32_t) 1 << int(DRing::DataTransferFlags::direction)); // outgoing

    // File size?
    input_.seekg(0, std::ios_base::end);
    info_.totalSize = input_.tellg();
    input_.close();

    metaInfo_ = std::make_shared<OptimisticMetaOutgoingInfo>(this, this->info_);
}

void
OutgoingFileTransfer::close() noexcept
{
    for (const auto& subtransfer : subtransfer_)
        subtransfer->close();
}

//==============================================================================

class IncomingFileTransfer final : public DataTransfer
{
public:
    IncomingFileTransfer(const DRing::DataTransferInfo&,
                         DRing::DataTransferId,
                         const InternalCompletionCb& cb = {});

    bool start() override;

    void close() noexcept override;

    void requestFilename(const std::function<void(const std::string&)>& cb);

    void accept(const std::string&, std::size_t offset) override;

    bool write(std::string_view data) override;

    void setFilename(const std::string& filename);

    void cancel() override
    {
        auto account = Manager::instance().getAccount<JamiAccount>(info_.accountId);
        if (account)
            account->closePeerConnection(internalId_);
    }

private:
    IncomingFileTransfer() = delete;

    DRing::DataTransferId internalId_;

    std::ofstream fout_;
    std::mutex cbMtx_ {};
    std::function<void(const std::string&)> onFilenameCb_ {};
};

IncomingFileTransfer::IncomingFileTransfer(const DRing::DataTransferInfo& info,
                                           DRing::DataTransferId internalId,
                                           const InternalCompletionCb& cb)
    : DataTransfer(internalId, cb)
    , internalId_(internalId)
{
    JAMI_WARN() << "[FTP] incoming transfert of " << info.totalSize
                << " byte(s): " << info.displayName;

    info_ = info;
    info_.flags |= (uint32_t) 1 << int(DRing::DataTransferFlags::direction); // incoming
}

void
IncomingFileTransfer::setFilename(const std::string& filename)
{
    info_.path = filename;
}

void
IncomingFileTransfer::requestFilename(const std::function<void(const std::string&)>& cb)
{
    if (!internalCompletionCb_) {
        std::lock_guard<std::mutex> lk(cbMtx_);
        onFilenameCb_ = cb;
    }

    emit(DRing::DataTransferEventCode::wait_host_acceptance);

    if (internalCompletionCb_) {
        std::string path = fmt::format("{}/{}/profiles",
                                       fileutils::get_cache_dir(),
                                       info_.accountId);
        fileutils::recursive_mkdir(path);
        auto filename = fmt::format("{}/{}", path, id);
        fileutils::ofstream(filename);
        if (not fileutils::isFile(filename))
            throw std::system_error(errno, std::generic_category());
        info_.path = filename;
        cb(filename);
    }
}

bool
IncomingFileTransfer::start()
{
    if (!DataTransfer::start())
        return false;

    fileutils::openStream(fout_, &info_.path[0], std::ios::binary);
    if (!fout_) {
        JAMI_ERR() << "[FTP] Can't open file " << info_.path;
        return false;
    }

    emit(DRing::DataTransferEventCode::ongoing);
    return true;
}

void
IncomingFileTransfer::close() noexcept
{
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        if (info_.lastEvent >= DRing::DataTransferEventCode::finished)
            return;
    }
    DataTransfer::close();

    decltype(onFilenameCb_) cb;
    {
        std::lock_guard<std::mutex> lk(cbMtx_);
        cb = std::move(onFilenameCb_);
    }
    if (cb)
        cb("");

    fout_.close();

    JAMI_DBG() << "[FTP] file closed, rx " << info_.bytesProgress << " on " << info_.totalSize;
    if (info_.bytesProgress >= info_.totalSize) {
        if (internalCompletionCb_)
            internalCompletionCb_(info_.path);
        emit(DRing::DataTransferEventCode::finished);
    } else
        emit(DRing::DataTransferEventCode::closed_by_host);
}

void
IncomingFileTransfer::accept(const std::string& filename, std::size_t offset)
{
    // TODO: offset?
    (void) offset;

    info_.path = filename;
    decltype(onFilenameCb_) cb;
    {
        std::lock_guard<std::mutex> lk(cbMtx_);
        cb = std::move(onFilenameCb_);
    }
    if (cb)
        cb(filename);
}

bool
IncomingFileTransfer::write(std::string_view buffer)
{
    if (buffer.empty())
        return true;
    fout_ << buffer;
    if (!fout_)
        return false;
    std::lock_guard<std::mutex> lk {infoMutex_};
    info_.bytesProgress += buffer.size();
    return true;
}

//==============================================================================
//                                 With Swarm
//==============================================================================

FileInfo::FileInfo(const std::shared_ptr<ChannelSocket>& channel,
                   const std::string& fileId,
                   const std::string& interactionId,
                   const DRing::DataTransferInfo& info)
    : fileId_(fileId)
    , interactionId_(interactionId)
    , info_(info)
    , channel_(channel)
{}

void
FileInfo::emit(DRing::DataTransferEventCode code)
{
    if (finishedCb_ && code >= DRing::DataTransferEventCode::finished)
        finishedCb_(uint32_t(code));
    if (interactionId_ != "") {
        // Else it's an internal transfer
        runOnMainThread([info = info_, iid = interactionId_, fid = fileId_, code]() {
            emitSignal<DRing::DataTransferSignal::DataTransferEvent>(info.accountId,
                                                                     info.conversationId,
                                                                     iid,
                                                                     fid,
                                                                     uint32_t(code));
        });
    }
}

OutgoingFile::OutgoingFile(const std::shared_ptr<ChannelSocket>& channel,
                           const std::string& fileId,
                           const std::string& interactionId,
                           const DRing::DataTransferInfo& info,
                           size_t start,
                           size_t end)
    : FileInfo(channel, fileId, interactionId, info)
    , start_(start)
    , end_(end)
{
    if (!fileutils::isFile(info_.path)) {
        channel_->shutdown();
        return;
    }
    fileutils::openStream(stream_, info_.path, std::ios::binary | std::ios::in);
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
        auto code = correct ? DRing::DataTransferEventCode::finished
                            : DRing::DataTransferEventCode::closed_by_peer;
        emit(code);
    }
}

void
OutgoingFile::cancel()
{
    // Remove link, not original file
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + "conversation_data"
                + DIR_SEPARATOR_STR + info_.accountId + DIR_SEPARATOR_STR + info_.conversationId
                + DIR_SEPARATOR_STR + fileId_;
    if (fileutils::isSymLink(path))
        fileutils::remove(path);
    isUserCancelled_ = true;
    emit(DRing::DataTransferEventCode::closed_by_host);
}

IncomingFile::IncomingFile(const std::shared_ptr<ChannelSocket>& channel,
                           const DRing::DataTransferInfo& info,
                           const std::string& fileId,
                           const std::string& interactionId,
                           const std::string& sha3Sum)
    : FileInfo(channel, fileId, interactionId, info)
    , sha3Sum_(sha3Sum)
{
    fileutils::openStream(stream_, info_.path, std::ios::binary | std::ios::out);
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
    channel_->setOnRecv([w = weak()](const uint8_t* buf, size_t len) {
        if (auto shared = w.lock()) {
            if (shared->stream_.is_open())
                shared->stream_.write(reinterpret_cast<const char*>(buf), len);
            shared->info_.bytesProgress = shared->stream_.tellp();
        }
        return len;
    });
    channel_->onShutdown([w = weak()] {
        auto shared = w.lock();
        if (!shared)
            return;
        auto correct = shared->sha3Sum_.empty();
        if (!correct) {
            if (shared->stream_ && shared->stream_.is_open())
                shared->stream_.close();
            // Verify shaSum
            auto sha3Sum = fileutils::sha3File(shared->info_.path);
            if (shared->sha3Sum_ == sha3Sum) {
                JAMI_INFO() << "New file received: " << shared->info_.path;
                correct = true;
            } else {
                JAMI_WARN() << "Remove file, invalid sha3sum detected for " << shared->info_.path;
                fileutils::remove(shared->info_.path, true);
            }
        }
        if (shared->isUserCancelled_)
            return;
        auto code = correct ? DRing::DataTransferEventCode::finished
                            : DRing::DataTransferEventCode::closed_by_host;
        shared->emit(code);
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
        if (!to_.empty()) {
            conversationDataPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId_
                                    + DIR_SEPARATOR_STR + "conversation_data" + DIR_SEPARATOR_STR
                                    + to_;
            fileutils::check_dir(conversationDataPath_.c_str());
            waitingPath_ = conversationDataPath_ + DIR_SEPARATOR_STR + "waiting";
        }
        profilesPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId_
                        + DIR_SEPARATOR_STR + "profiles";
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
    std::string profilesPath_ {};
    std::string conversationDataPath_ {};

    // Pre swarm
    std::map<DRing::DataTransferId, std::shared_ptr<OutgoingFileTransfer>> oMap_ {};
    std::map<DRing::DataTransferId, std::shared_ptr<IncomingFileTransfer>> iMap_ {};

    std::mutex mapMutex_ {};
    std::map<std::string, WaitingRequest> waitingIds_ {};
    std::map<std::shared_ptr<ChannelSocket>, std::shared_ptr<OutgoingFile>> outgoings_ {};
    std::map<std::string, std::shared_ptr<IncomingFile>> incomings_ {};
    std::map<std::pair<std::string, std::string>, std::shared_ptr<IncomingFile>> vcards_ {};
};

TransferManager::TransferManager(const std::string& accountId, const std::string& to)
    : pimpl_ {std::make_unique<Impl>(accountId, to)}
{}

TransferManager::~TransferManager() {}

DRing::DataTransferId
TransferManager::sendFile(const std::string& path,
                          const std::string& peer,
                          const InternalCompletionCb& icb)
{
    // IMPLEMENTATION NOTE: requestPeerConnection() may call the given callback a multiple time.
    // This happen when multiple agents handle communications of the given peer for the given
    // account. Example: Jami account supports multi-devices, each can answer to the request.
    auto account = Manager::instance().getAccount<JamiAccount>(pimpl_->accountId_);
    if (!account) {
        return {};
    }

    auto tid = generateUID();
    std::size_t found = path.find_last_of(DIR_SEPARATOR_CH);
    auto filename = path.substr(found + 1);

    DRing::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.author = account->getUsername();
    info.peer = peer;
    info.path = path;
    info.displayName = filename;
    info.bytesProgress = 0;

    auto transfer = std::make_shared<OutgoingFileTransfer>(tid, info, icb);
    {
        std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
        auto it = pimpl_->oMap_.find(tid);
        if (it != pimpl_->oMap_.end()) {
            // If the transfer is already in progress (aka not finished)
            // we do not need to send the request and can ignore it.
            if (!it->second->isFinished()) {
                JAMI_DBG("Can't send request for %lu. Already sending the file", tid);
                return {};
            }
            pimpl_->oMap_.erase(it);
        }
        pimpl_->oMap_.emplace(tid, transfer);
    }
    transfer->emit(DRing::DataTransferEventCode::created);

    try {
        account->requestConnection(
            info,
            tid,
            static_cast<bool>(icb),
            [transfer](const std::shared_ptr<ChanneledOutgoingTransfer>& out) {
                if (out)
                    out->linkTransfer(transfer->startNewOutgoing(out->peer()));
            },
            [transfer](const std::string& peer) {
                auto allFinished = transfer->cancelWithPeer(peer);
                if (allFinished and not transfer->hasBeenStarted()) {
                    transfer->emit(DRing::DataTransferEventCode::unjoinable_peer);
                    transfer->cancel();
                    transfer->close();
                }
            });
    } catch (const std::exception& ex) {
        JAMI_ERR() << "[XFER] exception during sendFile(): " << ex.what();
        return {};
    }

    return tid;
}

bool
TransferManager::acceptFile(const DRing::DataTransferId& id, const std::string& path)
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    auto it = pimpl_->iMap_.find(id);
    if (it == pimpl_->iMap_.end()) {
        JAMI_WARN("Cannot accept %lu, request not found", id);
        return false;
    }
    it->second->accept(path, 0);
    return true;
}

void
TransferManager::transferFile(const std::shared_ptr<ChannelSocket>& channel,
                              const std::string& fileId,
                              const std::string& interactionId,
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
    auto f = std::make_shared<OutgoingFile>(channel, fileId, interactionId, info, start, end);
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
TransferManager::cancel(const std::string& fileId)
{
    std::shared_ptr<ChannelSocket> channel;
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    if (!pimpl_->to_.empty()) {
        // Remove from waiting, this avoid auto-download
        auto itW = pimpl_->waitingIds_.find(fileId);
        if (itW != pimpl_->waitingIds_.end()) {
            pimpl_->waitingIds_.erase(itW);
            JAMI_DBG() << "Cancel " << fileId;
            pimpl_->saveWaiting();
        }
        // Note: For now, there is no cancel for outgoings.
        // The client can just remove the file.
        auto itC = pimpl_->incomings_.find(fileId);
        if (itC == pimpl_->incomings_.end())
            return false;
        itC->second->cancel();
        return true;
    }
    // Else, this is fallack.
    try {
        auto it = pimpl_->iMap_.find(std::stoull(fileId));
        if (it != pimpl_->iMap_.end()) {
            if (it->second)
                it->second->close();
            return true;
        }
        auto itO = pimpl_->oMap_.find(std::stoull(fileId));
        if (itO != pimpl_->oMap_.end()) {
            if (itO->second)
                itO->second->close();
            return true;
        }
    } catch (...) {
        JAMI_ERR() << "Invalid fileId: " << fileId;
    }
    return false;
}

bool
TransferManager::info(const DRing::DataTransferId& id, DRing::DataTransferInfo& info) const noexcept
{
    std::unique_lock<std::mutex> lk {pimpl_->mapMutex_};
    if (!pimpl_->to_.empty())
        return false;
    // Else it's fallback
    if (auto it = pimpl_->iMap_.find(id); it != pimpl_->iMap_.end()) {
        if (it->second)
            it->second->info(info);
        return true;
    }
    if (auto it = pimpl_->oMap_.find(id); it != pimpl_->oMap_.end()) {
        if (it->second)
            it->second->info(info);
        return true;
    }
    return false;
}

bool
TransferManager::info(const std::string& fileId,
                      std::string& path,
                      int64_t& total,
                      int64_t& progress) const noexcept
{
    std::unique_lock<std::mutex> lk {pimpl_->mapMutex_};
    if (pimpl_->to_.empty())
        return false;

    auto itI = pimpl_->incomings_.find(fileId);
    auto itW = pimpl_->waitingIds_.find(fileId);
    path = this->path(fileId);
    if (itI != pimpl_->incomings_.end()) {
        total = itI->second->info().totalSize;
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
    // Else we don't know infos there.
    progress = 0;
    return false;
}

void
TransferManager::onIncomingFileRequest(const DRing::DataTransferInfo& info,
                                       const DRing::DataTransferId& id,
                                       const std::function<void(const IncomingFileInfo&)>& cb,
                                       const InternalCompletionCb& icb)
{
    auto transfer = std::make_shared<IncomingFileTransfer>(info, id, icb);
    {
        std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
        pimpl_->iMap_.emplace(id, transfer);
    }
    transfer->emit(DRing::DataTransferEventCode::created);
    transfer->requestFilename([transfer, id, cb = std::move(cb)](const std::string& filename) {
        if (!filename.empty() && transfer->start())
            cb({id, std::static_pointer_cast<Stream>(transfer)});
        else
            cb({id, nullptr});
    });
}

void
TransferManager::waitForTransfer(const std::string& fileId,
                                 const std::string& interactionId,
                                 const std::string& sha3sum,
                                 const std::string& path,
                                 std::size_t total)
{
    std::unique_lock<std::mutex> lk(pimpl_->mapMutex_);
    auto itW = pimpl_->waitingIds_.find(fileId);
    if (itW != pimpl_->waitingIds_.end())
        return;
    pimpl_->waitingIds_[fileId] = {fileId, interactionId, sha3sum, path, total};
    JAMI_DBG() << "Wait for " << fileId;
    if (!pimpl_->to_.empty())
        pimpl_->saveWaiting();
    lk.unlock();
    emitSignal<DRing::DataTransferSignal::DataTransferEvent>(
        pimpl_->accountId_,
        pimpl_->to_,
        interactionId,
        fileId,
        uint32_t(DRing::DataTransferEventCode::wait_peer_acceptance));
}

void
TransferManager::onIncomingFileTransfer(const std::string& fileId,
                                        const std::shared_ptr<ChannelSocket>& channel)
{
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
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

    DRing::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;
    info.path = itW->second.path;
    info.totalSize = itW->second.totalSize;
    info.bytesProgress = 0;

    // Generate the file path within the conversation data directory
    // using the file id if no path has been specified, otherwise create
    // a symlink(Note: this will not work on Windows).
    auto filePath = path(fileId);
    if (info.path.empty()) {
        info.path = filePath;
    } else {
        // We don't need to check if this is an existing symlink here, as
        // the attempt to create one should report the error string correctly.
        fileutils::createFileLink(filePath, info.path);
    }

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
                    std::lock_guard<std::mutex> lk {pimpl->mapMutex_};
                    auto itO = pimpl->incomings_.find(fileId);
                    if (itO != pimpl->incomings_.end())
                        pimpl->incomings_.erase(itO);
                    if (code == uint32_t(DRing::DataTransferEventCode::finished)) {
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

std::string
TransferManager::path(const std::string& fileId) const
{
    return pimpl_->conversationDataPath_ + DIR_SEPARATOR_STR + fileId;
}

void
TransferManager::onIncomingProfile(const std::shared_ptr<ChannelSocket>& channel)
{
    if (!channel)
        return;

    auto name = channel->name();
    auto lastSep = name.find_last_of('/');
    auto fileId = name.substr(lastSep + 1);

    auto deviceId = channel->deviceId().toString();
    auto cert = channel->peerCertificate();
    if (!cert || !cert->issuer || fileId.find(".vcf") == std::string::npos)
        return;

    auto uri = fileId == "profile.vcf" ? cert->issuer->getId().toString()
                                       : fileId.substr(0, fileId.size() - 4 /*.vcf*/);

    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    auto idx = std::pair<std::string, std::string> {deviceId, uri};
    // Check if not already an incoming file for this id and that we are waiting this file
    auto itV = pimpl_->vcards_.find(idx);
    if (itV != pimpl_->vcards_.end()) {
        channel->shutdown();
        return;
    }

    auto tid = generateUID();
    DRing::DataTransferInfo info;
    info.accountId = pimpl_->accountId_;
    info.conversationId = pimpl_->to_;
    info.path = fileutils::get_cache_dir() + DIR_SEPARATOR_STR + pimpl_->accountId_
                + DIR_SEPARATOR_STR + "vcard" + DIR_SEPARATOR_STR + deviceId + "_" + uri + "_"
                + std::to_string(tid);

    auto ifile = std::make_shared<IncomingFile>(std::move(channel), info, "profile.vcf", "");
    auto res = pimpl_->vcards_.emplace(idx, std::move(ifile));
    if (res.second) {
        res.first->second->onFinished([w = weak(),
                                       uri = std::move(uri),
                                       deviceId = std::move(deviceId),
                                       accountId = pimpl_->accountId_,
                                       cert = std::move(cert),
                                       path = info.path](uint32_t code) {
            // schedule destroy transfer as not needed
            dht::ThreadPool().computation().run([w,
                                                 uri = std::move(uri),
                                                 deviceId = std::move(deviceId),
                                                 accountId = std::move(accountId),
                                                 cert = std::move(cert),
                                                 path = std::move(path),
                                                 code] {
                if (auto sthis_ = w.lock()) {
                    auto& pimpl = sthis_->pimpl_;
                    std::lock_guard<std::mutex> lk {pimpl->mapMutex_};
                    auto itO = pimpl->vcards_.find({deviceId, uri});
                    if (itO != pimpl->vcards_.end())
                        pimpl->vcards_.erase(itO);
                    if (code == uint32_t(DRing::DataTransferEventCode::finished)) {
                        emitSignal<DRing::ConfigurationSignal::ProfileReceived>(accountId,
                                                                                uri,
                                                                                path);
                    }
                }
            });
        });
        res.first->second->process();
    }
}

std::string
TransferManager::profilePath(const std::string& contactId) const
{
    // TODO iOS?
    return fmt::format("{}/{}.vcf", pimpl_->profilesPath_, base64::encode(contactId));
}

std::vector<WaitingRequest>
TransferManager::waitingRequests() const
{
    std::vector<WaitingRequest> res;
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
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
    std::lock_guard<std::mutex> lk(pimpl_->mapMutex_);
    return pimpl_->waitingIds_.find(fileId) != pimpl_->waitingIds_.end();
}

} // namespace jami
