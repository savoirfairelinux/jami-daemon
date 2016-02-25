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
#include "security/tls_session.h"

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

static DRing::DataConnectionId
get_unique_connection_id()
{
    static std::atomic<DRing::DataConnectionId> uid {0};
    uid += 1;
    return uid;
}

static DRing::DataTransferId
get_unique_transfer_id()
{
    static std::atomic<DRing::DataTransferId> uid {0};
    uid += 1;
    return uid;
}

using DataConnectionMap = std::map<DRing::DataConnectionId, std::weak_ptr<DataConnection>>;
static DataConnectionMap&
get_data_connection_map()
{
    static DataConnectionMap map;
    return map;
}

using DataTransferMap = std::map<DRing::DataTransferId, std::weak_ptr<DataTransfer>>;
static DataTransferMap&
get_data_transfer_map()
{
    static DataTransferMap map;
    return map;
}

//==================================================================================================

DataConnection::Packet::Packet(const void* bytes, std::size_t size)
{
    const auto pkt_size = sizeof(PacketHeader) + size;
    data_.reserve(pkt_size);
    data_.resize(pkt_size);
    std::copy_n(reinterpret_cast<const uint8_t*>(bytes), size, data_.data() + sizeof(PacketHeader));
}

DataConnection::Packet::Packet(const vec& v)
{
    const auto pkt_size = sizeof(PacketHeader) + v.size();
    data_.reserve(pkt_size);
    data_.resize(pkt_size);
    std::copy_n(reinterpret_cast<const uint8_t*>(v.data()), v.size(),
                data_.data() + sizeof(PacketHeader));
}

//==================================================================================================

TxQueue::~TxQueue()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void
TxQueue::init(tls::TlsSession* tls)
{
    tls_ = tls;
    thread_ = std::thread(&TxQueue::threadJob, this);
}

void
TxQueue::send(const DataConnection::Packet& pkt)
{
    std::unique_lock<std::mutex> lk {txMutex_};

    tls_->async_send(pkt.data(), pkt.size(), [this](std::size_t res) {
            std::lock_guard<std::mutex> lk(txMutex_);
            result_ = res;
            txCv_.notify_one();
        });

    auto t1 = clock::now();
    txCv_.wait(lk);

    if (gnutls_error_is_fatal(result_))
        throw std::runtime_error("fatal TLS error");
}

TxQueue::clock::time_point
TxQueue::nextScheduleTimePoint()
{
    return clock::now();
}

void
TxQueue::threadJob()
{
    while (running_) {
        std::unique_lock<std::mutex> lk {jMutex_};

        // wait for something to do
        if (senders_.empty()) {
            jCv_.wait(lk, [this]{ return !running_ or !senders_.empty(); });
            if (!running_)
                return;
            RING_ERR("empty: %u", senders_.empty());
            continue;
        }
        RING_ERR("lol");

        // wait until a sender has something to send
        auto when = nextScheduleTimePoint();
        lk.unlock();
        std::this_thread::sleep_until(when);

        auto sender = senders_.front();
        DataConnection::Packet pkt;
        if (sender->popPacket(pkt)) {
            send(std::move(pkt));
        }
        senders_.pop();
    }
}

//==================================================================================================

#if 0
std::shared_ptr<DataConnection>
DataConnection::makeDataConnection(SecureIceTransport& tr)
{
    auto cnx = std::make_shared<DataConnection>(tr);
    auto& map = get_data_connection_map();
    map.insert(cnx->id_, cnx);
    return cnx;
}

std::shared_ptr<DataConnection>
DataConnection::getDataConnection(const DRing::DataConnectionId& tid)
{
    auto& map = get_data_connection_map();
    auto iter = map.find(tid);
    if (iter == map.cend())
        return {};
    if (auto dt = iter->second.lock())
        return dt;
    map.erase(iter);
    return {};
}

DataConnection::DataConnection(IceTransaction& tr)
    : id_(get_unique_connection_id())
    , transport_(tr)
{
    info_.account = tr->account_id;
    info_.peer = tr->peer_id;
    info_.code = Dring::DataTransferCode::CODE_UNKNOWN;
}

bool
DataConnection::addTransfer(std::shared_ptr<DataTransfer> dtrx)
{
    std::lock_guard<std::mutex> lk {chanMutex_};

    // Find a free ChannelId
    ChannelId cid = 0;
    while (cid < channelIdPool_.size() && channelIdPool_.test(cididx)) {
      ++cid;
    }
    if (cid >= channelIdPool_.size()) {
        RING_ERR("[dc:%s] no more channel ids", id_.c_str());
        return false;
    }
    channelIdPool_.set(cid);
    return transferMap_.emplace(cid, std::move(dtrx))->second;
}

void
DataConnection::connected()
{
    client_ = transport_.client;
    if (client_)
        client_->onConnected(this);
}

void
DataConnection::disconnected()
{
    if (client_) {
        client_->onDisconnected();
        client_.reset();
    }
}

bool
DataConnection::send(Packet& pkt)
{
    std::unique_lock<std::mutex> lk {txMutex_};
    txPkt_ = std::move(pkt);
    auto& header = txPkt_.getHeader();
    header.seq = htonl(++lastTxSeq_);
    RING_DBG("tx seq = %zu", lastTxSeq_);
    txAck_ = false;

    int tries = 10;
    do {
        auto ret = transport_.send(txPkt_.data(), txPkt_.size(),
                                   [](std::size_t s){ RING_WARN("[data] %zu bytes sent", s); });
        if (ret < 0)
            return false;

        // Congestion layer :O)
        txCv_.wait_for(lk, std::chrono::milliseconds(200));
        if (txAck_)
            break;
        // Stop after 60s
        if (++tries >= 300) {
            RING_ERR("[data] peer lost");
            return false;
        }
    } while (not txAck_);

    return true;
}

void
DataConnection::txPktAck(const PacketHeader& head)
{
    PacketHeader ack = head;
    ack.type = 1;
    RING_DBG("tx ack seq = %zu", ntohl(ack.seq));
    transport_.send(&ack, sizeof(ack), [](std::size_t){});
}

void
DataConnection::onRxData(const std::vector<uint8_t>& buf)
{
    // [0] = protocol version, Only version 0 is supported (draft)
    if (buf.empty() or buf[0] != 0 or buf.size() < sizeof(PacketHeader))
        return;

    const auto head = reinterpret_cast<const PacketHeader*>(buf.data());
    RING_DBG("Rx: v=%u t=%u, c=%u, s=%u", head->version, head->type, head->channel, ntohl(head->seq));

    switch (head->type) {
        case 0: rxPktData(buf.data(), buf.size()); break;
        case 1: rxPktAck(buf.data(), buf.size()); break;
    }
}

void
DataConnection::rxPktData(const uint8_t* buf, std::size_t size)
{
    const auto& header = *reinterpret_cast<const PacketHeader*>(buf);
    if (ntohl(header.seq) == lastRxSeq_ + 1) {
        ++lastRxSeq_;
        if (client_)
            client_->onRxData(buf + sizeof(header), size - sizeof(header));
    }
    txPktAck(header);
}

void
DataConnection::rxPktAck(const uint8_t* buf, std::size_t size)
{
    std::lock_guard<std::mutex> lk {txMutex_};
    const auto& rx_header = *reinterpret_cast<const PacketHeader*>(buf);
    const auto& tx_header = txPkt_.getHeader();

    // OutOfOrder packet not handled yet
    if (rx_header.seq == tx_header.seq) {
        txAck_ = true;
        txCv_.notify_one();
    } else {
        RING_WARN("%u %u", ntohl(rx_header.seq), ntohl(tx_header.seq));
    }
}

//==================================================================================================

DataTransfer::DataTransfer(const DRing::DataConnectionId& connectionId, const std::string& name)
    : id_(get_unique_transfer_id())
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

#if 0
void
FileSender::onConnected(* transport)
{
    RING_WARN("[ftp-tx:%s] connected", id_.c_str());
    DataTransfer::onConnected(transport);
}
#endif

void
FileSender::onDisconnected()
{
    RING_WARN("[ftp-tx:%s] disconnected", id_.c_str());
    thread_.stop();
    DataTransfer::onDisconnected();
}

void
FileSender::onFataError(DRing::DataTransferCode code)
{
    setStatus(error);
    thread_.stop();
}

void
FileSender::process()
{
    std::array<uint8_t, 512> buf;

    // Init stream
    if (not initStream()) {
        onFataError(DRing::DataTransferCode::CODE_INTERNAL);
        return;
    }

    // Wait for accept msg
    while (thread_.isRunning()) {
        // TODO: remove me, is not safe at all!!
        thread_.wait([this]{ return not go_.empty(); });
    }

    setStatus(DRing::DataTransferCode::CODE_PROGRESSING);

    RING_ERR("go = '%s'", go_.c_str());
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
FileReceiver::onConnected(DataXfertEndPoint* transport)
{
    RING_WARN("[ftp-rx:%s] connected", id_.c_str());
    DataTransfer::onConnected(transport);
}

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
    if (msg.substr(5) == "HELLO")
        setStatus(DRing::DataTransferCode::CODE_NOTIFYING);
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
#endif

} // namespace ring
