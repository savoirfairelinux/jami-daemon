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

#include <atomic>
#include <filesystem>
#include <random>

#include "../../test_runner.h"

namespace jami {
namespace test {

class ServiceUpdateTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "service_update"; }

    void setUp() override
    {
        tmpDir_ = std::filesystem::temp_directory_path()
                  / ("jami_svcupd_test_" + std::to_string(::getpid()) + "_"
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
    std::mt19937_64 rng_ {std::random_device {}()};

    static ServiceRecord makeRec(const std::string& name = "web", uint16_t port = 8080)
    {
        ServiceRecord r;
        r.name = name;
        r.localHost = "127.0.0.1";
        r.localPort = port;
        return r;
    }

    template<typename T>
    static msgpack::object_handle pack_unpack(const T& src)
    {
        msgpack::sbuffer buf;
        msgpack::pack(buf, src);
        return msgpack::unpack(buf.data(), buf.size());
    }

    void protocol_service_update_roundtrip_test();
    void protocol_service_update_peek_type_test();
    void on_changed_fires_on_add_test();
    void on_changed_fires_on_update_test();
    void on_changed_fires_on_remove_test();
    void on_changed_not_fired_on_failed_add_test();
    void on_changed_not_fired_on_failed_remove_test();

    CPPUNIT_TEST_SUITE(ServiceUpdateTest);
    CPPUNIT_TEST(protocol_service_update_roundtrip_test);
    CPPUNIT_TEST(protocol_service_update_peek_type_test);
    CPPUNIT_TEST(on_changed_fires_on_add_test);
    CPPUNIT_TEST(on_changed_fires_on_update_test);
    CPPUNIT_TEST(on_changed_fires_on_remove_test);
    CPPUNIT_TEST(on_changed_not_fired_on_failed_add_test);
    CPPUNIT_TEST(on_changed_not_fired_on_failed_remove_test);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ServiceUpdateTest, ServiceUpdateTest::name());

void
ServiceUpdateTest::protocol_service_update_roundtrip_test()
{
    svc_protocol::SvcDiscServiceUpdate update;
    update.device = "abcdef0123456789";
    update.services.push_back({"id-1", "web", "my web server", "tcp", "http"});
    update.services.push_back({"id-2", "ssh", "", "tcp", "ssh"});

    auto oh = pack_unpack(update);
    svc_protocol::SvcDiscServiceUpdate decoded;
    oh.get().convert(decoded);

    CPPUNIT_ASSERT_EQUAL(svc_protocol::MaxVersion, decoded.v);
    CPPUNIT_ASSERT_EQUAL(std::string(svc_protocol::MsgType::ServiceUpdate), decoded.type);
    CPPUNIT_ASSERT_EQUAL(std::string("abcdef0123456789"), decoded.device);
    CPPUNIT_ASSERT_EQUAL(size_t {2}, decoded.services.size());
    CPPUNIT_ASSERT_EQUAL(std::string("id-1"), decoded.services[0].id);
    CPPUNIT_ASSERT_EQUAL(std::string("web"), decoded.services[0].name);
    CPPUNIT_ASSERT_EQUAL(std::string("my web server"), decoded.services[0].description);
    CPPUNIT_ASSERT_EQUAL(std::string("tcp"), decoded.services[0].proto);
    CPPUNIT_ASSERT_EQUAL(std::string("http"), decoded.services[0].scheme);
    CPPUNIT_ASSERT_EQUAL(std::string("id-2"), decoded.services[1].id);
    CPPUNIT_ASSERT_EQUAL(std::string("ssh"), decoded.services[1].name);
}

void
ServiceUpdateTest::protocol_service_update_peek_type_test()
{
    svc_protocol::SvcDiscServiceUpdate update;
    auto oh = pack_unpack(update);
    CPPUNIT_ASSERT_EQUAL(std::string_view("service_update"), svc_protocol::peekType(oh.get()));
}

void
ServiceUpdateTest::on_changed_fires_on_add_test()
{
    ServiceManager m(tmpDir_);
    std::atomic<int> count {0};
    m.setOnChanged([&count]() { ++count; });

    m.addService(makeRec("web", 8080), rng_);
    CPPUNIT_ASSERT_EQUAL(1, count.load());

    m.addService(makeRec("ssh", 22), rng_);
    CPPUNIT_ASSERT_EQUAL(2, count.load());
}

void
ServiceUpdateTest::on_changed_fires_on_update_test()
{
    ServiceManager m(tmpDir_);
    auto id = m.addService(makeRec("web", 8080), rng_);

    std::atomic<int> count {0};
    m.setOnChanged([&count]() { ++count; });

    auto rec = *m.getService(id);
    rec.name = "renamed";
    CPPUNIT_ASSERT(m.updateService(rec));
    CPPUNIT_ASSERT_EQUAL(1, count.load());
}

void
ServiceUpdateTest::on_changed_fires_on_remove_test()
{
    ServiceManager m(tmpDir_);
    auto id = m.addService(makeRec("web", 8080), rng_);

    std::atomic<int> count {0};
    m.setOnChanged([&count]() { ++count; });

    CPPUNIT_ASSERT(m.removeService(id));
    CPPUNIT_ASSERT_EQUAL(1, count.load());
}

void
ServiceUpdateTest::on_changed_not_fired_on_failed_add_test()
{
    ServiceManager m(tmpDir_);
    std::atomic<int> count {0};
    m.setOnChanged([&count]() { ++count; });

    // Invalid record (empty name) should not fire callback.
    auto bad = makeRec("", 8080);
    CPPUNIT_ASSERT(m.addService(bad, rng_).empty());
    CPPUNIT_ASSERT_EQUAL(0, count.load());

    // Invalid record (port 0) should not fire callback.
    auto bad2 = makeRec("web", 0);
    CPPUNIT_ASSERT(m.addService(bad2, rng_).empty());
    CPPUNIT_ASSERT_EQUAL(0, count.load());
}

void
ServiceUpdateTest::on_changed_not_fired_on_failed_remove_test()
{
    ServiceManager m(tmpDir_);
    std::atomic<int> count {0};
    m.setOnChanged([&count]() { ++count; });

    // Removing nonexistent service should not fire callback.
    CPPUNIT_ASSERT(!m.removeService("nonexistent-id"));
    CPPUNIT_ASSERT_EQUAL(0, count.load());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ServiceUpdateTest::name());
