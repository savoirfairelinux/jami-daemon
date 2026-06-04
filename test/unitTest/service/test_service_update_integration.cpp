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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "../../test_runner.h"
#include "../common.h"
#include "account_const.h"
#include "jami.h"
#include "jami/account_const.h"
#include "jami/configurationmanager_interface.h"
#include "jami/networkservice_interface.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/service_manager.h"
#include "manager.h"

using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

/**
 * End-to-end test for the service-update push mechanism:
 *  - Alice and Bob become mutual contacts.
 *  - Bob waits for the initial proactive service discovery to complete.
 *  - Alice adds/updates/removes services.
 *  - Bob receives unsolicited PeerServicesReceived signals (requestId==0)
 *    with the updated service list each time.
 */
class ServiceUpdateIntegrationTest : public CppUnit::TestFixture
{
public:
    ServiceUpdateIntegrationTest()
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ServiceUpdateIntegrationTest() { libjami::fini(); }

    static std::string name() { return "ServiceUpdateIntegration"; }
    void setUp() override;
    void tearDown() override;

private:
    void testPushOnAdd();
    void testPushOnUpdate();
    void testPushOnRemove();

    CPPUNIT_TEST_SUITE(ServiceUpdateIntegrationTest);
    CPPUNIT_TEST(testPushOnAdd);
    CPPUNIT_TEST(testPushOnUpdate);
    CPPUNIT_TEST(testPushOnRemove);
    CPPUNIT_TEST_SUITE_END();

    // Wait until Bob receives a PeerServicesReceived signal (requestId==0)
    // that satisfies the given predicate on the JSON payload.
    // Returns the services JSON on success, or asserts on timeout.
    std::string waitForPush(std::function<bool(const std::string&)> pred, std::chrono::seconds timeout = 60s);

    // Establish the contact relationship and wait for initial service discovery.
    void establishContacts();

    std::string aliceId;
    std::string bobId;
    std::string aliceUri;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers_;

    // State for PeerServicesReceived signals.
    struct PushEvent
    {
        uint32_t requestId;
        int status;
        std::string servicesJson;
    };
    std::vector<PushEvent> pushEvents_;
    size_t pushConsumed_ {0};

    // State for contact establishment.
    bool bobReceivedRequest_ {false};
    bool aliceContactAdded_ {false};
    bool bobContactAdded_ {false};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ServiceUpdateIntegrationTest, ServiceUpdateIntegrationTest::name());

void
ServiceUpdateIntegrationTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];

    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bob = Manager::instance().getAccount<JamiAccount>(bobId);
    CPPUNIT_ASSERT(alice && bob);
    aliceUri = alice->getUsername();

    pushEvents_.clear();
    pushConsumed_ = 0;
    bobReceivedRequest_ = false;
    aliceContactAdded_ = false;
    bobContactAdded_ = false;

    // Register signal handler for PeerServicesReceived on Bob's account.
    handlers_.insert(libjami::exportable_callback<libjami::ServiceSignal::PeerServicesReceived>(
        [&](uint32_t requestId,
            const std::string& accountId,
            const std::string& /*peerId*/,
            int status,
            const std::string& servicesJson) {
            if (accountId != bobId)
                return;
            std::lock_guard lk(mtx_);
            pushEvents_.push_back({requestId, status, servicesJson});
            cv_.notify_all();
        }));

    handlers_.insert(libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& accountId, const std::string&, const std::string&, const std::vector<uint8_t>&, time_t) {
            std::lock_guard lk(mtx_);
            if (accountId == bobId)
                bobReceivedRequest_ = true;
            cv_.notify_all();
        }));
    handlers_.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactAdded>(
        [&](const std::string& accountId, const std::string&, bool confirmed) {
            std::lock_guard lk(mtx_);
            if (confirmed) {
                if (accountId == aliceId)
                    aliceContactAdded_ = true;
                if (accountId == bobId)
                    bobContactAdded_ = true;
            }
            cv_.notify_all();
        }));

    libjami::registerSignalHandlers(handlers_);

    establishContacts();
}

void
ServiceUpdateIntegrationTest::tearDown()
{
    // Remove any leftover services.
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    if (alice && alice->hasServiceManager()) {
        for (const auto& svc : alice->serviceManager().getServices())
            alice->serviceManager().removeService(svc.id);
    }

    libjami::unregisterSignalHandlers();
    handlers_.clear();
    wait_for_removal_of({aliceId, bobId});
}

void
ServiceUpdateIntegrationTest::establishContacts()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bob = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bob->getUsername();

    alice->addContact(bobUri);
    alice->sendTrustRequest(bobUri, {});
    {
        std::unique_lock lk(mtx_);
        CPPUNIT_ASSERT(cv_.wait_for(lk, 60s, [&] { return bobReceivedRequest_; }));
    }
    CPPUNIT_ASSERT(bob->acceptTrustRequest(aliceUri));
    {
        std::unique_lock lk(mtx_);
        CPPUNIT_ASSERT(cv_.wait_for(lk, 60s, [&] { return aliceContactAdded_ && bobContactAdded_; }));
    }

    // Wait for proactive discovery (requestId==0) to complete. This ensures
    // the svcdisc channel is established between the two peers.
    {
        std::unique_lock lk(mtx_);
        CPPUNIT_ASSERT(cv_.wait_for(lk, 60s, [&] {
            for (const auto& ev : pushEvents_) {
                if (ev.requestId == 0 && ev.status == static_cast<int>(libjami::ServiceSignal::PeerServicesStatus::OK))
                    return true;
            }
            return false;
        }));
    }
    // Consume all events so far (initial proactive discovery).
    {
        std::lock_guard lk(mtx_);
        pushConsumed_ = pushEvents_.size();
    }
}

std::string
ServiceUpdateIntegrationTest::waitForPush(std::function<bool(const std::string&)> pred, std::chrono::seconds timeout)
{
    std::unique_lock lk(mtx_);
    std::string result;
    CPPUNIT_ASSERT(cv_.wait_for(lk, timeout, [&] {
        for (size_t i = pushConsumed_; i < pushEvents_.size(); ++i) {
            const auto& ev = pushEvents_[i];
            if (ev.requestId == 0 && ev.status == static_cast<int>(libjami::ServiceSignal::PeerServicesStatus::OK)
                && pred(ev.servicesJson)) {
                result = ev.servicesJson;
                pushConsumed_ = i + 1;
                return true;
            }
        }
        return false;
    }));
    return result;
}

void
ServiceUpdateIntegrationTest::testPushOnAdd()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    CPPUNIT_ASSERT(alice);

    // Alice adds a PUBLIC service — Bob should receive a push with it.
    ServiceRecord rec;
    rec.name = "pushed-svc";
    rec.description = "a pushed service";
    rec.localHost = "127.0.0.1";
    rec.localPort = 9999;
    rec.policy = AccessPolicy::PUBLIC;
    auto serviceId = alice->serviceManager().addService(rec, alice->rand);
    CPPUNIT_ASSERT(!serviceId.empty());

    auto json = waitForPush([&](const std::string& j) -> bool { return j.find(serviceId) != std::string::npos; });
    CPPUNIT_ASSERT(json.find("\"name\":\"pushed-svc\"") != std::string::npos);
    CPPUNIT_ASSERT(json.find("\"description\":\"a pushed service\"") != std::string::npos);

    // Cleanup
    alice->serviceManager().removeService(serviceId);
}

void
ServiceUpdateIntegrationTest::testPushOnUpdate()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    CPPUNIT_ASSERT(alice);

    ServiceRecord rec;
    rec.name = "update-me";
    rec.localHost = "127.0.0.1";
    rec.localPort = 7777;
    rec.policy = AccessPolicy::PUBLIC;
    auto serviceId = alice->serviceManager().addService(rec, alice->rand);
    CPPUNIT_ASSERT(!serviceId.empty());

    // Wait for the add push to arrive.
    waitForPush([&](const std::string& j) -> bool {
        return j.find(serviceId) != std::string::npos && j.find("\"name\":\"update-me\"") != std::string::npos;
    });

    // Now update the service name.
    auto fetched = alice->serviceManager().getService(serviceId);
    CPPUNIT_ASSERT(fetched.has_value());
    auto updated = *fetched;
    updated.name = "updated-name";
    CPPUNIT_ASSERT(alice->serviceManager().updateService(updated));

    // Bob should receive a push with the new name.
    auto json = waitForPush([&](const std::string& j) -> bool {
        return j.find(serviceId) != std::string::npos && j.find("\"name\":\"updated-name\"") != std::string::npos;
    });
    CPPUNIT_ASSERT(json.find("\"name\":\"updated-name\"") != std::string::npos);

    // Cleanup
    alice->serviceManager().removeService(serviceId);
}

void
ServiceUpdateIntegrationTest::testPushOnRemove()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    CPPUNIT_ASSERT(alice);

    ServiceRecord rec;
    rec.name = "remove-me";
    rec.localHost = "127.0.0.1";
    rec.localPort = 6666;
    rec.policy = AccessPolicy::PUBLIC;
    auto serviceId = alice->serviceManager().addService(rec, alice->rand);
    CPPUNIT_ASSERT(!serviceId.empty());

    // Wait for add push.
    waitForPush([&](const std::string& j) -> bool { return j.find(serviceId) != std::string::npos; });

    // Remove the service.
    CPPUNIT_ASSERT(alice->serviceManager().removeService(serviceId));

    // Bob should receive a push where the service is absent.
    auto json = waitForPush([&](const std::string& j) -> bool { return j.find(serviceId) == std::string::npos; });
    // The service ID must not appear in the received list.
    CPPUNIT_ASSERT(json.find(serviceId) == std::string::npos);
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::ServiceUpdateIntegrationTest::name())
