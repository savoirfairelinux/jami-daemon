/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
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

#include "audio/audio_frame_resizer.h"
#include "audio/audiobuffer.h"
#include "dring.h"
#include "libav_deps.h"
#include "media_buffer.h"

#include "../../../test_runner.h"

#include <stdexcept>

namespace ring { namespace test {

class AudioFrameResizerTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "audio_frame_resizer"; }

private:
    void testSameSize();
    void testBiggerInput();
    void testBiggerOutput();
    void testDifferentFormat();

    void gotFrame(std::shared_ptr<AudioFrame>&& framePtr);
    std::shared_ptr<AudioFrame> getFrame(int n);

    CPPUNIT_TEST_SUITE(AudioFrameResizerTest);
    CPPUNIT_TEST(testSameSize);
    CPPUNIT_TEST(testBiggerInput);
    CPPUNIT_TEST(testBiggerOutput);
    CPPUNIT_TEST(testDifferentFormat);
    CPPUNIT_TEST_SUITE_END();

    std::shared_ptr<AudioFrameResizer> q_;
    AudioFormat format_ = AudioFormat::STEREO();
    int outputSize_ = 960;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AudioFrameResizerTest, AudioFrameResizerTest::name());

void
AudioFrameResizerTest::gotFrame(std::shared_ptr<AudioFrame>&& framePtr)
{
    CPPUNIT_ASSERT(framePtr && framePtr->pointer());
    CPPUNIT_ASSERT(framePtr->pointer()->nb_samples == outputSize_);
}

std::shared_ptr<AudioFrame>
AudioFrameResizerTest::getFrame(int n)
{
    auto frame = std::make_shared<AudioFrame>();
    frame->pointer()->format = format_.sampleFormat;
    frame->pointer()->sample_rate = format_.sample_rate;
    frame->pointer()->channels = format_.nb_channels;
    frame->pointer()->channel_layout = av_get_default_channel_layout(format_.nb_channels);
    frame->pointer()->nb_samples = n;
    CPPUNIT_ASSERT(av_frame_get_buffer(frame->pointer(), 0) >= 0);
    return frame;
}

void
AudioFrameResizerTest::testSameSize()
{
    // input.nb_samples == output.nb_samples
    q_.reset(new AudioFrameResizer(format_, outputSize_, [this](std::shared_ptr<AudioFrame>&& f){ gotFrame(std::move(f)); }));
    auto in = getFrame(outputSize_);
    // gotFrame should be called after this
    CPPUNIT_ASSERT_NO_THROW(q_->enqueue(std::move(in)));
    CPPUNIT_ASSERT(q_->samples() == 0);
}

void
AudioFrameResizerTest::testBiggerInput()
{
    // input.nb_samples > output.nb_samples
    q_.reset(new AudioFrameResizer(format_, outputSize_, [this](std::shared_ptr<AudioFrame>&& f){ gotFrame(std::move(f)); }));
    auto in = getFrame(outputSize_ + 100);
    // gotFrame should be called after this
    CPPUNIT_ASSERT_NO_THROW(q_->enqueue(std::move(in)));
    CPPUNIT_ASSERT(q_->samples() == 100);
}

void
AudioFrameResizerTest::testBiggerOutput()
{
    // input.nb_samples < output.nb_samples
    q_.reset(new AudioFrameResizer(format_, outputSize_, [this](std::shared_ptr<AudioFrame>&& f){ gotFrame(std::move(f)); }));
    auto in = getFrame(outputSize_ - 100);
    CPPUNIT_ASSERT_NO_THROW(q_->enqueue(std::move(in)));
    CPPUNIT_ASSERT(q_->samples() == outputSize_ - 100);
    // gotFrame should be called after this
    CPPUNIT_ASSERT_NO_THROW(q_->enqueue(std::move(in)));
    // pushed 2 frames of (outputSize_ - 100) samples and got 1 frame
    CPPUNIT_ASSERT(q_->samples() == outputSize_ - 200);
}

void
AudioFrameResizerTest::testDifferentFormat()
{
    // frame format != q_->format_
    q_.reset(new AudioFrameResizer(format_, outputSize_, [this](std::shared_ptr<AudioFrame>&& f){ gotFrame(std::move(f)); }));
    auto in = getFrame(outputSize_-27);
    q_->enqueue(std::move(in));
    CPPUNIT_ASSERT(q_->samples()==outputSize_-27);
    q_->setFormat(AudioFormat::MONO(), 960);
    CPPUNIT_ASSERT(q_->samples() == 0);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::AudioFrameResizerTest::name());
