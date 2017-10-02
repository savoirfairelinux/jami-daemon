/*
 *  Copyright (C) 2017 Savoir-Faire Linux Inc.
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

#include <sys/socket.h>
#include <sys/unistd.h>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>

using namespace ring;

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

private:
    int sock_ {-1};
};

void
test_TURN::testSimpleConnection()
{
    TurnTransportParams param;
    param.server = IpAddr {"turn.ring.cx"};
    param.realm = "ring";
    param.username = "ring";
    param.password = "ring";
    param.isPeerConnection = true;

    TurnTransport turn {param};
    turn.waitServerReady();

    TCPSocket sock = {param.server.getFamily()};

    // Permit myself
    turn.permitPeer(turn.mappedAddr());
    sock.connect(turn.peerRelayAddr());

    std::string test_data = "Hello, World!";
    sock.send(test_data);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::map<IpAddr, std::vector<char>> streams;
    turn.recvfrom(streams);
    CPPUNIT_ASSERT(streams.size() == 1);

    auto peer_addr = std::begin(streams)->first;
    const auto& vec = std::begin(streams)->second;
    CPPUNIT_ASSERT(std::string(std::begin(vec), std::end(vec)) == test_data);

    turn.recvfrom(streams);
    CPPUNIT_ASSERT(streams.size() == 0);

    turn.sendto(peer_addr, std::vector<char>{1, 2, 3, 4});
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto res = sock.recv(1000);
    CPPUNIT_ASSERT(res.size() == 4);

#if 0
    std::vector<char> big(100000);
    using clock = std::chrono::high_resolution_clock;

    auto t1 = clock::now();
    sock.send(big);
    auto t2 = clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count();
    std::cout << "T= " << duration << "ns"
              << ", V= " << (8000. * big.size() / duration) << "Mb/s"
              << " / " << (1000. * big.size() / duration) << "MB/s"
              << '\n';
    std::this_thread::sleep_for(std::chrono::seconds(5));
#endif
}
