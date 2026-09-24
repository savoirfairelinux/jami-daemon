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

#include "media/audio/audio-processing/null_audio_processor.h"
#include "media/libav_utils.h"

#include "../../../test_runner.h"

namespace jami {
namespace test {

class AudioProcessorTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "audio_processor"; }

private:
    void testPassThrough();

    CPPUNIT_TEST_SUITE(AudioProcessorTest);
    CPPUNIT_TEST(testPassThrough);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AudioProcessorTest, AudioProcessorTest::name());

void
AudioProcessorTest::testPassThrough()
{
    const AudioFormat format(48000, 2, AV_SAMPLE_FMT_S16);
    const unsigned frameSize = 480;
    NullAudioProcessor processor(format, frameSize);

    const AudioFormat playbackFormat(44100, 2, AV_SAMPLE_FMT_FLT);
    const int iterations = 200;
    int processed = 0;
    for (int i = 0; i < iterations; ++i) {
        auto playback = std::make_shared<AudioFrame>(playbackFormat, 441);
        libav_utils::fillWithSilence(playback->pointer());
        processor.putPlayback(playback);

        auto record = std::make_shared<AudioFrame>(format, frameSize);
        libav_utils::fillWithSilence(record->pointer());
        processor.putRecorded(std::move(record));

        while (auto frame = processor.getProcessed()) {
            CPPUNIT_ASSERT_EQUAL(frameSize, (unsigned) frame->pointer()->nb_samples);
            processed++;
        }
    }
    // The first frame of each stream only starts the processor
    CPPUNIT_ASSERT_MESSAGE(std::to_string(processed), processed >= iterations - 3);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::AudioProcessorTest::name());
