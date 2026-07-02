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
#include "media/h265_profile.h"

#include "../../test_runner.h"

namespace jami {
namespace test {

class H265ProfileTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "h265_profile"; }

private:
    void testParseFmtp();
    void testProfileFromFmtp();
    void testFmtpParams();
    void testPixelFormat();
    void testSetLevelId();
    void testNegotiableHighProfiles();

    CPPUNIT_TEST_SUITE(H265ProfileTest);
    CPPUNIT_TEST(testParseFmtp);
    CPPUNIT_TEST(testProfileFromFmtp);
    CPPUNIT_TEST(testFmtpParams);
    CPPUNIT_TEST(testPixelFormat);
    CPPUNIT_TEST(testSetLevelId);
    CPPUNIT_TEST(testNegotiableHighProfiles);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(H265ProfileTest, H265ProfileTest::name());

void
H265ProfileTest::testParseFmtp()
{
    // RFC 7798 §7.1 defaults when parameters are absent
    auto info = h265::parseFmtp("");
    CPPUNIT_ASSERT_EQUAL(0, info.profileSpace);
    CPPUNIT_ASSERT_EQUAL(1, info.profileId); // Main
    CPPUNIT_ASSERT_EQUAL(0, info.tierFlag);
    CPPUNIT_ASSERT_EQUAL(93, info.levelId); // level 3.1
    CPPUNIT_ASSERT_EQUAL(uint64_t(0xB00000000000), info.interopConstraints);

    info = h265::parseFmtp("profile-id=4;level-id=123;interop-constraints=BE0800000000");
    CPPUNIT_ASSERT_EQUAL(4, info.profileId);
    CPPUNIT_ASSERT_EQUAL(123, info.levelId);
    CPPUNIT_ASSERT_EQUAL(uint64_t(0xBE0800000000), info.interopConstraints);

    info = h265::parseFmtp("profile-id=2;level-id=123");
    CPPUNIT_ASSERT_EQUAL(2, info.profileId);
    CPPUNIT_ASSERT_EQUAL(123, info.levelId);

    // level-id must not be confused with max-recv-level-id or profile-id
    info = h265::parseFmtp("max-recv-level-id=150;profile-id=1;level-id=120");
    CPPUNIT_ASSERT_EQUAL(1, info.profileId);
    CPPUNIT_ASSERT_EQUAL(120, info.levelId);

    // With payload type prefix, as stored in pjmedia fmtp attribute values
    info = h265::parseFmtp("96 profile-id=4;interop-constraints=bd0800000000");
    CPPUNIT_ASSERT_EQUAL(4, info.profileId);
    CPPUNIT_ASSERT_EQUAL(uint64_t(0xBD0800000000), info.interopConstraints);
}

void
H265ProfileTest::testProfileFromFmtp()
{
    // Main
    auto profile = h265::profileFromFmtp(h265::parseFmtp("profile-id=1"));
    CPPUNIT_ASSERT(profile.has_value());
    CPPUNIT_ASSERT(*profile == h265::Profile::Main);
    // Absent profile-id implies Main
    profile = h265::profileFromFmtp(h265::parseFmtp(""));
    CPPUNIT_ASSERT(profile.has_value());
    CPPUNIT_ASSERT(*profile == h265::Profile::Main);

    // Main 10
    profile = h265::profileFromFmtp(h265::parseFmtp("profile-id=2;level-id=123"));
    CPPUNIT_ASSERT(profile.has_value());
    CPPUNIT_ASSERT(*profile == h265::Profile::Main10);

    // Main 4:4:4 (Range Extensions)
    profile = h265::profileFromFmtp(h265::parseFmtp("profile-id=4;interop-constraints=BE0800000000"));
    CPPUNIT_ASSERT(profile.has_value());
    CPPUNIT_ASSERT(*profile == h265::Profile::Main444);

    // Main 4:2:2 10 (Range Extensions)
    profile = h265::profileFromFmtp(h265::parseFmtp("profile-id=4;interop-constraints=BD0800000000"));
    CPPUNIT_ASSERT(profile.has_value());
    CPPUNIT_ASSERT(*profile == h265::Profile::Main422_10);

    // Unknown Range Extensions profile
    CPPUNIT_ASSERT(!h265::profileFromFmtp(h265::parseFmtp("profile-id=4;interop-constraints=B00000000000")).has_value());
    // Unknown profile-id
    CPPUNIT_ASSERT(!h265::profileFromFmtp(h265::parseFmtp("profile-id=7")).has_value());
    // Non-zero profile-space
    CPPUNIT_ASSERT(!h265::profileFromFmtp(h265::parseFmtp("profile-space=1;profile-id=1")).has_value());
}

void
H265ProfileTest::testFmtpParams()
{
    CPPUNIT_ASSERT_EQUAL(std::string("profile-id=1;level-id=123"), h265::fmtpParams(h265::Profile::Main, 123));
    CPPUNIT_ASSERT_EQUAL(std::string("profile-id=2;level-id=123"), h265::fmtpParams(h265::Profile::Main10, 123));
    CPPUNIT_ASSERT_EQUAL(std::string("profile-id=4;level-id=123;interop-constraints=BE0800000000"),
                         h265::fmtpParams(h265::Profile::Main444, 123));
    CPPUNIT_ASSERT_EQUAL(std::string("profile-id=4;level-id=123;interop-constraints=BD0800000000"),
                         h265::fmtpParams(h265::Profile::Main422_10, 123));

    // Round-trip
    for (auto profile :
         {h265::Profile::Main, h265::Profile::Main10, h265::Profile::Main444, h265::Profile::Main422_10}) {
        auto parsed = h265::profileFromFmtp(h265::parseFmtp(h265::fmtpParams(profile, 120)));
        CPPUNIT_ASSERT(parsed.has_value());
        CPPUNIT_ASSERT(*parsed == profile);
    }
}

void
H265ProfileTest::testPixelFormat()
{
    CPPUNIT_ASSERT_EQUAL(AV_PIX_FMT_YUV420P, h265::pixelFormat(h265::Profile::Main));
    // 10-bit 4:2:0, semi-planar: the input format of hardware HEVC
    // 10-bit encoders (NVENC, VideoToolbox, QSV)
    CPPUNIT_ASSERT_EQUAL(AV_PIX_FMT_P010, h265::pixelFormat(h265::Profile::Main10));
    CPPUNIT_ASSERT_EQUAL(AV_PIX_FMT_YUV422P, h265::pixelFormat(h265::Profile::Main422_10));
    CPPUNIT_ASSERT_EQUAL(AV_PIX_FMT_YUV444P, h265::pixelFormat(h265::Profile::Main444));
}

void
H265ProfileTest::testSetLevelId()
{
    // Rewrites the level-id token only
    CPPUNIT_ASSERT_EQUAL(std::string("profile-id=1;level-id=93"), h265::setLevelId("profile-id=1;level-id=123", 93));
    // Does not touch max-recv-level-id
    CPPUNIT_ASSERT_EQUAL(std::string("max-recv-level-id=150;level-id=90"),
                         h265::setLevelId("max-recv-level-id=150;level-id=123", 90));
    // Appends when absent
    CPPUNIT_ASSERT_EQUAL(std::string("profile-id=1;level-id=90"), h265::setLevelId("profile-id=1", 90));
}

void
H265ProfileTest::testNegotiableHighProfiles()
{
    libav_utils::av_init();
    // Only profiles that the local encoders and decoders actually
    // support may be advertised (RFC 7798 §7.2.2 symmetric use).
    for (auto profile : h265::negotiableHighProfiles()) {
        CPPUNIT_ASSERT(profile == h265::Profile::Main444 || profile == h265::Profile::Main422_10
                       || profile == h265::Profile::Main10);
        CPPUNIT_ASSERT(h265::canEncode(profile));
        CPPUNIT_ASSERT(h265::canDecode(profile));
    }
    // Main is always encodable when an H265 encoder is present at all;
    // the software decoder handles every negotiable profile.
    CPPUNIT_ASSERT(h265::canDecode(h265::Profile::Main));
    CPPUNIT_ASSERT(h265::canDecode(h265::Profile::Main10));
    CPPUNIT_ASSERT(h265::canDecode(h265::Profile::Main444));
    CPPUNIT_ASSERT(h265::canDecode(h265::Profile::Main422_10));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::H265ProfileTest::name());
