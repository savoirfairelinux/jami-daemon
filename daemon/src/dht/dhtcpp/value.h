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
 */
struct Value {
    enum class Type : uint8_t {
        Peer,
        IndexDirectory,
        Certificate,
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
    Value(in_port_t port, const sockaddr* ip = nullptr, const InfoHash& owner={}, Id id=INVALID_ID);

    inline bool operator== (const Value& o) {
        return id == o.id && type == o.type && data == o.data;
    }

    bool isExpired(time_t now, time_t t) const {
        time_t elapsed = now - t;
        switch (type) {
        case Type::Certificate:
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

    sockaddr_storage getPeerAddr() const;

    /** print value for debugging */
    friend std::ostream& operator<< (std::ostream& s, const Value& v);

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
