/*
Copyright (c) 2014 Savoir-Faire Linux Inc.

Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "value.h"
#include "securedht.h" // print certificate ID

namespace Dht {

std::ostream& operator<< (std::ostream& s, const Value& v)
{
    s << "Value[id:" << std::hex << v.id << std::dec << " ";
    if (v.flags.isSigned())
        s << "signed ";
    if (v.flags.isEncrypted())
        s << "encrypted ";
    if (v.type == ServiceAnnouncement::TYPE.id) {
        s << ServiceAnnouncement(v.data);
    } else if (v.type == SecureDht::CERTIFICATE.id) {
        s << "Certificate";
        try {
            InfoHash h = SecureDht::Certificate(v.data).getPublicKey().getId();
            s << " with ID " << h;
        } catch (const std::exception& e) {
            s << " (invalid)";
        }
    } else {
        s << "Data: ";
        s << std::hex;
        for (size_t i=0; i<v.data.size(); i++)
            s << std::setfill('0') << std::setw(2) << (unsigned)v.data[i];
        s << std::dec;
    }
    s << "]";
    return s;
}

const ValueType ValueType::USER_DATA = {0, "User Data"};

bool
ServiceAnnouncement::storePolicy(std::shared_ptr<Value>& v, InfoHash, const sockaddr* from, socklen_t fromlen)
{
    WARN("ServiceAnnouncement::storePolicy %s", v->toString().c_str());
    ServiceAnnouncement request {};
    request.unpackBlob(v->data);
    if (request.getPort() == 0) {
        ERROR("Announce_values with forbidden port %d.", request.getPort());
        return false;
    }
    ServiceAnnouncement sa_addr {from, fromlen};
    sa_addr.setPort(request.getPort());
    // argument v is modified (not the value).
    v = std::make_shared<Value>(ServiceAnnouncement::TYPE, sa_addr, v->id);
    WARN("ServiceAnnouncement::storePolicy ->  %s", v->toString().c_str());
    return true;
}

const ValueType ServiceAnnouncement::TYPE = {1, "Service Announcement", 15 * 60, ServiceAnnouncement::storePolicy};

void
Value::packToSign(Blob& res) const
{
    res.push_back(flags.to_ulong());
    if (flags.isEncrypted()) {
        res.insert(res.end(), cypher.begin(), cypher.end());
    } else {
        res.insert(res.end(), owner.begin(), owner.end());
        if (flags.haveRecipient())
            res.insert(res.end(), recipient.begin(), recipient.end());
        serialize<ValueType::Id>(type, res);
        serialize<Blob>(data, res);
    }
}

Blob
Value::getToSign() const
{
    Blob ret;
    packToSign(ret);
    return ret;
}

/**
 * Pack part of the data to be encrypted
 */
void
Value::packToEncrypt(Blob& res) const
{
    packToSign(res);
    if (!flags.isEncrypted() && flags.isSigned())
        serialize<Blob>(signature, res);
}

Blob
Value::getToEncrypt() const
{
    Blob ret;
    packToEncrypt(ret);
    return ret;
}

void
Value::pack(Blob& res) const
{
    serialize<Id>(id, res);
    packToEncrypt(res);
}

void
Value::unpackBody(Blob::const_iterator& begin, Blob::const_iterator& end)
{
    flags = {deserialize<uint8_t>(begin, end)};
    if (flags.isEncrypted()) {
        cypher = {begin, end};
        begin = end;
    } else {
        owner = deserialize<InfoHash>(begin, end);
        if (flags.haveRecipient())
            recipient = deserialize<InfoHash>(begin, end);
        type = deserialize<ValueType::Id>(begin, end);
        data = deserialize<Blob>(begin, end);
        if (flags.isSigned())
            signature = deserialize<Blob>(begin, end);
    }
}

void
Value::unpack(Blob::const_iterator& begin, Blob::const_iterator& end)
{
    id = deserialize<Id>(begin, end);
    unpackBody(begin, end);
}

std::ostream& operator<< (std::ostream& s, const ServiceAnnouncement& v)
{
    s << "Peer: ";
    s << "port " << v.getPort();

    if (v.ss.ss_family == AF_INET || v.ss.ss_family == AF_INET6) {
        char hbuf[NI_MAXHOST];
        if (getnameinfo((sockaddr*)&v.ss, sizeof(v.ss), hbuf, sizeof(hbuf), nullptr, 0, NI_NUMERICHOST) == 0) {
            s << " addr " << std::string(hbuf, strlen(hbuf));
        }
    }
}

void
ServiceAnnouncement::pack(Blob& res) const
{
    serialize<in_port_t>(getPort(), res);
    if (ss.ss_family == AF_INET) {
        auto sa4 = reinterpret_cast<const sockaddr_in*>(&ss);
        serialize<in_addr>(sa4->sin_addr, res);
    } else if (ss.ss_family == AF_INET6) {
        auto sa6 = reinterpret_cast<const sockaddr_in6*>(&ss);
        serialize<in6_addr>(sa6->sin6_addr, res);
    }
}

void
ServiceAnnouncement::unpack(Blob::const_iterator& begin, Blob::const_iterator& end)
{
    setPort(deserialize<in_port_t>(begin, end));
    size_t addr_size = end - begin;
    if (addr_size < sizeof(in_addr)) {
        ss.ss_family = 0;
    } else if (addr_size < sizeof(in6_addr)) {
        auto sa4 = reinterpret_cast<sockaddr_in*>(&ss);
        sa4->sin_family = AF_INET;
        sa4->sin_addr = deserialize<in_addr>(begin, end);
    } else {
        auto sa6 = reinterpret_cast<sockaddr_in6*>(&ss);
        sa6->sin6_family = AF_INET6;
        sa6->sin6_addr = deserialize<in6_addr>(begin, end);
    }
}


}