/*
 *  Copyright (C) 2023 Savoir-faire Linux Inc.
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

#include <condition_variable>
#include <string>
#include <thread>
#include <restinio/all.hpp>

#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "account_const.h"
#include "common.h"

using namespace libjami::Account;
using namespace restinio;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {


using RestRouter = restinio::router::express_router_t<>;
struct RestRouterTraits : public restinio::default_traits_t
{
    using request_handler_t = RestRouter;
};

class NameDirectoryTest : public CppUnit::TestFixture
{
public:
    NameDirectoryTest()
    {
        // Init daemon
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

        // Create express router for our service.
        auto router = std::make_unique<router::express_router_t<>>();
        router->http_post(
                R"(/name/:name)",
                [](auto req, auto params) {
                    const auto qp = parse_query(req->header().query());
                    return req->create_response()
                            .set_body(
                                    fmt::format("{{\"success\":true}}")
                            )
                            .done();
                });

        router->non_matched_request_handler(
                [](auto req){
                    return req->create_response(restinio::status_not_found()).connection_close().done();
                });


        auto settings = restinio::run_on_this_thread_settings_t<RestRouterTraits>();
        settings.address("localhost");
        settings.port(1412);
        settings.request_handler(std::move(router));
        httpServer_ = std::make_unique<restinio::http_server_t<RestRouterTraits>>(
            Manager::instance().ioContext(),
            std::forward<restinio::run_on_this_thread_settings_t<RestRouterTraits>>(std::move(settings))
        );
        // run http server
        serverThread_ = std::thread([this](){
            httpServer_->open_async([]{/*ok*/}, [](std::exception_ptr ex){
                std::rethrow_exception(ex);
            });
            httpServer_->io_context().run();
        });
    }
    ~NameDirectoryTest() {
        libjami::fini();
        if (serverThread_.joinable())
            serverThread_.join();
    }
    static std::string name() { return "NameDirectory"; }
    void setUp();
    void tearDown();

    std::string aliceId;

    // http server
    std::thread serverThread_;
    std::unique_ptr<restinio::http_server_t<RestRouterTraits>> httpServer_;

private:
    void testRegisterName();

    CPPUNIT_TEST_SUITE(NameDirectoryTest);
    CPPUNIT_TEST(testRegisterName);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(NameDirectoryTest, NameDirectoryTest::name());

void
NameDirectoryTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice.yml");
    aliceId = actors["alice"];
    std::map<std::string, std::string> details;
    details[ConfProperties::RingNS::URI] = "http://localhost:1412";
    libjami::setAccountDetails(aliceId, details);
}

void
NameDirectoryTest::tearDown()
{
    wait_for_removal_of({aliceId});
}

void
NameDirectoryTest::testRegisterName()
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool nameRegistered {false};
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::NameRegistrationEnded>(
        [&](const std::string&,
            int status,
            const std::string&) {
            nameRegistered = status == 0;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    CPPUNIT_ASSERT(libjami::registerName(aliceId, "", "foo"));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return nameRegistered; }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::NameDirectoryTest::name())
