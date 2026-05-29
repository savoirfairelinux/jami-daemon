/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "media/network_sim/network_simulator.h"
#include "media/network_sim/network_sim_registry.h"

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <chrono>
#include <cmath>
#include <thread>

namespace jami {
namespace test {

class NetworkSimulatorTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "NetworkSimulator"; }

private:
    void testDisabledPassesAll();
    void testPacketLoss();
    void testBandwidthLimit();
    void testRegistry();
    void testStatsReset();

    CPPUNIT_TEST_SUITE(NetworkSimulatorTest);
    CPPUNIT_TEST(testDisabledPassesAll);
    CPPUNIT_TEST(testPacketLoss);
    CPPUNIT_TEST(testBandwidthLimit);
    CPPUNIT_TEST(testRegistry);
    CPPUNIT_TEST(testStatsReset);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(NetworkSimulatorTest, NetworkSimulatorTest::name());

void
NetworkSimulatorTest::testDisabledPassesAll()
{
    NetworkSimulator sim;
    // Disabled by default
    CPPUNIT_ASSERT(!sim.isEnabled());

    // All packets should pass when disabled
    for (int i = 0; i < 1000; i++) {
        CPPUNIT_ASSERT(sim.shouldSend(100));
    }

    auto stats = sim.getStats();
    // When disabled, no stats are recorded (packets bypass the sim)
    CPPUNIT_ASSERT_EQUAL(uint64_t(0), stats.packetsDropped);
}

void
NetworkSimulatorTest::testPacketLoss()
{
    NetworkSimulator sim;
    sim.setEnabled(true);
    sim.setPacketLoss(0.5f); // 50% loss

    int passed = 0;
    int dropped = 0;
    const int iterations = 10000;

    for (int i = 0; i < iterations; i++) {
        if (sim.shouldSend(100))
            passed++;
        else
            dropped++;
    }

    // With 50% loss over 10000 packets, we expect roughly 5000 each
    // Allow 5% tolerance
    float observedLoss = static_cast<float>(dropped) / iterations;
    CPPUNIT_ASSERT(observedLoss > 0.45f);
    CPPUNIT_ASSERT(observedLoss < 0.55f);

    // Check stats match
    auto stats = sim.getStats();
    CPPUNIT_ASSERT_EQUAL(static_cast<uint64_t>(passed), stats.packetsSent);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint64_t>(dropped), stats.packetsDropped);

    // Check observed loss API
    float apiLoss = sim.getObservedPacketLoss();
    CPPUNIT_ASSERT(std::fabs(apiLoss - observedLoss) < 0.001f);
}

void
NetworkSimulatorTest::testBandwidthLimit()
{
    NetworkSimulator sim;
    sim.setEnabled(true);
    sim.setBandwidthLimit(8000); // 8000 bps = 1000 bytes/sec

    // Send a burst of 10 packets of 200 bytes each (2000 bytes total)
    // Token bucket starts at 1 second (1000 bytes), so after 1000 bytes
    // the bucket empties and further packets should be dropped
    int passed = 0;
    for (int i = 0; i < 10; i++) {
        if (sim.shouldSend(200))
            passed++;
    }

    // Should have passed about 5 packets (1000 bytes / 200 bytes)
    CPPUNIT_ASSERT(passed >= 4);
    CPPUNIT_ASSERT(passed <= 6);

    // Wait for bucket to refill (1 second)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // Should be able to send again
    CPPUNIT_ASSERT(sim.shouldSend(200));
}

void
NetworkSimulatorTest::testRegistry()
{
    auto& reg = NetworkSimRegistry::instance();
    reg.clear();

    auto sim1 = reg.getOrCreate("call-123", "audio");
    auto sim2 = reg.getOrCreate("call-123", "video");
    auto sim3 = reg.getOrCreate("call-123", "audio"); // same as sim1

    CPPUNIT_ASSERT(sim1 != nullptr);
    CPPUNIT_ASSERT(sim2 != nullptr);
    CPPUNIT_ASSERT(sim1 == sim3); // Same instance returned
    CPPUNIT_ASSERT(sim1 != sim2); // Different streams get different instances

    // Lookup existing
    auto found = reg.get("call-123", "audio");
    CPPUNIT_ASSERT(found == sim1);

    // Lookup non-existing
    auto notFound = reg.get("call-999", "audio");
    CPPUNIT_ASSERT(notFound == nullptr);

    // Remove
    reg.remove("call-123", "audio");
    auto afterRemove = reg.get("call-123", "audio");
    CPPUNIT_ASSERT(afterRemove == nullptr);

    // Video still there
    auto stillThere = reg.get("call-123", "video");
    CPPUNIT_ASSERT(stillThere == sim2);

    reg.clear();
}

void
NetworkSimulatorTest::testStatsReset()
{
    NetworkSimulator sim;
    sim.setEnabled(true);
    sim.setPacketLoss(0.0f);

    sim.shouldSend(100);
    sim.shouldSend(200);

    auto stats = sim.getStats();
    CPPUNIT_ASSERT_EQUAL(uint64_t(2), stats.packetsSent);
    CPPUNIT_ASSERT_EQUAL(uint64_t(300), stats.bytesSent);

    sim.resetStats();

    stats = sim.getStats();
    CPPUNIT_ASSERT_EQUAL(uint64_t(0), stats.packetsSent);
    CPPUNIT_ASSERT_EQUAL(uint64_t(0), stats.bytesSent);
}

} // namespace test
} // namespace jami
