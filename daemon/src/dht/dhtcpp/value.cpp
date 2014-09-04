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

Value::Value(in_port_t port, const sockaddr* ip, const InfoHash& owner, Id id)
 : owner(owner), id(id), type(Type::Peer)
 {
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


sockaddr_storage
Value::getPeerAddr() const
{
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


std::ostream& operator<< (std::ostream& s, const Value& v)
{
    s << "Value[id:" << std::hex << v.id << std::dec << " ";
    if (v.isSigned())
        s << "signed ";
    if (v.isEncrypted())
        s << "encrypted ";
    switch (v.type) {
    case Value::Type::Peer:
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
    case Value::Type::Certificate: {
        s << "Certificate";
        try {
            InfoHash h = SecureDht::Certificate(v.data).getPublicKey().getId();
            s << " with ID " << h;
        } catch (const std::exception& e) {
            s << " (invalid)";
        }
        break;
    }
    case Value::Type::UserData:
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

}