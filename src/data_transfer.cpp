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

#include "data_transfer.h"
#include "manager.h"
#include "client/ring_signal.h"
#include "string_utils.h"

#include <random>
#include <algorithm>
#include <array>

namespace ring {

template<typename... Args>
inline void
emitDataXferStatus(Args... args)
{
    emitSignal<DRing::DataTransferSignal::DataTransferStatus>(args...);
}

static DRing::DataTransferId
get_unique_id()
{
    static std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream accId;
    accId << std::hex << dist(Manager::instance().getRandomEngine());
    return accId.str();
}

using DataTransferMap = std::map<DRing::DataTransferId, std::weak_ptr<DataTransfer>>;
static DataTransferMap&
get_data_transfer_map()
{
    static DataTransferMap map;
    return map;
}

//==================================================================================================

DataXfertEndPoint::Packet::Packet(const void* bytes, std::size_t size)
{
    const auto pkt_size = sizeof(PacketHeader) + size;
    data_.reserve(pkt_size);
    std::copy_n(reinterpret_cast<const uint8_t*>(bytes), size, data_.data() + sizeof(PacketHeader));
    data_.resize(pkt_size);
}

//==================================================================================================

DataTransfer::DataTransfer(const DRing::DataConnectionId& connectionId, const std::string& name)
    : id_(get_unique_id())
    , info_()
{
    info_.connectionId = connectionId;
    info_.name = name;
    info_.size = -1;
    info_.code = DRing::DataTransferCode::CODE_UNKNOWN;
}

DataTransfer::~DataTransfer()
{
    if (info_.code > 0 and ((info_.code / 100) < 2 or info_.code == DRing::DataTransferCode::CODE_ACCEPTED))
        emitDataXferStatus(id_, DRing::DataTransferCode::CODE_SERVICE_UNAVAILABLE);
}

void
DataTransfer::setStatus(DRing::DataTransferCode code)
{
    {
        std::lock_guard<std::mutex> lk(infoMutex_);
        info_.code = code;
    }
    emitDataXferStatus(id_, code);
}

void
DataTransfer::getInfo(DRing::DataTransferInfo& info) const
{
    std::lock_guard<std::mutex> lk(infoMutex_);
    info = info_;
}

std::streamsize
DataTransfer::getCount() const
{
    std::lock_guard<std::mutex> lk(infoMutex_);
    return bytes_sent_;
}

std::shared_ptr<DataTransfer>
DataTransfer::getDataTransfer(const DRing::DataTransferId& tid)
{
    auto& map = get_data_transfer_map();
    auto iter = map.find(tid);
    if (iter == map.cend())
        return {};
    if (auto dt = iter->second.lock())
        return dt;
    map.erase(iter);
    return {};
}

//==================================================================================================

std::shared_ptr<FileSender>
FileSender::newFileSender(const DRing::DataConnectionId& cid, const std::string& name,
                          std::ifstream&& stream)
{
    auto ptr = std::shared_ptr<FileSender>(new FileSender(cid, name, std::move(stream)));
    get_data_transfer_map()[ptr->getId()] = ptr;
    return ptr;
}

FileSender::FileSender(const DRing::DataConnectionId& cid, const std::string& name,
                       std::ifstream&& stream)
    : DataTransfer(cid, name)
    , stream_(std::move(stream))
    , thread_ ([]{ return true; }, [this]{ process(); }, []{})
{
    // get length of file:
    stream_.seekg(0, stream_.end);
    info_.size = stream_.tellg();
    stream_.seekg(0, stream_.beg);
}

FileSender::~FileSender()
{
    thread_.join();
}

void
FileSender::onConnected(DataXfertEndPoint* transport)
{
    RING_WARN("[ftp-tx:%s] connected", id_.c_str());
    DataTransfer::onConnected(transport);
    thread_.start();
}

void
FileSender::onDisconnected()
{
    RING_WARN("[ftp-tx:%s] disconnected", id_.c_str());
    thread_.stop();
    DataTransfer::onDisconnected();
}

void
FileSender::process()
{
    std::array<uint8_t, 512> buf;

    setStatus(DRing::DataTransferCode::CODE_PROGRESSING);

    // Init stream
    if (not sendHello())
        goto stop_thread;

    // Wait for accept msg
    while (thread_.isRunning()) {
        // TODO: remove me, is not safe at all!!
        thread_.wait([this]{ return not go_.empty(); });
    }

    if (go_ == "NOGO") {
        setStatus(DRing::DataTransferCode::CODE_UNAUTHORIZED);
        goto stop_thread;
    }

    while (not thread_.isRunning() and bytes_sent_ <= info_.size) {
        stream_.read(reinterpret_cast<char*>(buf.data()), buf.size());
        if (auto read_size = stream_.gcount()) {
            RING_DBG("[ftp-tx:%s] %zu bytes read", id_.c_str(), stream_.gcount());
            if (sendData(buf.data(), read_size)) {
                std::lock_guard<std::mutex> lk(infoMutex_);
                bytes_sent_ += read_size;
            } else {
                setStatus(DRing::DataTransferCode::CODE_INTERNAL);
                break;
            }
        } else {
            RING_WARN("[ftp-tx:%s] eof", id_.c_str());
            sendEof(); // no error check
            setStatus(DRing::DataTransferCode::CODE_OK);
            break;
        }
    } while (true);

stop_thread:
    thread_.stop();
}

bool
FileSender::sendHello()
{
    auto msg = "hello|" + to_string(info_.size) + "|" + info_.name;
    DataXfertEndPoint::Packet::vec payload {msg.cbegin(), msg.cend()};
    DataXfertEndPoint::Packet pkt {payload.data(), payload.size()};
    return transport_->send(pkt);
}

bool
FileSender::sendEof()
{
    auto msg = "eof:" + info_.name;
    DataXfertEndPoint::Packet::vec payload {msg.cbegin(), msg.cend()};
    DataXfertEndPoint::Packet pkt {payload.data(), payload.size()};
    return transport_->send(pkt);
}

bool
FileSender::sendData(const uint8_t* buf, std::size_t size)
{
    DataXfertEndPoint::Packet pkt {buf, size};
    return transport_->send(pkt);
}

void
FileSender::onRxData(const void* buf, std::size_t size)
{
    std::string msg {reinterpret_cast<const char*>(buf), size};
    RING_WARN("[ftp-tx:%s] rx msg '%s'", getId().c_str(), msg.c_str());
    if (msg == "GO" or msg == "NOGO") {
        go_ = msg;
        thread_.interrupt();
    }
}

//==================================================================================================

std::shared_ptr<FileReceiver>
FileReceiver::newFileReceiver(const DRing::DataConnectionId& cid, const std::string& name)
{
    auto ptr = std::shared_ptr<FileReceiver>(new FileReceiver(cid, name));
    get_data_transfer_map()[ptr->getId()] = ptr;
    return ptr;
}

FileReceiver::FileReceiver(const DRing::DataConnectionId& cid, const std::string& name)
    : DataTransfer(cid, name)
{}

void
FileReceiver::accept(const std::string& pathname)
{
    std::string msg {"GO"};
    DataXfertEndPoint::Packet::vec payload {msg.cbegin(), msg.cend()};
    DataXfertEndPoint::Packet pkt {payload.data(), payload.size()};
    transport_->send(pkt);
}

void
FileReceiver::onRxData(const void* buf, std::size_t size)
{
    std::string msg {reinterpret_cast<const char*>(buf), size};
    RING_WARN("[ftp-rx:%s] rx msg '%s'", getId().c_str(), msg.c_str());
}

//==================================================================================================

void
acceptFileTransfer(const DRing::DataTransferId& tid, const std::string& pathname)
{
    auto& map = get_data_transfer_map();
    auto iter = map.find(tid);
    if (iter == map.cend())
        return;
    if (auto receiver = std::dynamic_pointer_cast<FileReceiver>(iter->second.lock()))
        receiver->accept(pathname);
}

} // namespace ring
