/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include "jami.h"
#include "videomanager_interface.h"

#include "../../test_runner.h"

namespace jami { namespace test {

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
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
}

void
MediaFrameTest::tearDown()
{
    libjami::fini();
}

void
MediaFrameTest::testCopy()
{
    // test allocation
    libjami::VideoFrame v1;
    v1.reserve(AV_PIX_FMT_YUV420P, 100, 100);
    v1.pointer()->data[0][0] = 42;
    CPPUNIT_ASSERT(v1.pointer());

    // test frame referencing (different pointers, but same data)
    libjami::VideoFrame v2;
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
    auto a1 = std::make_unique<libjami::AudioFrame>(format, nbSamples);
    auto d1 = reinterpret_cast<AudioSample*>(a1->pointer()->extended_data[0]);
    d1[0] = 0;
    d1[1] = 1;
    d1[2] = 3;
    d1[3] = -2;
    d1[4] = 5;
    d1[5] = std::numeric_limits<AudioSample>::min();
    d1[6] = std::numeric_limits<AudioSample>::max();
    auto a2 = std::make_unique<libjami::AudioFrame>(format, nbSamples);
    auto d2 = reinterpret_cast<AudioSample*>(a2->pointer()->extended_data[0]);
    d2[0] = 0;
    d2[1] = 3;
    d2[2] = -1;
    d2[3] = 3;
    d2[4] = -6;
    d2[5] = -101;
    d2[6] = 101;
    a2->mix(*a1);
    CPPUNIT_ASSERT(d2[0] == 0);
    CPPUNIT_ASSERT(d2[1] == 4);
    CPPUNIT_ASSERT(d2[2] == 2);
    CPPUNIT_ASSERT(d2[3] == 1);
    CPPUNIT_ASSERT(d2[4] == -1);
    CPPUNIT_ASSERT(d2[5] == std::numeric_limits<AudioSample>::min());
    CPPUNIT_ASSERT(d2[6] == std::numeric_limits<AudioSample>::max());
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::MediaFrameTest::name());
