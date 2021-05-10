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

#pragma once

#include "dring/datatransfer_interface.h"
#include "jamidht/multiplexed_socket.h"
#include "noncopyable.h"

#include <memory>
#include <string>
#include <fstream>
#include <optional>

namespace jami {

DRing::DataTransferId generateUID();

class Stream;

struct IncomingFileInfo
{
    DRing::DataTransferId id;
    std::shared_ptr<Stream> stream;
};

struct WaitingRequest
{
    std::string fileId;
    std::string sha3sum;
    std::string path;
    std::size_t totalSize;
    MSGPACK_DEFINE(fileId, sha3sum, path, totalSize)
};

typedef std::function<void(const std::string&)> InternalCompletionCb;
typedef std::function<void(const DRing::DataTransferId&, const DRing::DataTransferEventCode&)>
    OnStateChangedCb;

class FileInfo
{
public:
    FileInfo(const std::shared_ptr<ChannelSocket>& channel,
             const std::string& fileId,
             const DRing::DataTransferInfo& info);
    virtual ~FileInfo() {}
    virtual void process() = 0;
    std::shared_ptr<ChannelSocket> channel() const { return channel_; }
    DRing::DataTransferInfo info() const { return info_; }
    virtual void cancel() = 0;
    void onFinished(std::function<void(uint32_t)>&& cb) { finishedCb_ = std::move(cb); }
    void emit(DRing::DataTransferEventCode code);

protected:
    std::atomic_bool isUserCancelled_ {false};
    std::string fileId_ {};
    DRing::DataTransferInfo info_ {};
    std::shared_ptr<ChannelSocket> channel_ {};
    std::function<void(uint32_t)> finishedCb_ {};
};

class IncomingFile : public FileInfo
{
public:
    IncomingFile(const std::shared_ptr<ChannelSocket>& channel,
                 const DRing::DataTransferInfo& info,
                 const std::string& fileId,
                 const std::string& sha3Sum = "");
    ~IncomingFile();
    void process() override;
    void cancel() override;

private:
    std::ofstream stream_;
    std::string sha3Sum_ {};
};

class OutgoingFile : public FileInfo
{
public:
    OutgoingFile(const std::shared_ptr<ChannelSocket>& channel,
                 const std::string& fileId,
                 const DRing::DataTransferInfo& info,
                 size_t start = 0,
                 size_t end = 0);
    ~OutgoingFile();
    void process() override;
    void cancel() override;

private:
    std::ifstream stream_;
    size_t start_ {0};
    size_t end_ {0};
};

class TransferManager : public std::enable_shared_from_this<TransferManager>
{
public:
    TransferManager(const std::string& accountId, const std::string& to);
    ~TransferManager();

    /**
     * Send a file
     * @param path      of the file
     * @param peer      DeviceId for vcard or dest
     * @param icb       used for internal files (like vcard)
     */
    /*[[deprecated("Non swarm method")]]*/ DRing::DataTransferId sendFile(
        const std::string& path, const std::string& peer, const InternalCompletionCb& icb = {});

    /**
     * Accepts a transfer
     * @param id        of the transfer
     * @param path      of the file
     */
    /*[[deprecated("Non swarm method")]]*/ bool acceptFile(const DRing::DataTransferId& id,
                                                           const std::string& path);

    /**
     * Inform the transfer manager that a new file is incoming
     * @param info      of the transfer
     * @param id        of the transfer
     * @param cb        callback to trigger when connected
     * @param icb       used for vcard
     */
    /*[[deprecated("Non swarm method")]]*/ void onIncomingFileRequest(
        const DRing::DataTransferInfo& info,
        const DRing::DataTransferId& id,
        const std::function<void(const IncomingFileInfo&)>& cb,
        const InternalCompletionCb& icb = {});

    /**
     * Get current transfer infos
     * @param id        of the transfer
     * @param info      to fill
     * @return if found
     */
    /*[[deprecated("Non swarm method")]]*/ bool info(const DRing::DataTransferId& id,
                                                     DRing::DataTransferInfo& info) const noexcept;

    /**
     * Send a file to a channel
     * @param channel       channel to use
     * @param fileId        fileId of the transfer
     * @param path          path of the file
     * @param start         start offset
     * @param end           end
     */
    void transferFile(const std::shared_ptr<ChannelSocket>& channel,
                      const std::string& fileId,
                      const std::string& path,
                      size_t start,
                      size_t end);

    /**
     * Refuse a transfer
     * @param id        of the transfer
     */
    bool cancel(const std::string& fileId);

    /**
     * Get current transfer info
     * @param id        of the transfer
     * @param total     size
     * @param path      path of the file
     * @param progress  current progress
     * @return if found
     */
    bool info(const std::string& fileId,
              std::string& path,
              int64_t& total,
              int64_t& progress) const noexcept;

    /**
     * Inform the transfer manager that a transfer is waited (and will be automatically accepted)
     * @param id        of the transfer
     * @param sha3sum   attended sha3sum
     * @param path      where the file will be downloaded
     * @param total     total size of the file
     */
    void waitForTransfer(const std::string& fileId,
                         const std::string& sha3sum,
                         const std::string& path,
                         std::size_t total);

    /**
     * When a new channel request come for a transfer id
     * @param id        id of the transfer
     * @return if we can accept this transfer
     */
    bool onFileChannelRequest(const std::string& fileId) const;

    /**
     * Handle incoming transfer
     * @param id        Related id
     * @param channel   Related channel
     */
    void onIncomingFileTransfer(const std::string& fileId,
                                const std::shared_ptr<ChannelSocket>& channel);

    /**
     * Retrieve path of a file
     * @param id
     */
    std::string path(const std::string& fileId) const;

    /**
     * Retrieve waiting files
     * @return waiting list
     */
    std::vector<WaitingRequest> waitingRequests() const;
    void onIncomingProfile(const std::shared_ptr<ChannelSocket>& channel);

private:
    std::weak_ptr<TransferManager> weak()
    {
        return std::static_pointer_cast<TransferManager>(shared_from_this());
    }
    NON_COPYABLE(TransferManager);
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
