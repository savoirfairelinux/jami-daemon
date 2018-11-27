/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

#include "audio/audiobuffer.h"
#include "dring.h"
#include "videomanager_interface.h"

#include "../../test_runner.h"

namespace ring { namespace test {

class MediaFrameTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_frame"; }

    void setUp();
    void tearDown();

private:
    void testCopy();
    void testMix();

    CPPUNIT_TEST_SUITE(MediaFrameTest);
    CPPUNIT_TEST(testCopy);
    CPPUNIT_TEST(testMix);
    CPPUNIT_TEST_SUITE_END();
};


CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaFrameTest, MediaFrameTest::name());

void
MediaFrameTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
}

void
MediaFrameTest::tearDown()
{
    DRing::fini();
}

void
MediaFrameTest::testCopy()
{
    // test allocation
    DRing::VideoFrame v1;
    v1.reserve(AV_PIX_FMT_YUV420P, 100, 100);
    v1.pointer()->data[0][0] = 42;
    CPPUNIT_ASSERT(v1.pointer());

    // test frame referencing (different pointers, but same data)
    DRing::VideoFrame v2;
    v2.copyFrom(v1);
    CPPUNIT_ASSERT(v1.format() == v2.format());
    CPPUNIT_ASSERT(v1.width() == v2.width());
    CPPUNIT_ASSERT(v1.height() == v2.height());
    CPPUNIT_ASSERT(v1.pointer() != v2.pointer());
    CPPUNIT_ASSERT(v1.pointer()->data[0][0] == 42);
    CPPUNIT_ASSERT(v2.pointer()->data[0][0] == 42);
}

void
MediaFrameTest::testMix()
{
    const AudioFormat& format = AudioFormat::STEREO();
    const int nbSamples = format.sample_rate / 50;
    auto a1 = std::make_unique<DRing::AudioFrame>(format, nbSamples);
    a1->pointer()->extended_data[0][0] = 1;
    auto a2 = std::make_unique<DRing::AudioFrame>(format, nbSamples);
    a2->pointer()->extended_data[0][0] = 3;
    auto a3 = std::make_unique<DRing::AudioFrame>();
    a3->copyFrom(*a2);
    CPPUNIT_ASSERT(a3->pointer()->extended_data[0][0] == 3);
    a3->mix(*a1);
    CPPUNIT_ASSERT(a3->pointer()->extended_data[0][0] == 4);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::MediaFrameTest::name());
