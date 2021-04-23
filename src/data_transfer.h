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
#include <optional>

namespace jami {

// TODO move def
DRing::DataTransferId generateUID();

class Stream;

struct IncomingFileInfo
{
    DRing::DataTransferId id;
    std::shared_ptr<Stream> stream;
};

struct WaitingRequest
{
    std::string interactionId;
    std::string sha3sum;
    std::string path;
    MSGPACK_DEFINE(interactionId, sha3sum, path)
};

typedef std::function<void(const std::string&)> InternalCompletionCb;
typedef std::function<bool(const std::string&)> OnVerifyCb;
typedef std::function<void(const DRing::DataTransferId&, const DRing::DataTransferEventCode&)>
    OnStateChangedCb;

class FileInfo
{
public:
    virtual void process() = 0;
    std::shared_ptr<ChannelSocket> channel() const { return channel_; }
    DRing::DataTransferInfo info() const;

protected:
    DRing::DataTransferInfo info_ {};
    std::shared_ptr<ChannelSocket> channel_ {};
};

class IncomingFile : public FileInfo
{
public:
    void process() override {}
};

class OutgoingFile : public FileInfo
{
public:
    void process() override {}
};

class TransferManager
{
public:
    TransferManager(const std::string& accountId, const std::string& to, bool isConversation = true);
    ~TransferManager();

    /**
     * Send a file
     * @param path      of the file
     * @param icb       used for internal files (like vcard)
     * @param deviceId  if we only want to transmit to one device
     * @param resendId  if we need to resend a file, just specify previous id there.
     */
    [[deprecated("Non swarm method")]] DRing::DataTransferId sendFile(
        const std::string& path,
        const InternalCompletionCb& icb = {},
        const std::string& deviceId = {},
        DRing::DataTransferId resendId = {0});

    /**
     * Accepts a transfer
     * @param id        of the transfer
     * @param path      of the file
     */
    [[deprecated("Non swarm method")]] bool acceptFile(const DRing::DataTransferId& id,
                                                       const std::string& path);

    /**
     * Refuse a transfer
     * @param id        of the transfer
     */
    bool cancel(const DRing::DataTransferId& id);

    /**
     * Get current transfer infos
     * @param id        of the transfer
     * @param info      to fill
     * @return if found
     */
    bool info(const DRing::DataTransferId& id, DRing::DataTransferInfo& info) const noexcept;

    /**
     * Get current transfer progress
     * @param id        of the transfer
     * @param total     size
     * @param progress  current progress
     * @return if found
     */
    bool bytesProgress(const DRing::DataTransferId& id,
                       int64_t& total,
                       int64_t& progress) const noexcept;

    /**
     * Inform the transfer manager that a new file is incoming
     * @param info      of the transfer
     * @param id        of the transfer
     * @param cb        callback to trigger when connected
     * @param icb       used for vcard
     */
    void onIncomingFileRequest(const DRing::DataTransferInfo& info,
                               const DRing::DataTransferId& id,
                               const std::function<void(const IncomingFileInfo&)>& cb,
                               const InternalCompletionCb& icb = {});

    /**
     * Inform the transfer manager that a transfer is waited (and will be automatically accepted)
     * @param id             of the transfer
     * @param interactionId  of the transfer in the confirmation
     * @param sha3sum        attended sha3sum
     * @param path           where the file will be downloaded
     */
    void waitForTransfer(const DRing::DataTransferId& id,
                         const std::string& interactionId,
                         const std::string& sha3sum,
                         const std::string& path);

    bool acceptIncomingChannel(const DRing::DataTransferId& id) const;
    void handleChannel(const DRing::DataTransferId& id,
                       const std::shared_ptr<ChannelSocket>& channel);

    std::vector<WaitingRequest> waitingRequests() const; 

private:
    NON_COPYABLE(TransferManager);
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
