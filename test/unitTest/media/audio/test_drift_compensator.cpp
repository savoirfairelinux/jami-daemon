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

#include "media/audio/audio-processing/drift_compensator.h"

#include "../../../test_runner.h"

#include <cmath>
#include <random>

namespace jami {
namespace test {

class DriftCompensatorTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "drift_compensator"; }

private:
    void testSameClock();
    void testDrift();
    void testGap();

    CPPUNIT_TEST_SUITE(DriftCompensatorTest);
    CPPUNIT_TEST(testSameClock);
    CPPUNIT_TEST(testDrift);
    CPPUNIT_TEST(testGap);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(DriftCompensatorTest, DriftCompensatorTest::name());

namespace {

constexpr unsigned SAMPLE_RATE = 48000;

struct Result
{
    double drift;
    double correction;
    // Mean alignment (queued playback - queued capture samples) over the first and last quarter
    double alignmentStart;
    double alignmentEnd;
};

/**
 * Simulate capture and playback devices whose clocks deviate from the nominal rate by
 * @recordPpm and @playbackPpm, delivering chunks of different sizes with timing jitter.
 * The playback stream is resampled by the compensator correction.
 */
Result
simulate(
    DriftCompensator& dc, double recordPpm, double playbackPpm, double duration, double gapAt = -1, double gapLength = 0)
{
    constexpr size_t recordChunk = 480;
    constexpr size_t playbackChunk = 1024;
    const double recordPeriod = recordChunk / (SAMPLE_RATE * (1 + recordPpm * 1e-6));
    const double playbackPeriod = playbackChunk / (SAMPLE_RATE * (1 + playbackPpm * 1e-6));

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> jitter(0, 0.004);
    auto origin = DriftCompensator::clock::now();
    auto at = [&](double t) {
        return origin + std::chrono::duration_cast<DriftCompensator::clock::duration>(std::chrono::duration<double>(t));
    };

    double nextRecord = 0, nextPlayback = 0.003;
    double recorded = 0, played = 0, playedFraction = 0;
    double startSum = 0, endSum = 0;
    unsigned startCount = 0, endCount = 0;
    while (std::min(nextRecord, nextPlayback) < duration) {
        if (gapAt >= 0 && nextRecord >= gapAt && nextPlayback >= gapAt) {
            nextRecord += gapLength;
            nextPlayback += gapLength;
            gapAt = -1;
        }
        if (nextRecord <= nextPlayback) {
            dc.recorded(at(nextRecord + jitter(rng)), recordChunk);
            recorded += recordChunk;
            auto alignment = played - recorded;
            if (nextRecord < duration / 4) {
                startSum += alignment;
                startCount++;
            } else if (nextRecord > duration * 3 / 4) {
                endSum += alignment;
                endCount++;
            }
            nextRecord += recordPeriod;
        } else {
            playedFraction += playbackChunk * (1 + dc.correction());
            auto out = std::floor(playedFraction);
            playedFraction -= out;
            dc.played(at(nextPlayback + jitter(rng)), playbackChunk, (size_t) out);
            played += out;
            nextPlayback += playbackPeriod;
        }
    }
    return {dc.drift(), dc.correction(), startSum / startCount, endSum / endCount};
}

} // namespace

void
DriftCompensatorTest::testSameClock()
{
    DriftCompensator dc(SAMPLE_RATE);
    auto r = simulate(dc, 0, 0, 300);
    CPPUNIT_ASSERT_MESSAGE(std::to_string(r.drift * 1e6), std::abs(r.drift) < 5e-6);
    CPPUNIT_ASSERT_MESSAGE(std::to_string(r.correction * 1e6), std::abs(r.correction) < 5e-6);
    auto shift = r.alignmentEnd - r.alignmentStart;
    CPPUNIT_ASSERT_MESSAGE(std::to_string(shift), std::abs(shift) < 48);
}

void
DriftCompensatorTest::testDrift()
{
    // 120 ppm: 1728 samples (36 ms) of misalignment after 5 minutes without compensation
    DriftCompensator dc(SAMPLE_RATE);
    auto r = simulate(dc, -40, 80, 300);
    auto expected = (1 - 40e-6) / (1 + 80e-6) - 1;
    CPPUNIT_ASSERT_MESSAGE(std::to_string(r.drift * 1e6), std::abs(r.drift - expected) < 5e-6);
    CPPUNIT_ASSERT_MESSAGE(std::to_string(r.correction * 1e6), std::abs(r.correction - expected) < 10e-6);
    auto shift = r.alignmentEnd - r.alignmentStart;
    CPPUNIT_ASSERT_MESSAGE(std::to_string(shift), std::abs(shift) < 96);
}

void
DriftCompensatorTest::testGap()
{
    DriftCompensator dc(SAMPLE_RATE);
    auto r = simulate(dc, 100, -100, 300, 100, 5);
    auto expected = (1 + 100e-6) / (1 - 100e-6) - 1;
    CPPUNIT_ASSERT_MESSAGE(std::to_string(r.drift * 1e6), std::abs(r.drift - expected) < 5e-6);
    CPPUNIT_ASSERT_MESSAGE(std::to_string(r.correction * 1e6), std::abs(r.correction - expected) < 10e-6);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::DriftCompensatorTest::name());
