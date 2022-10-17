/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
 *  Author: Fadi Shehadeh <fadi.shehadeh@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "../../test_runner.h"
#include "jami.h"
#include "../common.h"
#include "jamidht/swarm/routing_table.h"
#include "jamidht/multiplexed_socket.h"

#include "opendht/infohash.h"
#include "peer_connection.h"

using namespace std::string_literals;
using namespace dht;
using NodeId = dht::PkId;

namespace jami {
namespace test {

class RoutingTableTest : public CppUnit::TestFixture
{
public:
    RoutingTableTest();
    static std::string name() { return "RoutingTable"; }

private:
    void testCreateBucket();

    CPPUNIT_TEST_SUITE(RoutingTableTest);
    CPPUNIT_TEST(testCreateBucket);

    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RoutingTableTest, RoutingTableTest::name());

RoutingTableTest::RoutingTableTest()
{
    NodeId node1("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    auto cert1 = dht::crypto::Certificate();
    dht::crypto::Identity local_identity;
    auto it1 = std::make_shared<IceTransport>("test1");
    auto is1 = std::make_unique<IceSocketEndpoint>(nullptr, false);
    auto ps1 = std::make_unique<TlsSocketEndpoint>(is1, local_identity, nullptr, cert1);
    auto ms1 = std::make_shared<MultiplexedSocket>(node1, ps1);
    auto cs1 = std::make_shared<ChannelSocket>(ms1, "n1", 0);
}
/*     IceTransport(const char* name);

    explicit IceSocketEndpoint(std::shared_ptr<IceTransport> ice, bool isSender);

    TlsSocketEndpoint(std::unique_ptr<IceSocketEndpoint>&& tr,
                      const Identity& local_identity,
                      const std::shared_future<tls::DhParams>& dh_params,
                      const dht::crypto::Certificate& peer_cert);

    MultiplexedSocket(const DeviceId& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint);

    ChannelSocket(std::weak_ptr<MultiplexedSocket> endpoint,
                  const std::string& name,
                  const uint16_t& channel,
                  bool isInitiator = false); */

void
RoutingTableTest::testCreateBucket()
{
    NodeId node1("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    Bucket bucket(node1);
    std::cout << node1 << std::endl;
    std::cout << bucket.getLowerLimit().toString();

    CPPUNIT_ASSERT(true);
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::RoutingTableTest::name())

// creer une socket
// ajouter supprimer dans les differents tableaux
// recuperer les informations
