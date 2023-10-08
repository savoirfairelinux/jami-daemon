/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#pragma once

#include "jami/datatransfer_interface.h"
#include "noncopyable.h"

#include <dhtnet/multiplexed_socket.h>

#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace jami {

libjami::DataTransferId generateUID();

class Stream;

struct IncomingFileInfo
{
    libjami::DataTransferId id;
    std::shared_ptr<Stream> stream;
};

struct WaitingRequest
{
    std::string fileId;
    std::string interactionId;
    std::string sha3sum;
    std::string path;
    std::size_t totalSize;
    MSGPACK_DEFINE(fileId, interactionId, sha3sum, path, totalSize)
};

typedef std::function<void(const std::string&)> InternalCompletionCb;
typedef std::function<void()> OnFinishedCb;

class FileInfo
{
public:
    FileInfo(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
             const std::string& fileId,
             const std::string& interactionId,
             const libjami::DataTransferInfo& info);
    virtual ~FileInfo() {}
    virtual void process() = 0;
    std::shared_ptr<dhtnet::ChannelSocket> channel() const { return channel_; }
    libjami::DataTransferInfo info() const { return info_; }
    virtual void cancel() = 0;
    void onFinished(std::function<void(uint32_t)>&& cb) { finishedCb_ = std::move(cb); }
    void emit(libjami::DataTransferEventCode code);

protected:
    std::atomic_bool isUserCancelled_ {false};
    std::string fileId_ {};
    std::string interactionId_ {};
    libjami::DataTransferInfo info_ {};
    std::shared_ptr<dhtnet::ChannelSocket> channel_ {};
    std::function<void(uint32_t)> finishedCb_ {};
};

class IncomingFile : public FileInfo, public std::enable_shared_from_this<IncomingFile>
{
public:
    IncomingFile(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                 const libjami::DataTransferInfo& info,
                 const std::string& fileId,
                 const std::string& interactionId,
                 const std::string& sha3Sum = "");
    ~IncomingFile();
    void process() override;
    void cancel() override;

private:
    std::weak_ptr<IncomingFile> weak()
    {
        return std::static_pointer_cast<IncomingFile>(shared_from_this());
    }
    std::ofstream stream_;
    std::string sha3Sum_ {};
};

class FileBufferInputStream;
class OutgoingFile : public FileInfo
{
public:
    OutgoingFile(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                 const std::string& fileId,
                 const std::string& interactionId,
                 const libjami::DataTransferInfo& info,
                 size_t start = 0,
                 size_t end = 0);
    ~OutgoingFile();
    void process() override;
    void cancel() override;

private:
    std::unique_ptr<FileBufferInputStream> stream_;
};

class TransferManager : public std::enable_shared_from_this<TransferManager>
{
public:
    TransferManager(const std::string& accountId, const std::string& to);
    ~TransferManager();

    /**
     * Send a file to a channel
     * @param channel       channel to use
     * @param fileId        fileId of the transfer
     * @param interactionId interactionId of the transfer
     * @param path          path of the file
     * @param start         start offset
     * @param end           end
     */
    void transferFile(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                      const std::string& fileId,
                      const std::string& interactionId,
                      const std::string& path,
                      size_t start = 0,
                      size_t end = 0,
                      OnFinishedCb onFinished = {});

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
     * @param id              of the transfer
     * @param interactionId   linked interaction
     * @param sha3sum         attended sha3sum
     * @param path            where the file will be downloaded
     * @param total           total size of the file
     */
    void waitForTransfer(const std::string& fileId,
                         const std::string& interactionId,
                         const std::string& sha3sum,
                         const std::string& path,
                         std::size_t total);

    /**
     * Handle incoming transfer
     * @param id        Related id
     * @param channel   Related channel
     */
    void onIncomingFileTransfer(const std::string& fileId,
                                const std::shared_ptr<dhtnet::ChannelSocket>& channel);

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
    bool isWaiting(const std::string& fileId) const;
    void onIncomingProfile(const std::shared_ptr<dhtnet::ChannelSocket>& channel, const std::string& sha3Sum = "");

    /**
     * @param contactId     contact's id
     * @return where profile.vcf is stored
     */
    std::string profilePath(const std::string& contactId) const;

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
