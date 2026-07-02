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

#include "media/libav_deps.h"
#include "media/h264_profile.h"

#include "../../test_runner.h"

namespace jami {
namespace test {

class H264ProfileTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "h264_profile"; }

private:
    void testParseProfileLevelId();
    void testMakeProfileLevelId();
    void testPixelFormat();
    void testNegotiableHighProfiles();
    void testLevelHelpers();
    void testProfileName();

    CPPUNIT_TEST_SUITE(H264ProfileTest);
    CPPUNIT_TEST(testParseProfileLevelId);
    CPPUNIT_TEST(testMakeProfileLevelId);
    CPPUNIT_TEST(testPixelFormat);
    CPPUNIT_TEST(testNegotiableHighProfiles);
    CPPUNIT_TEST(testLevelHelpers);
    CPPUNIT_TEST(testProfileName);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(H264ProfileTest, H264ProfileTest::name());

void
H264ProfileTest::testParseProfileLevelId()
{
    // Constrained Baseline (RFC 6184 §8.1 example)
    auto pl = h264::parseProfileLevelId("profile-level-id=42e01f");
    CPPUNIT_ASSERT(pl.has_value());
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_CONSTRAINED_BASELINE, pl->profile);
    CPPUNIT_ASSERT_EQUAL(0x1f, pl->level);

    // Baseline without constraint_set1_flag
    pl = h264::parseProfileLevelId("profile-level-id=42001f");
    CPPUNIT_ASSERT(pl.has_value());
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_BASELINE, pl->profile);

    // High
    pl = h264::parseProfileLevelId("profile-level-id=640029");
    CPPUNIT_ASSERT(pl.has_value());
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH, pl->profile);
    CPPUNIT_ASSERT_EQUAL(0x29, pl->level);

    // High 4:2:2
    pl = h264::parseProfileLevelId("profile-level-id=7a0029");
    CPPUNIT_ASSERT(pl.has_value());
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH_422, pl->profile);

    // High 4:4:4 Predictive
    pl = h264::parseProfileLevelId("profile-level-id=f40029");
    CPPUNIT_ASSERT(pl.has_value());
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH_444_PREDICTIVE, pl->profile);

    // Embedded among other fmtp parameters
    pl = h264::parseProfileLevelId("packetization-mode=1;profile-level-id=f40029;max-fs=8160");
    CPPUNIT_ASSERT(pl.has_value());
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH_444_PREDICTIVE, pl->profile);

    // Missing or malformed
    CPPUNIT_ASSERT(!h264::parseProfileLevelId("").has_value());
    CPPUNIT_ASSERT(!h264::parseProfileLevelId("packetization-mode=1").has_value());
    CPPUNIT_ASSERT(!h264::parseProfileLevelId("profile-level-id=42e0").has_value());
    // Unknown profile_idc
    CPPUNIT_ASSERT(!h264::parseProfileLevelId("profile-level-id=ff0029").has_value());
}

void
H264ProfileTest::testMakeProfileLevelId()
{
    CPPUNIT_ASSERT_EQUAL(std::string("profile-level-id=42e029"),
                         h264::makeProfileLevelId(AV_PROFILE_H264_CONSTRAINED_BASELINE, 0x29));
    CPPUNIT_ASSERT_EQUAL(std::string("profile-level-id=7a0029"),
                         h264::makeProfileLevelId(AV_PROFILE_H264_HIGH_422, 0x29));
    CPPUNIT_ASSERT_EQUAL(std::string("profile-level-id=f40029"),
                         h264::makeProfileLevelId(AV_PROFILE_H264_HIGH_444_PREDICTIVE, 0x29));

    // Round-trip
    for (int profile : {AV_PROFILE_H264_CONSTRAINED_BASELINE,
                        AV_PROFILE_H264_BASELINE,
                        AV_PROFILE_H264_MAIN,
                        AV_PROFILE_H264_HIGH,
                        AV_PROFILE_H264_HIGH_422,
                        AV_PROFILE_H264_HIGH_444_PREDICTIVE}) {
        auto pl = h264::parseProfileLevelId(h264::makeProfileLevelId(profile, 0x28));
        CPPUNIT_ASSERT(pl.has_value());
        CPPUNIT_ASSERT_EQUAL(profile, pl->profile);
        CPPUNIT_ASSERT_EQUAL(0x28, pl->level);
    }
}

void
H264ProfileTest::testPixelFormat()
{
    CPPUNIT_ASSERT_EQUAL(AV_PIX_FMT_YUV420P, h264::pixelFormat(AV_PROFILE_H264_CONSTRAINED_BASELINE));
    CPPUNIT_ASSERT_EQUAL(AV_PIX_FMT_YUV420P, h264::pixelFormat(AV_PROFILE_H264_HIGH));
    CPPUNIT_ASSERT_EQUAL(AV_PIX_FMT_YUV422P, h264::pixelFormat(AV_PROFILE_H264_HIGH_422));
    CPPUNIT_ASSERT_EQUAL(AV_PIX_FMT_YUV444P, h264::pixelFormat(AV_PROFILE_H264_HIGH_444_PREDICTIVE));
}

void
H264ProfileTest::testProfileName()
{
    // Human-readable profile name for the advanced call information
    CPPUNIT_ASSERT_EQUAL(std::string("Constrained Baseline"), h264::profileName("profile-level-id=42e01f"));
    CPPUNIT_ASSERT_EQUAL(std::string("High"), h264::profileName("profile-level-id=640029"));
    CPPUNIT_ASSERT_EQUAL(std::string("High 4:2:2"), h264::profileName("profile-level-id=7a0029"));
    CPPUNIT_ASSERT_EQUAL(std::string("High 4:4:4 Predictive"), h264::profileName("profile-level-id=f40029"));
    // RFC 6184 §8.1: an absent profile-level-id implies Constrained Baseline
    CPPUNIT_ASSERT_EQUAL(std::string("Constrained Baseline"), h264::profileName(""));
}

void
H264ProfileTest::testNegotiableHighProfiles()
{
    libav_utils::av_init();
    const auto profiles = h264::negotiableHighProfiles();
    // The bundled software encoder (libx264) supports 4:4:4 and 4:2:2,
    // and the software decoder can consume them.
    CPPUNIT_ASSERT_EQUAL(size_t(2), profiles.size());
    // Best chroma first
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH_444_PREDICTIVE, profiles[0]);
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH_422, profiles[1]);

    for (int profile : profiles) {
        CPPUNIT_ASSERT(h264::canEncode(profile));
        CPPUNIT_ASSERT(h264::canDecode(profile));
    }
}

void
H264ProfileTest::testLevelHelpers()
{
    // level-asymmetry-allowed detection (RFC 6184 §8.1)
    CPPUNIT_ASSERT(h264::levelAsymmetryAllowed("profile-level-id=428029;level-asymmetry-allowed=1"));
    CPPUNIT_ASSERT(h264::levelAsymmetryAllowed("level-asymmetry-allowed=1;profile-level-id=428029"));
    CPPUNIT_ASSERT(!h264::levelAsymmetryAllowed("profile-level-id=428029;level-asymmetry-allowed=0"));
    CPPUNIT_ASSERT(!h264::levelAsymmetryAllowed("profile-level-id=428029"));
    CPPUNIT_ASSERT(!h264::levelAsymmetryAllowed(""));

    // setLevel rewrites the level part of profile-level-id in place
    CPPUNIT_ASSERT_EQUAL(std::string("profile-level-id=42801f;level-asymmetry-allowed=1"),
                         h264::setLevel("profile-level-id=428029;level-asymmetry-allowed=1", 0x1f));
    CPPUNIT_ASSERT_EQUAL(std::string("profile-level-id=f40033"), h264::setLevel("profile-level-id=f40029", 0x33));
    // No profile-level-id: parameters returned unchanged
    CPPUNIT_ASSERT_EQUAL(std::string("packetization-mode=1"), h264::setLevel("packetization-mode=1", 0x1f));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::H264ProfileTest::name());
