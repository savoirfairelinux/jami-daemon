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
#include "jamidht/p2p.h"

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

static DRing::DataTransferId
generateUID()
{
    thread_local dht::crypto::random_device rd;
    std::uniform_int_distribution<DRing::DataTransferId> dist;
    return dist(rd);
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
    {
        std::lock_guard<std::mutex> lk {infoMutex_};
        info_.lastEvent = code;
    }
    if (stateChangedCb_)
        stateChangedCb_(id, code);
    if (internalCompletionCb_)
        return; // VCard transfer is just for the daemon
    runOnMainThread([id = id, code]() {
        emitSignal<DRing::DataTransferSignal::DataTransferEvent>(id, uint32_t(code));
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

    bool isFinished() const { return info_.lastEvent >= DRing::DataTransferEventCode::finished; }

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
        auto header = fmt::format(
            "Content-Length: {}\n"
            "Display-Name: {}\n"
            "Offset: 0\n\n", info_.totalSize, info_.displayName);
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
            if (internalCompletionCb_)
                internalCompletionCb_(info_.path);

            if (info_.bytesProgress != info_.totalSize)
                emit(DRing::DataTransferEventCode::closed_by_peer);
            else
                emit(DRing::DataTransferEventCode::finished);
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
    fileutils::openStream(input_, info_.path, std::ios::binary);
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
                         const InternalCompletionCb& cb);
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
    IncomingFileTransfer(DRing::DataTransferId,
                         const DRing::DataTransferInfo&,
                         DRing::DataTransferId,
                         const InternalCompletionCb& cb);

    bool start() override;

    void close() noexcept override;

    void requestFilename(const std::function<void(const std::string&)>& cb);

    void accept(const std::string&, std::size_t offset) override;

    bool write(std::string_view data) override;

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

IncomingFileTransfer::IncomingFileTransfer(DRing::DataTransferId tid,
                                           const DRing::DataTransferInfo& info,
                                           DRing::DataTransferId internalId,
                                           const InternalCompletionCb& cb)
    : DataTransfer(tid, cb)
    , internalId_(internalId)
{
    JAMI_WARN() << "[FTP] incoming transfert of " << info.totalSize
                << " byte(s): " << info.displayName;

    info_ = info;
    info_.flags |= (uint32_t) 1 << int(DRing::DataTransferFlags::direction); // incoming
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
        std::string filename = fileutils::get_cache_dir() + DIR_SEPARATOR_STR + std::to_string(id);
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

class DataTransferFacade::Impl
{
public:
    mutable std::mutex mapMutex_;
    std::unordered_map<DRing::DataTransferId, std::shared_ptr<DataTransfer>> map_;

    std::shared_ptr<DataTransfer> createOutgoingFileTransfer(const DRing::DataTransferInfo& info,
                                                             DRing::DataTransferId& tid,
                                                             InternalCompletionCb cb = {});
    std::shared_ptr<IncomingFileTransfer> createIncomingFileTransfer(const DRing::DataTransferInfo&,
                                                                     const DRing::DataTransferId&,
                                                                     InternalCompletionCb cb);
    std::shared_ptr<DataTransfer> getTransfer(const DRing::DataTransferId&);
    void cancel(DataTransfer&);
};

void
DataTransferFacade::Impl::cancel(DataTransfer& transfer)
{
    transfer.close();
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
DataTransferFacade::Impl::createOutgoingFileTransfer(const DRing::DataTransferInfo& info,
                                                     DRing::DataTransferId& tid,
                                                     InternalCompletionCb cb)
{
    tid = generateUID();
    auto transfer = std::make_shared<OutgoingFileTransfer>(tid, info, cb);
    {
        std::lock_guard<std::mutex> lk {mapMutex_};
        map_.emplace(tid, transfer);
    }
    transfer->emit(DRing::DataTransferEventCode::created);
    return transfer;
}

std::shared_ptr<IncomingFileTransfer>
DataTransferFacade::Impl::createIncomingFileTransfer(const DRing::DataTransferInfo& info,
                                                     const DRing::DataTransferId& internal_id,
                                                     InternalCompletionCb cb)
{
    auto tid = generateUID();
    auto transfer = std::make_shared<IncomingFileTransfer>(tid, info, internal_id, cb);
    {
        std::lock_guard<std::mutex> lk {mapMutex_};
        map_.emplace(tid, transfer);
    }
    transfer->emit(DRing::DataTransferEventCode::created);
    return transfer;
}

//==============================================================================

DataTransferFacade::DataTransferFacade()
    : pimpl_ {std::make_unique<Impl>()}
{
    // JAMI_DBG("[XFER] facade created, pimpl @%p", pimpl_.get());
}

DataTransferFacade::~DataTransferFacade()
{
    // JAMI_DBG("[XFER] facade destroy, pimpl @%p", pimpl_.get());
}

std::vector<DRing::DataTransferId>
DataTransferFacade::list() const noexcept
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    return map_utils::extractKeys(pimpl_->map_);
}

DRing::DataTransferError
DataTransferFacade::sendFile(const DRing::DataTransferInfo& info,
                             DRing::DataTransferId& tid,
                             const InternalCompletionCb& cb) noexcept
{
    auto account = Manager::instance().getAccount<JamiAccount>(info.accountId);
    if (!account) {
        JAMI_ERR() << "[XFER] unknown id " << tid;
        return DRing::DataTransferError::invalid_argument;
    }

    if (!fileutils::isFile(info.path)) {
        JAMI_ERR() << "[XFER] invalid filename '" << info.path << "'";
        return DRing::DataTransferError::invalid_argument;
    }

    try {
        pimpl_->createOutgoingFileTransfer(info, tid, cb);
    } catch (const std::exception& ex) {
        JAMI_ERR() << "[XFER] exception during createFileTransfer(): " << ex.what();
        return DRing::DataTransferError::io;
    }

    try {
        // IMPLEMENTATION NOTE: requestPeerConnection() may call the given callback a multiple time.
        // This happen when multiple agents handle communications of the given peer for the given
        // account. Example: Jami account supports multi-devices, each can answer to the request.
        account->requestPeerConnection(
            info.peer,
            tid,
            static_cast<bool>(cb),
            [this, tid](const std::shared_ptr<ChanneledOutgoingTransfer>& out) {
                if (auto transfer = pimpl_->getTransfer(tid))
                    if (out)
                        out->linkTransfer(std::dynamic_pointer_cast<OutgoingFileTransfer>(transfer)
                                              ->startNewOutgoing(out->peer()));
            },
            [this, tid](const std::string& peer) {
                if (auto transfer = std::dynamic_pointer_cast<OutgoingFileTransfer>(
                        pimpl_->getTransfer(tid))) {
                    auto allFinished = transfer->cancelWithPeer(peer);
                    if (allFinished and not transfer->hasBeenStarted()) {
                        transfer->emit(DRing::DataTransferEventCode::unjoinable_peer);
                        pimpl_->cancel(*transfer);
                    }
                }
            });
        return DRing::DataTransferError::success;
    } catch (const std::exception& ex) {
        JAMI_ERR() << "[XFER] exception during sendFile(): " << ex.what();
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
        return DRing::DataTransferError::invalid_argument;
#ifndef _WIN32
    iter->second->accept(file_path, offset);
#else
    iter->second->accept(file_path, offset);
#endif
    return DRing::DataTransferError::success;
}

DRing::DataTransferError
DataTransferFacade::cancel(const DRing::DataTransferId& id) noexcept
{
    if (auto transfer = pimpl_->getTransfer(id)) {
        transfer->cancel();
        pimpl_->cancel(*transfer);
        return DRing::DataTransferError::success;
    }
    return DRing::DataTransferError::invalid_argument;
}

void
DataTransferFacade::close(const DRing::DataTransferId& id) noexcept
{
    std::lock_guard<std::mutex> lk {pimpl_->mapMutex_};
    const auto& iter = pimpl_->map_.find(static_cast<uint64_t>(id));
    if (iter != std::end(pimpl_->map_)) {
        // NOTE: don't erase from map. The client can retrieve
        // related info() to know if the file is finished.
        iter->second->close();
    }
}

DRing::DataTransferError
DataTransferFacade::bytesProgress(const DRing::DataTransferId& id,
                                  int64_t& total,
                                  int64_t& progress) const noexcept
{
    try {
        if (auto transfer = pimpl_->getTransfer(id)) {
            transfer->bytesProgress(total, progress);
            return DRing::DataTransferError::success;
        }
        return DRing::DataTransferError::invalid_argument;
    } catch (const std::exception& ex) {
        JAMI_ERR() << "[XFER] exception during bytesProgress(): " << ex.what();
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
        JAMI_ERR() << "[XFER] exception during info(): " << ex.what();
    }
    return DRing::DataTransferError::unknown;
}

DRing::DataTransferId
DataTransferFacade::createIncomingTransfer(const DRing::DataTransferInfo& info,
                                           const DRing::DataTransferId& internal_id,
                                           const InternalCompletionCb& cb)
{
    auto transfer = pimpl_->createIncomingFileTransfer(info, internal_id, cb);
    if (!transfer)
        return {};
    return transfer->getId();
}

void
DataTransferFacade::onIncomingFileRequest(const DRing::DataTransferId& id,
                                          const std::function<void(const IncomingFileInfo&)>& cb)
{
    if (auto transfer = std::static_pointer_cast<IncomingFileTransfer>(pimpl_->getTransfer(id))) {
        transfer->requestFilename([transfer, id, cb = std::move(cb)](const std::string& filename) {
            if (!filename.empty() && transfer->start())
                cb({id, std::static_pointer_cast<Stream>(transfer)});
            else
                cb({id, nullptr});
        });
    }
}

} // namespace jami
