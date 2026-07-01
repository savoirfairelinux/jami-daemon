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

#include "jami.h"
#include "media/video/accel.h"

#include "../../../test_runner.h"

namespace jami {
namespace video {
namespace test {

class AccelTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "accel"; }

    void setUp();
    void tearDown();

private:
    void testEncoderAvailabilityConsistency();
    void testUnsupportedCodecIsUnavailable();

    CPPUNIT_TEST_SUITE(AccelTest);
    CPPUNIT_TEST(testEncoderAvailabilityConsistency);
    CPPUNIT_TEST(testUnsupportedCodecIsUnavailable);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AccelTest, AccelTest::name());

void
AccelTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
}

void
AccelTest::tearDown()
{
    libjami::fini();
}

void
AccelTest::testEncoderAvailabilityConsistency()
{
    // In a fresh process no device has been probed yet, so no device can be
    // in the NOT_USABLE state: availability must match whether a compatible
    // API/device pair exists at all.
    const bool hasCompatible = !HardwareAccel::getCompatibleAccel(AV_CODEC_ID_H264, 2560, 1440, CODEC_ENCODER).empty();
    CPPUNIT_ASSERT_EQUAL(hasCompatible, HardwareAccel::isEncoderAvailable(AV_CODEC_ID_H264, 2560, 1440));
}

void
AccelTest::testUnsupportedCodecIsUnavailable()
{
    // No hardware encoder API is declared for these codecs, so availability
    // must be false regardless of the host hardware.
    CPPUNIT_ASSERT_EQUAL(false, HardwareAccel::isEncoderAvailable(AV_CODEC_ID_MJPEG, 1280, 720));
    CPPUNIT_ASSERT_EQUAL(false, HardwareAccel::isEncoderAvailable(AV_CODEC_ID_NONE, 1280, 720));
}

} // namespace test
} // namespace video
} // namespace jami

CORE_TEST_RUNNER(jami::video::test::AccelTest::name());
