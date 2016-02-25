/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

#include "datatransfer_interface.h"
#include "threadloop.h"
#include "noncopyable.h"

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace ring {

class DataTransfer
{
public:
    DataTransfer(const DRing::DataConnectionId& connectionId, const std::string& name);
    virtual ~DataTransfer();

    DataTransfer(DataTransfer&& o)
        : id_(std::move(o.id_))
        , info_(std::move(o.info_)) {}

    DRing::DataTransferId getId() const noexcept { return id_; }

    void setStatus(DRing::DataTransferCode code);

    virtual void onConnected() {};
    virtual void onDisconnected() {};
    virtual void onRxData(const void*, std::size_t) {};
    virtual void onEof() {};

private:
    NON_COPYABLE(DataTransfer);

    DRing::DataTransferId id_ {}; // unique (in the process scope) data transfer identifier

    std::mutex infoMutex_ {};
    DRing::DataTransferInfo info_ {};
};

class FileSender : public DataTransfer
{
public:
    FileSender(FileSender&& o)
        : DataTransfer(std::move(o))
        , stream_(std::move(o.stream_))
        , thread_(std::move(o.thread_)) {}

    void onConnected() override;
    void onDisconnected() override;

    static std::shared_ptr<FileSender> newFileSender(const DRing::DataConnectionId& connectionId,
                                                     const std::string& name,
                                                     std::ifstream&& stream);

private:
    NON_COPYABLE(FileSender);
    std::ifstream stream_ {};
    ThreadLoop thread_;

    // No public ctors, use newFileSender()
    FileSender(const DRing::DataConnectionId& connectionId, const std::string& name,
               std::ifstream&& stream);
};

class FileReceiver : public DataTransfer
{
public:
    FileReceiver(FileReceiver&& o, const std::string& name)
        : DataTransfer(std::move(o))
        , stream_(std::move(o.stream_)) {}

    void accept(const std::string& pathname);

    static std::shared_ptr<FileReceiver> newFileReceiver(const DRing::DataConnectionId& connectionId,
                                                         const std::string& name);

private:
    NON_COPYABLE(FileReceiver);
    std::ofstream stream_ {};

    // no public ctor, use newFileReceiver()
    FileReceiver(const DRing::DataConnectionId& connectionId, const std::string& name);
};

extern void acceptFileTransfer(const DRing::DataTransferId& tid, const std::string& pathname);

} // namespace ring
