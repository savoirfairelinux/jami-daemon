/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
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

#include "audio/audio_queue.h"
#include "audio/audiobuffer.h"
#include "dring.h"
#include "libav_deps.h"
#include "media_buffer.h"

#include "../../../test_runner.h"

#include <iostream>
namespace ring { namespace test {

class AudioQueueTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "audio_queue"; }

private:
    void testSameSize();
    void testBiggerInput();
    void testBiggerOutput();
    void testDifferentFormat();
    void testFormatChange();

    std::unique_ptr<AudioFrame> getFrame(int n);

    CPPUNIT_TEST_SUITE(AudioQueueTest);
    CPPUNIT_TEST(testSameSize);
    CPPUNIT_TEST(testBiggerInput);
    CPPUNIT_TEST(testBiggerOutput);
    CPPUNIT_TEST(testDifferentFormat);
    CPPUNIT_TEST(testFormatChange);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<AudioQueue> q_;
    AudioFormat format_ = AudioFormat::STEREO();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AudioQueueTest, AudioQueueTest::name());

std::unique_ptr<AudioFrame>
AudioQueueTest::getFrame(int n)
{
    auto frame = std::make_unique<AudioFrame>();
    frame->pointer()->format = format_.sampleFormat;
    frame->pointer()->sample_rate = format_.sample_rate;
    frame->pointer()->channels = format_.nb_channels;
    frame->pointer()->channel_layout = av_get_default_channel_layout(format_.nb_channels);
    frame->pointer()->nb_samples = n;
    CPPUNIT_ASSERT(av_frame_get_buffer(frame->pointer(), 0) >= 0);
    return frame;
}

void
AudioQueueTest::testSameSize()
{
    // input.nb_samples == output.nb_samples
    q_.reset(new AudioQueue(format_));
    auto in = getFrame(960);
    CPPUNIT_ASSERT(q_->enqueue(*in) >= 0);
    CPPUNIT_ASSERT(q_->getSize() == in->pointer()->nb_samples);
    auto out = q_->dequeue(960);
    CPPUNIT_ASSERT(out && out->pointer()->nb_samples == 960);
    CPPUNIT_ASSERT(q_->getSize() == 0);
}

void
AudioQueueTest::testBiggerInput()
{
    // input.nb_samples > output.nb_samples
    q_.reset(new AudioQueue(format_));
    auto in = getFrame(960);
    CPPUNIT_ASSERT(q_->enqueue(*in) >= 0);
    CPPUNIT_ASSERT(q_->getSize() == in->pointer()->nb_samples);
    auto out = q_->dequeue(882);
    CPPUNIT_ASSERT(out && out->pointer()->nb_samples == 882);
    CPPUNIT_ASSERT(q_->getSize() == in->pointer()->nb_samples - out->pointer()->nb_samples);
}

void
AudioQueueTest::testBiggerOutput()
{
    // input.nb_samples < output.nb_samples
    q_.reset(new AudioQueue(format_));
    auto in = getFrame(882);
    CPPUNIT_ASSERT(q_->enqueue(*in) >= 0);
    CPPUNIT_ASSERT(q_->getSize() == in->pointer()->nb_samples);
    auto out = q_->dequeue(960);
    CPPUNIT_ASSERT(!out);
    CPPUNIT_ASSERT(q_->getSize() == in->pointer()->nb_samples);
    CPPUNIT_ASSERT(q_->enqueue(*in) >= 0);
    CPPUNIT_ASSERT(q_->getSize() == in->pointer()->nb_samples * 2);
    out = q_->dequeue(960);
    CPPUNIT_ASSERT(out && out->pointer()->nb_samples == 960);
    CPPUNIT_ASSERT(q_->getSize() == (in->pointer()->nb_samples * 2 - out->pointer()->nb_samples));
}

void
AudioQueueTest::testDifferentFormat()
{
    // frame format != q_->getFormat()
    q_.reset(new AudioQueue(format_));
    auto in = getFrame(960);
    in->pointer()->channels = 1;
    CPPUNIT_ASSERT(q_->enqueue(*in) < 0);
    CPPUNIT_ASSERT(q_->getSize() == 0);
}

void
AudioQueueTest::testFormatChange()
{
    // setFormat drains the q_
    q_.reset(new AudioQueue(format_));
    auto in = getFrame(960);
    CPPUNIT_ASSERT(q_->enqueue(*in) == in->pointer()->nb_samples);
    q_->setFormat(AudioFormat::MONO());
    CPPUNIT_ASSERT(q_->getSize() == 0);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::AudioQueueTest::name());
