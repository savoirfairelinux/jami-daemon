/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "client/videomanager.h"
#include "jami.h"
#include "manager.h"

#include "../../test_runner.h"

#include <chrono>

namespace jami::test {

class HeadlessInputsTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "headless_inputs"; }

    HeadlessInputsTest()
    {
        const auto flags = libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_NO_LOCAL_MEDIA
                           | libjami::LIBJAMI_FLAG_NO_AUTOLOAD;
        CPPUNIT_ASSERT(libjami::init(libjami::InitFlag(flags)));
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }

    ~HeadlessInputsTest() override { libjami::fini(); }

    void testBrowserAudioInputAvailableWithoutDevices()
    {
        CPPUNIT_ASSERT(Manager::instance().getVideoManager());
        auto input = getAudioInput("headless-browser-audio");
        CPPUNIT_ASSERT(input);
        auto params = input->switchInput("", AudioInput::SourceMode::RING_BUFFER_ONLY);
        CPPUNIT_ASSERT(params.valid());
        CPPUNIT_ASSERT(params.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
#ifdef ENABLE_VIDEO
        CPPUNIT_ASSERT(!getVideoDeviceMonitor());
        CPPUNIT_ASSERT(!getVideoInput("headless-browser-video"));
#endif
        CPPUNIT_ASSERT(!Manager::instance().getAudioDriver());
    }

    CPPUNIT_TEST_SUITE(HeadlessInputsTest);
    CPPUNIT_TEST(testBrowserAudioInputAvailableWithoutDevices);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(HeadlessInputsTest, HeadlessInputsTest::name());

} // namespace jami::test

CORE_TEST_RUNNER(jami::test::HeadlessInputsTest::name());
