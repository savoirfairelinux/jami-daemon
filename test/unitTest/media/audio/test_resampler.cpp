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

#include "jami.h"
#include "videomanager_interface.h"
#include "libav_deps.h"
#include "audio/resampler.h"

#include "../test_runner.h"

namespace jami { namespace test {

class ResamplerTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "resampler"; }

    void setUp();
    void tearDown();

private:
    void testAudioBuffer();
    void testAudioFrame();
    void testRematrix();

    CPPUNIT_TEST_SUITE(ResamplerTest);
    CPPUNIT_TEST(testAudioBuffer);
    CPPUNIT_TEST(testAudioFrame);
    CPPUNIT_TEST(testRematrix);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<Resampler> resampler_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ResamplerTest, ResamplerTest::name());

void
ResamplerTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
}

void
ResamplerTest::tearDown()
{
    libjami::fini();
}

void
ResamplerTest::testAudioBuffer()
{
    const constexpr AudioFormat infmt(44100, 1);
    const constexpr AudioFormat outfmt(48000, 2);

    resampler_.reset(new Resampler);

    AudioBuffer inbuf(1024, infmt);
    AudioBuffer outbuf(0, outfmt);

    resampler_->resample(inbuf, outbuf);
    CPPUNIT_ASSERT(outbuf.getFormat().sample_rate == 48000);
    CPPUNIT_ASSERT(outbuf.getFormat().nb_channels == 2);
}

void
ResamplerTest::testAudioFrame()
{
    const constexpr AudioFormat infmt(44100, 1);

    resampler_.reset(new Resampler);

    AudioBuffer inbuf(1024, infmt);
    auto input = inbuf.toAVFrame();
    CPPUNIT_ASSERT(input->pointer()->data && input->pointer()->data[0]);
    CPPUNIT_ASSERT(input->pointer()->data[0][0] == 0);

    libjami::AudioFrame out;
    auto output = out.pointer();
    output->format = AV_SAMPLE_FMT_FLT;
    output->sample_rate = 48000;
    output->channel_layout = AV_CH_LAYOUT_STEREO;
    output->channels = 2;

    int ret = resampler_->resample(input->pointer(), output);
    CPPUNIT_ASSERT_MESSAGE(libav_utils::getError(ret).c_str(), ret >= 0);
    CPPUNIT_ASSERT(output->data && output->data[0]);
    CPPUNIT_ASSERT(output->data[0][0] == 0);
}

void
ResamplerTest::testRematrix()
{
    int ret = 0;
    const constexpr AudioFormat inFormat = AudioFormat(44100, 6);
    resampler_.reset(new Resampler);

    auto input = std::make_unique<libjami::AudioFrame>(inFormat, 882);
    CPPUNIT_ASSERT(input->pointer() && input->pointer()->data);

    auto output1 = std::make_unique<libjami::AudioFrame>(AudioFormat::STEREO(), 960);
    CPPUNIT_ASSERT(output1->pointer() && output1->pointer()->data);

    ret = resampler_->resample(input->pointer(), output1->pointer());
    CPPUNIT_ASSERT_MESSAGE(libav_utils::getError(ret).c_str(), ret >= 0);
    CPPUNIT_ASSERT(output1->pointer()->data && output1->pointer()->data[0]);

    auto output2 = std::make_unique<libjami::AudioFrame>(AudioFormat::MONO(), 960);
    CPPUNIT_ASSERT(output2->pointer() && output2->pointer()->data);

    ret = resampler_->resample(input->pointer(), output2->pointer());
    CPPUNIT_ASSERT_MESSAGE(libav_utils::getError(ret).c_str(), ret >= 0);
    CPPUNIT_ASSERT(output2->pointer()->data && output2->pointer()->data[0]);
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::ResamplerTest::name());
