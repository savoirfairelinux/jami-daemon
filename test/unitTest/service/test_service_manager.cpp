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

#include "jamidht/service_manager.h"
#include "jamidht/svc_protocol.h"
#include "uri.h"

#include <filesystem>
#include <fstream>
#include <set>

#include "../../test_runner.h"

namespace jami {
namespace test {

class ServiceManagerTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "service_manager"; }

    void setUp() override
    {
        tmpDir_ = std::filesystem::temp_directory_path()
                  / ("jami_svcmgr_test_" + std::to_string(::getpid()) + "_"
                     + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(tmpDir_);
    }
    void tearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(tmpDir_, ec);
    }

private:
    std::filesystem::path tmpDir_;

    static ServiceRecord makeRec(const std::string& name = "web", uint16_t port = 8080)
    {
        ServiceRecord r;
        r.name = name;
        r.localHost = "127.0.0.1";
        r.localPort = port;
        return r;
    }

    void uuid_format_test();
    void uuid_uniqueness_test();
    void uri_scheme_test();
    void add_get_remove_test();
    void add_invalid_record_test();
    void update_test();
    void persistence_test();
    void auth_public_test();
    void auth_contacts_only_test();
    void auth_specific_contacts_test();
    void auth_disabled_test();
    void visible_services_test();
    void protocol_query_roundtrip_test();
    void protocol_response_roundtrip_test();
    void protocol_peek_type_test();
    void protocol_peek_version_test();
    void protocol_version_mismatch_roundtrip_test();

    CPPUNIT_TEST_SUITE(ServiceManagerTest);
    CPPUNIT_TEST(uuid_format_test);
    CPPUNIT_TEST(uuid_uniqueness_test);
    CPPUNIT_TEST(uri_scheme_test);
    CPPUNIT_TEST(add_get_remove_test);
    CPPUNIT_TEST(add_invalid_record_test);
    CPPUNIT_TEST(update_test);
    CPPUNIT_TEST(persistence_test);
    CPPUNIT_TEST(auth_public_test);
    CPPUNIT_TEST(auth_contacts_only_test);
    CPPUNIT_TEST(auth_specific_contacts_test);
    CPPUNIT_TEST(auth_disabled_test);
    CPPUNIT_TEST(visible_services_test);
    CPPUNIT_TEST(protocol_query_roundtrip_test);
    CPPUNIT_TEST(protocol_response_roundtrip_test);
    CPPUNIT_TEST(protocol_peek_type_test);
    CPPUNIT_TEST(protocol_peek_version_test);
    CPPUNIT_TEST(protocol_version_mismatch_roundtrip_test);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ServiceManagerTest, ServiceManagerTest::name());

void
ServiceManagerTest::uuid_format_test()
{
    auto u = generateServiceUuid();
    CPPUNIT_ASSERT_EQUAL(size_t {36}, u.size());
    CPPUNIT_ASSERT_EQUAL('-', u[8]);
    CPPUNIT_ASSERT_EQUAL('-', u[13]);
    CPPUNIT_ASSERT_EQUAL('-', u[18]);
    CPPUNIT_ASSERT_EQUAL('-', u[23]);
    CPPUNIT_ASSERT_EQUAL('4', u[14]); // version 4
    CPPUNIT_ASSERT(u[19] == '8' || u[19] == '9' || u[19] == 'a' || u[19] == 'b'); // variant
}

void
ServiceManagerTest::uuid_uniqueness_test()
{
    std::set<std::string> seen;
    for (int i = 0; i < 1000; ++i)
        seen.insert(generateServiceUuid());
    CPPUNIT_ASSERT_EQUAL(size_t {1000}, seen.size());
}

void
ServiceManagerTest::uri_scheme_test()
{
    Uri u1("svcdisc://query");
    CPPUNIT_ASSERT(u1.scheme() == Uri::Scheme::SVC_DISCOVERY);
    CPPUNIT_ASSERT_EQUAL(std::string("//query"), u1.authority());

    Uri u2("svc://abc-123");
    CPPUNIT_ASSERT(u2.scheme() == Uri::Scheme::SVC_TUNNEL);
    CPPUNIT_ASSERT_EQUAL(std::string("//abc-123"), u2.authority());

    // Round-trip via toString preserves the scheme.
    Uri u3(u2.toString());
    CPPUNIT_ASSERT(u3.scheme() == Uri::Scheme::SVC_TUNNEL);
}

void
ServiceManagerTest::add_get_remove_test()
{
    ServiceManager m(tmpDir_);
    CPPUNIT_ASSERT(m.getServices().empty());

    auto id = m.addService(makeRec("web", 8080));
    CPPUNIT_ASSERT(!id.empty());
    CPPUNIT_ASSERT_EQUAL(size_t {36}, id.size()); // UUID length

    auto rec = m.getService(id);
    CPPUNIT_ASSERT(rec.has_value());
    CPPUNIT_ASSERT_EQUAL(std::string("web"), rec->name);
    CPPUNIT_ASSERT_EQUAL(uint16_t {8080}, rec->localPort);
    CPPUNIT_ASSERT(rec->enabled);
    CPPUNIT_ASSERT(rec->policy == AccessPolicy::CONTACTS_ONLY);

    CPPUNIT_ASSERT_EQUAL(size_t {1}, m.getServices().size());

    CPPUNIT_ASSERT(m.removeService(id));
    CPPUNIT_ASSERT(!m.getService(id).has_value());
    CPPUNIT_ASSERT(!m.removeService(id));
}

void
ServiceManagerTest::add_invalid_record_test()
{
    ServiceManager m(tmpDir_);
    // Missing name
    auto r1 = makeRec("", 8080);
    CPPUNIT_ASSERT(m.addService(r1).empty());
    // Missing port
    auto r2 = makeRec("web", 0);
    CPPUNIT_ASSERT(m.addService(r2).empty());
    CPPUNIT_ASSERT(m.getServices().empty());
}

void
ServiceManagerTest::update_test()
{
    ServiceManager m(tmpDir_);
    auto id = m.addService(makeRec("web", 8080));
    auto rec = *m.getService(id);
    rec.name = "renamed";
    rec.localPort = 9090;
    rec.enabled = false;
    CPPUNIT_ASSERT(m.updateService(rec));
    auto fetched = m.getService(id);
    CPPUNIT_ASSERT(fetched.has_value());
    CPPUNIT_ASSERT_EQUAL(std::string("renamed"), fetched->name);
    CPPUNIT_ASSERT_EQUAL(uint16_t {9090}, fetched->localPort);
    CPPUNIT_ASSERT(!fetched->enabled);

    // Update with bogus id fails.
    ServiceRecord bogus = rec;
    bogus.id = generateServiceUuid();
    CPPUNIT_ASSERT(!m.updateService(bogus));
}

void
ServiceManagerTest::persistence_test()
{
    std::string id;
    {
        ServiceManager m(tmpDir_);
        id = m.addService(makeRec("game", 27015));
        auto rec = *m.getService(id);
        rec.policy = AccessPolicy::SPECIFIC_CONTACTS;
        rec.allowedContacts = {"alice", "bob"};
        rec.description = "Counter-Strike";
        m.updateService(rec);
    }
    // Reopen and verify everything is restored.
    {
        ServiceManager m(tmpDir_);
        auto svcs = m.getServices();
        CPPUNIT_ASSERT_EQUAL(size_t {1}, svcs.size());
        const auto& r = svcs[0];
        CPPUNIT_ASSERT_EQUAL(id, r.id);
        CPPUNIT_ASSERT_EQUAL(std::string("game"), r.name);
        CPPUNIT_ASSERT_EQUAL(std::string("Counter-Strike"), r.description);
        CPPUNIT_ASSERT_EQUAL(uint16_t {27015}, r.localPort);
        CPPUNIT_ASSERT(r.policy == AccessPolicy::SPECIFIC_CONTACTS);
        CPPUNIT_ASSERT_EQUAL(size_t {2}, r.allowedContacts.size());
    }
}

void
ServiceManagerTest::auth_public_test()
{
    ServiceManager m(tmpDir_);
    auto rec = makeRec();
    rec.policy = AccessPolicy::PUBLIC;
    auto id = m.addService(rec);
    auto contactChecker = [](const std::string&) { return false; };
    CPPUNIT_ASSERT(m.isAuthorized(id, "anyone", contactChecker));
    CPPUNIT_ASSERT(m.isAuthorized(id, "", contactChecker));
}

void
ServiceManagerTest::auth_contacts_only_test()
{
    ServiceManager m(tmpDir_);
    auto rec = makeRec();
    rec.policy = AccessPolicy::CONTACTS_ONLY;
    auto id = m.addService(rec);
    auto contactChecker = [](const std::string& uri) { return uri == "alice"; };
    CPPUNIT_ASSERT(m.isAuthorized(id, "alice", contactChecker));
    CPPUNIT_ASSERT(!m.isAuthorized(id, "bob", contactChecker));
    CPPUNIT_ASSERT(!m.isAuthorized(id, "alice", nullptr));
}

void
ServiceManagerTest::auth_specific_contacts_test()
{
    ServiceManager m(tmpDir_);
    auto rec = makeRec();
    rec.policy = AccessPolicy::SPECIFIC_CONTACTS;
    rec.allowedContacts = {"alice", "carol"};
    auto id = m.addService(rec);
    auto contactChecker = [](const std::string&) { return true; }; // doesn't matter
    CPPUNIT_ASSERT(m.isAuthorized(id, "alice", contactChecker));
    CPPUNIT_ASSERT(m.isAuthorized(id, "carol", contactChecker));
    CPPUNIT_ASSERT(!m.isAuthorized(id, "bob", contactChecker));
}

void
ServiceManagerTest::auth_disabled_test()
{
    ServiceManager m(tmpDir_);
    auto rec = makeRec();
    rec.policy = AccessPolicy::PUBLIC;
    rec.enabled = false;
    auto id = m.addService(rec);
    auto contactChecker = [](const std::string&) { return true; };
    CPPUNIT_ASSERT(!m.isAuthorized(id, "alice", contactChecker));
    CPPUNIT_ASSERT(!m.isAuthorized("nonexistent", "alice", contactChecker));
}

void
ServiceManagerTest::visible_services_test()
{
    ServiceManager m(tmpDir_);
    auto pub = makeRec("pub", 80);
    pub.policy = AccessPolicy::PUBLIC;
    m.addService(pub);

    auto contactsOnly = makeRec("priv", 81);
    contactsOnly.policy = AccessPolicy::CONTACTS_ONLY;
    m.addService(contactsOnly);

    auto specific = makeRec("alice-only", 82);
    specific.policy = AccessPolicy::SPECIFIC_CONTACTS;
    specific.allowedContacts = {"alice"};
    m.addService(specific);

    auto disabled = makeRec("off", 83);
    disabled.policy = AccessPolicy::PUBLIC;
    disabled.enabled = false;
    m.addService(disabled);

    auto bobIsContact = [](const std::string& u) { return u == "bob"; };
    auto bobVisible = m.getVisibleServices("bob", bobIsContact);
    CPPUNIT_ASSERT_EQUAL(size_t {2}, bobVisible.size());

    auto aliceVisible = m.getVisibleServices("alice", bobIsContact);
    CPPUNIT_ASSERT_EQUAL(size_t {2}, aliceVisible.size());

    auto strangerVisible = m.getVisibleServices("eve", bobIsContact);
    CPPUNIT_ASSERT_EQUAL(size_t {1}, strangerVisible.size());
    CPPUNIT_ASSERT_EQUAL(std::string("pub"), strangerVisible[0].name);
}

// --- Wire protocol -----------------------------------------------------------

template<typename T>
static msgpack::object_handle pack_unpack(const T& src)
{
    msgpack::sbuffer buf;
    msgpack::pack(buf, src);
    return msgpack::unpack(buf.data(), buf.size());
}

void
ServiceManagerTest::protocol_query_roundtrip_test()
{
    svc_protocol::SvcDiscQuery q;
    auto oh = pack_unpack(q);
    svc_protocol::SvcDiscQuery decoded;
    oh.get().convert(decoded);
    CPPUNIT_ASSERT_EQUAL(svc_protocol::kMaxVersion, decoded.v);
    CPPUNIT_ASSERT_EQUAL(std::string(svc_protocol::MsgType::kQuery), decoded.type);
}

void
ServiceManagerTest::protocol_response_roundtrip_test()
{
    svc_protocol::SvcDiscResponse resp;
    resp.services.push_back({"id-a", "web",  "the web", "tcp"});
    resp.services.push_back({"id-b", "game", "",        "tcp"});
    auto oh = pack_unpack(resp);

    svc_protocol::SvcDiscResponse decoded;
    oh.get().convert(decoded);
    CPPUNIT_ASSERT_EQUAL(std::string(svc_protocol::MsgType::kServiceList), decoded.type);
    CPPUNIT_ASSERT_EQUAL(size_t {2}, decoded.services.size());
    CPPUNIT_ASSERT_EQUAL(std::string("id-a"), decoded.services[0].id);
    CPPUNIT_ASSERT_EQUAL(std::string("web"), decoded.services[0].name);
    CPPUNIT_ASSERT_EQUAL(std::string("the web"), decoded.services[0].description);
    CPPUNIT_ASSERT_EQUAL(std::string("tcp"), decoded.services[0].proto);
    CPPUNIT_ASSERT_EQUAL(std::string("game"), decoded.services[1].name);
    CPPUNIT_ASSERT(decoded.services[1].description.empty());
}

void
ServiceManagerTest::protocol_peek_type_test()
{
    {
        auto oh = pack_unpack(svc_protocol::SvcDiscQuery{});
        CPPUNIT_ASSERT_EQUAL(std::string("query"), svc_protocol::peekType(oh.get()));
    }
    {
        svc_protocol::SvcDiscError err;
        err.code = 403;
        err.message = "no";
        auto oh = pack_unpack(err);
        CPPUNIT_ASSERT_EQUAL(std::string("error"), svc_protocol::peekType(oh.get()));
    }
    {
        auto oh = pack_unpack(svc_protocol::SvcDiscResponse{});
        CPPUNIT_ASSERT_EQUAL(std::string("service_list"), svc_protocol::peekType(oh.get()));
    }
}

void
ServiceManagerTest::protocol_peek_version_test()
{
    svc_protocol::SvcDiscQuery q;
    q.v = 7;
    auto oh = pack_unpack(q);
    CPPUNIT_ASSERT_EQUAL(uint8_t {7}, svc_protocol::peekVersion(oh.get()));
}

void
ServiceManagerTest::protocol_version_mismatch_roundtrip_test()
{
    svc_protocol::SvcDiscVersionMismatch vm;
    vm.max_supported = 1;
    auto oh = pack_unpack(vm);
    svc_protocol::SvcDiscVersionMismatch decoded;
    oh.get().convert(decoded);
    CPPUNIT_ASSERT_EQUAL(std::string("version_mismatch"), decoded.type);
    CPPUNIT_ASSERT_EQUAL(uint8_t {1}, decoded.max_supported);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ServiceManagerTest::name());
