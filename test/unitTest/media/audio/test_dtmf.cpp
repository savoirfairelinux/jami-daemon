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

#include "videomanager_interface.h"
#include "media/audio/sound/dtmf.h"

#include "../test_runner.h"

#include <cstring>

namespace jami {
namespace test {

class DtmfTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "dtmf"; }

private:
    void pulseLongerThanToneWraps();
    void mismatchedFrameIsRejected();

    CPPUNIT_TEST_SUITE(DtmfTest);
    CPPUNIT_TEST(pulseLongerThanToneWraps);
    CPPUNIT_TEST(mismatchedFrameIsRejected);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(DtmfTest, DtmfTest::name());

void
DtmfTest::pulseLongerThanToneWraps()
{
    constexpr unsigned rate = 8000;
    constexpr unsigned extra = 100;
    DTMF dtmf(rate, AV_SAMPLE_FMT_S16);
    libjami::AudioFrame frame(AudioFormat(rate, 1), rate + extra);
    dtmf.startTone('1');
    CPPUNIT_ASSERT(dtmf.generateDTMF(frame.pointer()));

    // The tone buffer holds one second, so the pulse must loop back to its start.
    const auto* samples = reinterpret_cast<const int16_t*>(frame.pointer()->data[0]);
    CPPUNIT_ASSERT(std::memcmp(samples + rate, samples, extra * sizeof(int16_t)) == 0);
}

void
DtmfTest::mismatchedFrameIsRejected()
{
    DTMF dtmf(48000, AV_SAMPLE_FMT_FLT);
    libjami::AudioFrame frame(AudioFormat(48000, 2, AV_SAMPLE_FMT_FLTP), 960);
    dtmf.startTone('1');
    CPPUNIT_ASSERT(!dtmf.generateDTMF(frame.pointer()));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::DtmfTest::name());
