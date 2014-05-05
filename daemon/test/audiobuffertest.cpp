/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Adrien Beraud <adrienberaud@gmail.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <string>
#include "audiobuffertest.h"
#include "audio/audiobuffer.h"
#include "logger.h"
#include "test_utils.h"

void AudioBufferTest::testAudioBufferConstructors()
{
    TITLE();

    SFLAudioSample test_samples2[] = {10, 11, 12, 13, 14, 15, 16, 17};

    AudioBuffer empty_buf(0, AudioFormat::MONO);
    CPPUNIT_ASSERT(empty_buf.frames() == 0);
    CPPUNIT_ASSERT(empty_buf.channels() == 1);
    CPPUNIT_ASSERT(empty_buf.getChannel(0)->size() == 0);

    AudioBuffer test_buf1(8, AudioFormat::STEREO);
    CPPUNIT_ASSERT(test_buf1.frames() == 8);
    CPPUNIT_ASSERT(test_buf1.channels() == 2);
    CPPUNIT_ASSERT(test_buf1.getChannel(0)->size() == 8);
    CPPUNIT_ASSERT(test_buf1.getChannel(1)->size() == 8);
    CPPUNIT_ASSERT(test_buf1.getChannel(2) == NULL);

    AudioBuffer test_buf3(test_samples2, 4, AudioFormat::STEREO);
    CPPUNIT_ASSERT(test_buf3.frames() == 4);
    CPPUNIT_ASSERT(test_buf3.channels() == 2);
    CPPUNIT_ASSERT(test_buf3.getChannel(0)->size() == 4);
}

void AudioBufferTest::testAudioBufferMix()
{
    TITLE();

    SFLAudioSample test_samples1[] = {18, 19, 20, 21, 22, 23, 24, 25};
    SFLAudioSample test_samples2[] = {10, 11, 12, 13, 14, 15, 16, 17, 18};

    AudioBuffer test_buf1(test_samples1, 4, AudioFormat::STEREO);
    CPPUNIT_ASSERT(test_buf1.channels() == 2);
    test_buf1.setChannelNum(1);
    CPPUNIT_ASSERT(test_buf1.channels() == 1);
    test_buf1.setChannelNum(2);
    CPPUNIT_ASSERT(test_buf1.channels() == 2);
    CPPUNIT_ASSERT(test_buf1.getChannel(1)->size() == 4);
    CPPUNIT_ASSERT((*test_buf1.getChannel(1))[0] == 0);
    test_buf1.setChannelNum(1);
    test_buf1.setChannelNum(2, true);
    CPPUNIT_ASSERT((*test_buf1.getChannel(1))[0] == test_samples1[0]);

    AudioBuffer test_buf2(0, AudioFormat::MONO);
    test_buf2.deinterleave(test_samples2, 3, 3);
    CPPUNIT_ASSERT((*test_buf2.getChannel(0))[2] == test_samples2[6]);
    CPPUNIT_ASSERT((*test_buf2.getChannel(1))[1] == test_samples2[4]);
    CPPUNIT_ASSERT((*test_buf2.getChannel(2))[0] == test_samples2[2]);
    CPPUNIT_ASSERT(test_buf2.capacity() == 9);

    SFLAudioSample *output = new SFLAudioSample[test_buf2.capacity()];
    test_buf2.interleave(output);
    CPPUNIT_ASSERT(std::equal(test_samples2, test_samples2 + sizeof test_samples2 / sizeof *test_samples2, output));
    //CPPUNIT_ASSERT(std::equal(std::begin(test_samples2), std::end(test_samples2), std::begin(output))); C++11

    test_buf1.mix(test_buf2);
    CPPUNIT_ASSERT(test_buf1.channels() == 2);
    CPPUNIT_ASSERT(test_buf1.frames() == 4);
    CPPUNIT_ASSERT((*test_buf1.getChannel(0))[0] == test_samples1[0]+test_samples2[0]);
    CPPUNIT_ASSERT((*test_buf1.getChannel(1))[0] == test_samples1[0]+test_samples2[1]);
}


AudioBufferTest::AudioBufferTest() : CppUnit::TestCase("Audio Buffer Tests") {}
