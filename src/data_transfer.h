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
#include <ios>
#include <vector>
#include <condition_variable>
#include <bitset>

namespace ring {

class SecureIceTransport;
class FileSender;
class FileReceiver;
class DataTransfer;

using ChannelId = uint8_t;

template<typename T>
struct Timestamp : T {
    static T get_now() {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<T>(now);
    }

    Timestamp() : T(get_now()) {}

    inline bool newer(const T& other) const {
        return other < *this;
    }

    inline bool older(const T& other) const {
        return other > *this;
    }
};

class DataConnection
{
    friend std::shared_ptr<DataConnection> makeDataConnection(SecureIceTransport& tr);

private:
    struct PacketHeader {
        uint8_t version {0};
        uint8_t type {0};
        uint8_t rsv0 {0};
        uint8_t channel {0};
        uint32_t seq {0}; // Transmit Sequence Number relative the channel
    };

public:
    class Packet {
    public:
        using vec = std::vector<uint8_t>;
        Packet() { data_.resize(sizeof(PacketHeader)); }
        Packet(const void*, std::size_t);
        PacketHeader& getHeader() { return *reinterpret_cast<PacketHeader*>(data_.data()); }
        const PacketHeader& getHeader() const { return *reinterpret_cast<const PacketHeader*>(data_.data()); }
        void* data() noexcept { return data_.data(); }
        const void* data() const noexcept { return data_.data(); }
        std::size_t size() const noexcept { return data_.size(); }
    private:
        std::vector<uint8_t> data_;
    };

    static std::shared_ptr<DataConnection> getDataConnection(const DRing::DataConnectionId& tid);

#if 0
    DataConnection(DataConnection&& o)
        : id_(std::move(o.id_))
        , transport_(o.transport_)
        , info_(std::move(o.info_))
        , channelIdPool_(std::move(o.channelIdPool_)) {}
#endif

    ~DataConnection();

    DRing::DataConnectionId getId() const noexcept { return id_; }
    void changeTransport(SecureIceTransport& transport);
    void enable();
    void disable();

    void setStatus(DRing::DataTransferCode code);
    void getInfo(DRing::DataTransferInfo& info) const;

    void connected();
    void disconnected();
    bool send(Packet& pkt);
    void txPktAck(const PacketHeader& head);
    void onRxData(const std::vector<uint8_t>& buf);
    void rxPktData(const uint8_t* buf, std::size_t size);
    void rxPktAck(const uint8_t* buf, std::size_t size);

private:
    NON_COPYABLE(DataConnection);
    DataConnection(SecureIceTransport&);

    const DRing::DataConnectionId id_;
    SecureIceTransport& transport_;

    mutable std::mutex infoMutex_ {};
    DRing::DataConnectionInfo info_ {};

    // Data Transfers
    std::mutex chanMutex_ {};
    std::bitset<sizeof(ChannelId) * 8> channelIdPool_ {};
    std::map<ChannelId, std::shared_ptr<DataTransfer>> transferMap_ {};

    // Flow control
    uint32_t lastRxSeq_ {0};

    std::mutex txMutex_ {};
    std::condition_variable txCv_ {};
    uint32_t lastTxSeq_ {0};
    bool txAck_ {};
    Packet txPkt_;

    bool addTransfer(std::shared_ptr<DataTransfer> dtrx);
};

std::shared_ptr<DataConnection> makeDataConnection(SecureIceTransport& tr);


#if 0
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
    void getInfo(DRing::DataTransferInfo& info) const;
    std::streamsize getCount() const;

    //virtual void onConnected(DataXfertEndPoint* transport) { transport_ = transport; };
    virtual void onDisconnected() {};
    virtual void onRxData(const void*, std::size_t) {};

    static std::shared_ptr<DataTransfer> getDataTransfer(const DRing::DataTransferId& tid);

protected:
    DRing::DataTransferId id_ {}; // unique (in the process scope) data transfer identifier
    mutable std::mutex infoMutex_ {};
    DRing::DataTransferInfo info_ {};
    std::streamoff bytes_sent_ {0};

private:
    NON_COPYABLE(DataTransfer);
};

class FileSender : public DataTransfer
{
public:
    FileSender(FileSender&& o)
        : DataTransfer(std::move(o))
        , stream_(std::move(o.stream_))
        , thread_(std::move(o.thread_)) {}

    ~FileSender();

    void onConnected(DataXfertEndPoint* transport) override;
    void onDisconnected() override;
    void onRxData(const void*, std::size_t) override;

    static std::shared_ptr<FileSender> newFileSender(const DRing::DataConnectionId& connectionId,
                                                     const std::string& name,
                                                     std::ifstream&& stream);

private:
    NON_COPYABLE(FileSender);
    std::ifstream stream_ {};
    InterruptedThreadLoop thread_;

    // No public ctors, use newFileSender()
    FileSender(const DRing::DataConnectionId& connectionId, const std::string& name,
               std::ifstream&& stream);

    void process();
    bool sendHello();
    bool sendEof();
    bool sendData(const uint8_t* buf, std::size_t size);

    std::mutex mtx_ {};
    std::string go_ {};
};

class FileReceiver : public DataTransfer
{
public:
    FileReceiver(FileReceiver&& o)
        : DataTransfer(std::move(o))
        , stream_(std::move(o.stream_)) {}

    void accept(const std::string& pathname);
    void onConnected(DataXfertEndPoint* transport) override;
    void onRxData(const void*, std::size_t) override;

    static std::shared_ptr<FileReceiver> newFileReceiver(const DRing::DataConnectionId& connectionId,
                                                         const std::string& name);

private:
    NON_COPYABLE(FileReceiver);
    std::ofstream stream_ {};

    // no public ctor, use newFileReceiver()
    FileReceiver(const DRing::DataConnectionId& connectionId, const std::string& name);
};

extern void acceptFileTransfer(const DRing::DataTransferId& tid, const std::string& pathname);

#endif

class FileTransfer {
public:
    static DRing::DataTransferId newFileTransfer(const DRing::DataConnectionId& cid,
                                                 const std::string& name,
                                                 std::ifstream&& stream);
};

} // namespace ring
