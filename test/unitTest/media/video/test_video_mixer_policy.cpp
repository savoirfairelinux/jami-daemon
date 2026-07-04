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
#include "media/video/video_mixer.h"

#include "../../../test_runner.h"

namespace jami {
namespace video {
namespace test {

namespace {
constexpr std::pair<int, int> BASE_CAP {2560, 1440};
constexpr std::pair<int, int> BIG_CAP {3840, 2160};
} // namespace

class VideoMixerPolicyTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "video_mixer_policy"; }

    void setUp();
    void tearDown();

private:
    void testSingleCameraShrinksToSource();
    void testGridGrowsWithParticipants();
    void testDenseGridStaysUnderBaseCap();
    void testOneBigFollowsActiveSource();
    void testOneBigWithSmallKeepsPreviewsUsable();
    void testBigCapOnlyForSmallConferences();
    void testNoUsableSource();
    void testFrameRateFollowsFastestSource();

    CPPUNIT_TEST_SUITE(VideoMixerPolicyTest);
    CPPUNIT_TEST(testSingleCameraShrinksToSource);
    CPPUNIT_TEST(testGridGrowsWithParticipants);
    CPPUNIT_TEST(testDenseGridStaysUnderBaseCap);
    CPPUNIT_TEST(testOneBigFollowsActiveSource);
    CPPUNIT_TEST(testOneBigWithSmallKeepsPreviewsUsable);
    CPPUNIT_TEST(testBigCapOnlyForSmallConferences);
    CPPUNIT_TEST(testNoUsableSource);
    CPPUNIT_TEST(testFrameRateFollowsFastestSource);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(VideoMixerPolicyTest, VideoMixerPolicyTest::name());

void
VideoMixerPolicyTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
}

void
VideoMixerPolicyTest::tearDown()
{
    libjami::fini();
}

void
VideoMixerPolicyTest::testSingleCameraShrinksToSource()
{
    // A single 720p camera does not need a surface larger than itself:
    // upscaling it would waste encode budget without any quality gain.
    std::vector<VideoMixer::SourceSpec> sources {{1280, 720, 30, false}};
    const auto surface = VideoMixer::computeTargetSurface(Layout::GRID, sources, BASE_CAP, BIG_CAP);
    CPPUNIT_ASSERT_EQUAL(1280, surface.first);
    CPPUNIT_ASSERT_EQUAL(720, surface.second);
}

void
VideoMixerPolicyTest::testGridGrowsWithParticipants()
{
    // Four 720p cameras: 2x2 grid of full-definition cells.
    std::vector<VideoMixer::SourceSpec> sources(4, VideoMixer::SourceSpec {1280, 720, 30, false});
    const auto surface = VideoMixer::computeTargetSurface(Layout::GRID, sources, BASE_CAP, BIG_CAP);
    CPPUNIT_ASSERT_EQUAL(2560, surface.first);
    CPPUNIT_ASSERT_EQUAL(1440, surface.second);
}

void
VideoMixerPolicyTest::testDenseGridStaysUnderBaseCap()
{
    // Nine 720p cameras would need 3840x2160: the grid stays under the base
    // cap because the composite is encoded once per participant.
    std::vector<VideoMixer::SourceSpec> sources(9, VideoMixer::SourceSpec {1280, 720, 30, false});
    const auto surface = VideoMixer::computeTargetSurface(Layout::GRID, sources, BASE_CAP, BIG_CAP);
    CPPUNIT_ASSERT(surface.first <= BASE_CAP.first);
    CPPUNIT_ASSERT(surface.second <= BASE_CAP.second);
    // The aspect ratio of the requested grid is preserved.
    CPPUNIT_ASSERT_EQUAL(2560, surface.first);
    CPPUNIT_ASSERT_EQUAL(1440, surface.second);
}

void
VideoMixerPolicyTest::testOneBigFollowsActiveSource()
{
    // A promoted 4K screen share drives the surface up to the big cap.
    std::vector<VideoMixer::SourceSpec> sources {{3840, 2160, 30, true}, {1280, 720, 30, false}};
    const auto surface = VideoMixer::computeTargetSurface(Layout::ONE_BIG, sources, BASE_CAP, BIG_CAP);
    CPPUNIT_ASSERT_EQUAL(3840, surface.first);
    CPPUNIT_ASSERT_EQUAL(2160, surface.second);

    // A promoted 720p camera shrinks the surface to itself.
    std::vector<VideoMixer::SourceSpec> cam {{1280, 720, 30, true}};
    const auto small = VideoMixer::computeTargetSurface(Layout::ONE_BIG, cam, BASE_CAP, BIG_CAP);
    CPPUNIT_ASSERT_EQUAL(1280, small.first);
    CPPUNIT_ASSERT_EQUAL(720, small.second);
}

void
VideoMixerPolicyTest::testOneBigWithSmallKeepsPreviewsUsable()
{
    // A low-definition promoted camera must not collapse the surface: the
    // small previews on the top line still need their share of the base
    // surface to stay legible.
    std::vector<VideoMixer::SourceSpec> sources {{720, 480, 30, true},
                                                 {1280, 720, 30, false},
                                                 {1280, 720, 30, false}};
    const auto surface = VideoMixer::computeTargetSurface(Layout::ONE_BIG_WITH_SMALL, sources, BASE_CAP, BIG_CAP);
    CPPUNIT_ASSERT_EQUAL(BASE_CAP.first, surface.first);
    CPPUNIT_ASSERT_EQUAL(BASE_CAP.second, surface.second);

    // Low-definition previews only claim what they can fill.
    std::vector<VideoMixer::SourceSpec> lowPreviews {{720, 480, 30, true}, {320, 240, 30, false}};
    const auto modest = VideoMixer::computeTargetSurface(Layout::ONE_BIG_WITH_SMALL, lowPreviews, BASE_CAP, BIG_CAP);
    CPPUNIT_ASSERT(modest.first < BASE_CAP.first);
    CPPUNIT_ASSERT(modest.first >= 720);

    // A promoted 4K screen share still drives the surface up to the big cap.
    std::vector<VideoMixer::SourceSpec> share {{3840, 2160, 30, true},
                                               {1280, 720, 30, false},
                                               {1280, 720, 30, false}};
    const auto big = VideoMixer::computeTargetSurface(Layout::ONE_BIG_WITH_SMALL, share, BASE_CAP, BIG_CAP);
    CPPUNIT_ASSERT_EQUAL(3840, big.first);
    CPPUNIT_ASSERT_EQUAL(2160, big.second);
}

void
VideoMixerPolicyTest::testBigCapOnlyForSmallConferences()
{
    // With more than 3 sources the composite is encoded for each of them:
    // the 4K cap is not allowed anymore, the base cap applies.
    std::vector<VideoMixer::SourceSpec> sources(4, VideoMixer::SourceSpec {1280, 720, 30, false});
    sources[0] = {3840, 2160, 30, true};
    const auto surface = VideoMixer::computeTargetSurface(Layout::ONE_BIG, sources, BASE_CAP, BIG_CAP);
    CPPUNIT_ASSERT_EQUAL(2560, surface.first);
    CPPUNIT_ASSERT_EQUAL(1440, surface.second);
}

void
VideoMixerPolicyTest::testNoUsableSource()
{
    // No source at all, or no valid frame yet: no opinion, keep the current
    // surface.
    std::vector<VideoMixer::SourceSpec> none;
    CPPUNIT_ASSERT(VideoMixer::computeTargetSurface(Layout::GRID, none, BASE_CAP, BIG_CAP)
                   == (std::pair<int, int> {0, 0}));

    std::vector<VideoMixer::SourceSpec> invalid {{0, 0, 0, false}};
    CPPUNIT_ASSERT(VideoMixer::computeTargetSurface(Layout::GRID, invalid, BASE_CAP, BIG_CAP)
                   == (std::pair<int, int> {0, 0}));

    // ONE_BIG without an active source keeps the current surface too.
    std::vector<VideoMixer::SourceSpec> inactive {{1280, 720, 30, false}};
    CPPUNIT_ASSERT(VideoMixer::computeTargetSurface(Layout::ONE_BIG, inactive, BASE_CAP, BIG_CAP)
                   == (std::pair<int, int> {0, 0}));
}

void
VideoMixerPolicyTest::testFrameRateFollowsFastestSource()
{
    // Standard cameras keep the mixer at 30 fps, measurement jitter included.
    std::vector<VideoMixer::SourceSpec> cams {{1280, 720, 31, false}, {1920, 1080, 24, false}};
    CPPUNIT_ASSERT_EQUAL(30, VideoMixer::computeTargetFrameRate(cams, 60));

    // A 60 fps capture raises the mixer rate, up to the allowed maximum.
    std::vector<VideoMixer::SourceSpec> fast {{1920, 1080, 58, false}, {1280, 720, 30, false}};
    CPPUNIT_ASSERT_EQUAL(60, VideoMixer::computeTargetFrameRate(fast, 60));

    // Without headroom (software host), the rate stays at 30 fps.
    CPPUNIT_ASSERT_EQUAL(30, VideoMixer::computeTargetFrameRate(fast, 30));
}

} // namespace test
} // namespace video
} // namespace jami

CORE_TEST_RUNNER(jami::video::test::VideoMixerPolicyTest::name());
