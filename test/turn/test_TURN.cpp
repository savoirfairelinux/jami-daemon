/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

#include "test_TURN.h"
#include "turn_transport.h"

#include <chrono>
#include <opendht/sockaddr.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <thread>
#include <vector>

using namespace jami;

CPPUNIT_TEST_SUITE_REGISTRATION( test_TURN );

class TCPSocket {
public:
    TCPSocket(int fa) {
        sock_ = ::socket(fa, SOCK_STREAM, 0);
        if (sock_ < 0)
            throw std::system_error(errno, std::system_category());
        IpAddr bound {"0.0.0.0"};
        ::bind(sock_, bound, bound.getLength());
    }

    ~TCPSocket() {
        if (sock_ >= 0)
            ::close(sock_);
    }

    void connect(const IpAddr& addr) {
        if (::connect(sock_, addr, addr.getLength()) < 0)
            throw std::system_error(errno, std::system_category());
    }

    void send(const std::string& pkt) {
        if (::send(sock_, pkt.data(), pkt.size(), 0) < 0)
            throw std::system_error(errno, std::system_category());
    }

    void send(const std::vector<char>& pkt) {
        if (::send(sock_, pkt.data(), pkt.size(), 0) < 0)
            throw std::system_error(errno, std::system_category());
    }

    std::vector<char> recv(std::size_t max_len) {
        std::vector<char> pkt(max_len);
        auto rs = ::recv(sock_, pkt.data(), pkt.size(), 0);
        if (rs < 0)
            throw std::system_error(errno, std::system_category());
        pkt.resize(rs);
        pkt.shrink_to_fit();
        return pkt;
    }

    IpAddr address() const {
        struct sockaddr addr;
        socklen_t addrlen;
        if (::getsockname(sock_, &addr, &addrlen) < 0)
            throw std::system_error(errno, std::system_category());
        return IpAddr {addr};
    }

private:
    int sock_ {-1};
};

void
test_TURN::testSimpleConnection()
{
    TurnTransportParams param;
    param.server = IpAddr {"turn.jami.net"};
    param.realm = "ring";
    param.username = "ring";
    param.password = "ring";
    param.isPeerConnection = true;

    TurnTransport turn {param};
    turn.waitServerReady();

    TCPSocket sock = {param.server.getFamily()};

    // Permit myself
    auto mapped = static_cast<dht::SockAddr>(turn.mappedAddr());
    turn.permitPeer(IpAddr { mapped.getMappedIPv4().toString() });
    std::this_thread::sleep_for(std::chrono::seconds(2));

    sock.connect(turn.peerRelayAddr());

    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto peers = turn.peerAddresses();
    CPPUNIT_ASSERT(peers.size() == 1);
    auto remotePeer = peers[0];

    // Peer send data
    std::string test_data = "Hello, World!";
    sock.send(test_data);

    // Client read
    std::vector<char> data(1000);
    turn.recvfrom(remotePeer, data);
    CPPUNIT_ASSERT(std::string(std::begin(data), std::end(data)) == test_data);

    turn.sendto(remotePeer, std::vector<char>{1, 2, 3, 4});

    auto res = sock.recv(1000);
    CPPUNIT_ASSERT(res.size() == 4);

#if 0
    // DISABLED SECTION
    // This code higly load the network and can be long to execute.
    // Only kept for manual testing purpose.
    std::vector<char> big(100000);
    std::size_t count = 1000;
    using clock = std::chrono::high_resolution_clock;

    auto t1 = clock::now();
    auto i = count;
    while (i--)
        sock.send(big);
    auto t2 = clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count();
    std::cout << "T= " << duration << "ns"
              << ", V= " << (8000. * count * big.size() / duration) << "Mb/s"
              << " / " << (1000. * count * big.size() / duration) << "MB/s"
              << '\n';
    std::this_thread::sleep_for(std::chrono::seconds(5));
#endif
}
