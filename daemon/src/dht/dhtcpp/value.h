/*
Copyright (c) 2014 Savoir-Faire Linux Inc.

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

#pragma once

#include "infohash.h"

#include <bitset>
#include <vector>
#include <iostream>

#include <sys/socket.h>
#include <netdb.h>

namespace Dht {

typedef std::vector<uint8_t> Blob;

/**
 * A "value" is data potentially stored on the Dht.
 * It can be an IP:port announced for a service, a public key, or any kind of
 * light user-defined data (recommended: less than 512 bytes).
 * Values are stored at a given InfoHash in the Dht, but also have a
 * unique ID to distinguish between values stored at the same location.
 *
 * @author Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 */
struct Value {
    enum class Type : uint8_t {
        Peer,
        IndexDirectory,
        PublicKey,
        UserData
    };

    typedef uint64_t Id;
    static const Id INVALID_ID {0};

    static const size_t HEADER_LENGTH = sizeof(Id) + sizeof(Type) + 1 /*sizeof(CryptoFlags)*/;

    /**
     * Hold information about how the data is signed/encrypted.
     * Class is final because bitset have no virtual destructor.
     */
    class CryptoFlags final : public std::bitset<2> {
    public:
        using std::bitset<2>::bitset;
        CryptoFlags() {}
        CryptoFlags(bool sign, bool encrypted) : bitset<2>((sign ? 1:0) | (encrypted ? 2:0)) {}
    };

    Value() {}

    /** Generic constructor */
    Value(Type t, const Blob& data, CryptoFlags f={}, const InfoHash& owner={}, Id id=INVALID_ID)
     : owner(owner), id(id), type(t), flags(f), data(data) {}

    /** Custom user data constructor */
    Value(const Blob& userdata) : data(userdata) {}

    /** Peer announce/info constructor
     * port must be in host byte order (sockaddr* port is not considered).
     */
    Value(in_port_t port, const sockaddr* ip = nullptr, const InfoHash& owner={}, Id id=INVALID_ID) : owner(owner), id(id), type(Type::Peer) {
        if (ip && ip->sa_family == AF_INET) {
            data.resize(2+4);
            auto sin = reinterpret_cast<const sockaddr_in*>(ip);
            memcpy(data.data()+2, &sin->sin_addr, 4);
        } else if (ip && ip->sa_family == AF_INET6) {
            data.resize(2+16);
            auto sin = reinterpret_cast<const sockaddr_in6*>(ip);
            memcpy(data.data()+2, &sin->sin6_addr, 16);
        } else {
            data.resize(2);
        }
        *reinterpret_cast<in_port_t*>(data.data()) = port;
    }

    inline bool operator== (const Value& o) {
        return id == o.id && type == o.type && data == o.data;
    }

    bool isExpired(time_t now, time_t t) const {
        time_t elapsed = now - t;
        switch (type) {
        case Type::PublicKey:
            return elapsed > 62 * 60;
        case Type::Peer:
            return elapsed > 32 * 60;
        default:
            return elapsed > 24*60*60*7;
        }
    }
    bool isExpired(time_t now) const {
        return isExpired(now, time);
    }

    inline bool isSigned() const {
        return flags[0];
    }

    inline bool isEncrypted() const {
        return flags[1];
    }

    sockaddr_storage getPeerAddr() const {
        if (type != Type::Peer)
            throw DhtException("Value must be a Peer");
        sockaddr_storage addr {};
        reinterpret_cast<sockaddr_in*>(&addr)->sin_port = htons(*reinterpret_cast<const in_port_t*>(data.data()));
        if (data.size() == 2)
            return addr;
        addr.ss_family = (data.size() == 18) ? AF_INET6 : AF_INET;
        if (addr.ss_family == AF_INET6)
            memcpy(&reinterpret_cast<sockaddr_in6*>(&addr)->sin6_addr, data.data()+2, sizeof(in6_addr));
        else
            memcpy(&reinterpret_cast<sockaddr_in*>(&addr)->sin_addr, data.data()+2, sizeof(in_addr));
        return addr;
    }

    /** print value for debugging */
    friend std::ostream& operator<< (std::ostream& s, const Value& v) {
        s << "Value[id:" << std::hex << v.id << std::dec << " ";
        if (v.isSigned())
            s << "signed ";
        if (v.isEncrypted())
            s << "encrypted ";
        switch (v.type) {
        case Type::Peer:
            s << "Peer: ";
            s << "port " << *reinterpret_cast<const in_port_t*>(v.data.data());
            if (v.data.size() == 6 || v.data.size() == 18) {
                sockaddr_storage sa;
                size_t len;
                sa.ss_family = v.data.size() == 6 ? AF_INET : AF_INET6;
                if (sa.ss_family == AF_INET) {
                    len = sizeof(sockaddr_in);
                    memcpy(&reinterpret_cast<sockaddr_in*>(&sa)->sin_addr, v.data.data()+2, 4);
                }
                else {
                    len = sizeof(sockaddr_in6);
                    memcpy(&reinterpret_cast<sockaddr_in6*>(&sa)->sin6_addr, v.data.data()+2, 16);
                }
                char hbuf[NI_MAXHOST];
                if (getnameinfo((sockaddr*)&sa, len, hbuf, sizeof(hbuf), nullptr, 0, NI_NUMERICHOST) == 0) {
                    s << " addr " << std::string(hbuf, strlen(hbuf));
                }
            }
            break;
        case Type::PublicKey: {
            InfoHash h = InfoHash::get(v.data);
            s << "Public key with ID: " << h;
            break;
        }
        case Type::UserData:
            s << "Data: ";
            //s.write((char*)v.data.data(), v.data.size());
            s << std::hex;
            for (size_t i=0; i<v.data.size(); i++)
                s << std::setfill('0') << std::setw(2) << (unsigned)v.data[i];
            s << std::dec;
            break;
        default:
            break;
        }
        s << "]";
        return s;
    }

    // meta
    InfoHash owner {};
    Id id {INVALID_ID};
    time_t time {0};

    // data
    Type type {Type::UserData};
    CryptoFlags flags {};
    Blob data {};
};

}
