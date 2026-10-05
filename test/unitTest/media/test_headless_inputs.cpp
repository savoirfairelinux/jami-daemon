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

#include "jami.h"
#include "manager.h"
#include "media/audio/audio_input.h"
#include "media/audio/audio_rtp_session.h"
#include "media/system_codec_container.h"
#include "client/videomanager.h"
#include "videomanager_interface.h"

#include "../../test_runner.h"

#include <dhtnet/ice_socket.h>

namespace jami::test {

class HeadlessInputsTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "headless_inputs"; }

    void setUp() override
    {
        const auto flags = libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_NO_LOCAL_MEDIA
                           | libjami::LIBJAMI_FLAG_NO_AUTOLOAD;
        CPPUNIT_ASSERT(libjami::init(libjami::InitFlag(flags)));
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }

    void tearDown() override { libjami::fini(); }

    void testBrowserAudioSenderWithoutLocalMedia()
    {
        MediaDescription send;
        send.enabled = true;
        send.addr = dhtnet::IpAddr("127.0.0.1");
        send.addr.setPort(50000);
        send.rtcp_mux = true;
        send.codec = Manager::instance().getSystemCodecContainer()->searchCodecByPayload(0, MEDIA_AUDIO);
        CPPUNIT_ASSERT(send.codec);

        MediaDescription receive;
        receive.rtcp_mux = true;

        auto session = std::make_shared<AudioRtpSession>("headless-browser",
                                                         "headless-browser-audio",
                                                         nullptr,
                                                         AudioRtpSession::Mode::CONFERENCE_HOST);
        session->updateMedia(send, receive);
        session->start(nullptr, nullptr);
        auto input = session->getAudioLocal();
        CPPUNIT_ASSERT(input);
        CPPUNIT_ASSERT(input->isCapturing());
        CPPUNIT_ASSERT(input->getSourceRingBufferId().empty());
        CPPUNIT_ASSERT(!Manager::instance().getAudioDriver());
#ifdef ENABLE_VIDEO
        CPPUNIT_ASSERT(Manager::instance().getVideoManager());
        CPPUNIT_ASSERT(!getVideoDeviceMonitor());
        CPPUNIT_ASSERT(!getVideoInput("headless-browser-video"));
        CPPUNIT_ASSERT(libjami::openVideoInput("headless-browser-video").empty());
#endif
    }

    CPPUNIT_TEST_SUITE(HeadlessInputsTest);
    CPPUNIT_TEST(testBrowserAudioSenderWithoutLocalMedia);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(HeadlessInputsTest, HeadlessInputsTest::name());

} // namespace jami::test

CORE_TEST_RUNNER(jami::test::HeadlessInputsTest::name());
