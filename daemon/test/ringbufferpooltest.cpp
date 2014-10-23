/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
#include <memory>

#include "ringbufferpooltest.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "logger.h"
#include "test_utils.h"

using namespace sfl;

void RingBufferPoolTest::testBindUnbindBuffer()
{
    TITLE();

    std::string test_id1 = "bind unbind 1";
    std::string test_id2 = "bind unbind 2";

    // bind test_id1 with RingBufferPool::DEFAULT_ID (test_id1 not already created)
    rbPool_->bindCallID(test_id1, RingBufferPool::DEFAULT_ID);

    // unbind test_id1 with RingBufferPool::DEFAULT_ID
    rbPool_->unBindCallID(test_id1, RingBufferPool::DEFAULT_ID);

    rbPool_->bindCallID(test_id1, RingBufferPool::DEFAULT_ID);
    rbPool_->bindCallID(test_id1, RingBufferPool::DEFAULT_ID);

    rbPool_->bindCallID(test_id2, RingBufferPool::DEFAULT_ID);
    rbPool_->bindCallID(test_id2, RingBufferPool::DEFAULT_ID);

    // bind test_id1 with test_id2 (both testid1 and test_id2 already created)
    // calling it twice not supposed to break anything
    rbPool_->bindCallID(test_id1, test_id2);
    rbPool_->bindCallID(test_id1, test_id2);

    rbPool_->unBindCallID(test_id1, test_id2);
    rbPool_->unBindCallID(test_id1, test_id2);

    rbPool_->unBindCallID(RingBufferPool::DEFAULT_ID, test_id2);
    rbPool_->unBindCallID(RingBufferPool::DEFAULT_ID, test_id2);

    rbPool_->unBindCallID(RingBufferPool::DEFAULT_ID, test_id1);

    // test unbind all function
    rbPool_->bindCallID(RingBufferPool::DEFAULT_ID, test_id1);
    rbPool_->bindCallID(RingBufferPool::DEFAULT_ID, test_id2);
    rbPool_->bindCallID(test_id1, test_id2);

    rbPool_->unBindAll(test_id2);
}

void RingBufferPoolTest::testGetPutData()
{
    TITLE();

    std::string test_id = "incoming rtp session";

    auto mainRingBuffer = rbPool_->getRingBuffer(RingBufferPool::DEFAULT_ID);
    auto testRingBuffer = rbPool_->createRingBuffer(test_id);

    rbPool_->bindCallID(test_id, RingBufferPool::DEFAULT_ID);

    SFLAudioSample test_sample1 = 12;
    SFLAudioSample test_sample2 = 13;

    AudioBuffer test_input1(&test_sample1, 1, AudioFormat::MONO());
    AudioBuffer test_input2(&test_sample2, 1, AudioFormat::MONO());
    AudioBuffer test_output(100, AudioFormat::MONO());

    // get by test_id without preleminary put
    CPPUNIT_ASSERT(rbPool_->getData(test_output, test_id) == 0);

    // put by RingBufferPool::DEFAULT_ID, get by test_id
    mainRingBuffer->put(test_input1);
    CPPUNIT_ASSERT(rbPool_->getData(test_output, test_id) == 1);
    CPPUNIT_ASSERT(test_sample1 == (*test_output.getChannel(0))[0]);

    // get by RingBufferPool::DEFAULT_ID without preleminary put
    CPPUNIT_ASSERT(rbPool_->getData(test_output, RingBufferPool::DEFAULT_ID) == 0);

    // put by test_id, get by RingBufferPool::DEFAULT_ID
    testRingBuffer->put(test_input2);
    CPPUNIT_ASSERT(rbPool_->getData(test_output, RingBufferPool::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(test_sample2 == (*test_output.getChannel(0))[0]);

    rbPool_->unBindCallID(test_id, RingBufferPool::DEFAULT_ID);
}

void RingBufferPoolTest::testGetAvailableData()
{
    TITLE();
    std::string test_id = "getData putData";
    std::string false_id = "false id";

    auto mainRingBuffer = rbPool_->getRingBuffer(RingBufferPool::DEFAULT_ID);
    auto testRingBuffer = rbPool_->createRingBuffer(test_id);

    rbPool_->bindCallID(test_id, RingBufferPool::DEFAULT_ID);

    SFLAudioSample test_sample1 = 12;
    SFLAudioSample test_sample2 = 13;

    AudioBuffer test_input1(&test_sample1, 1, AudioFormat::MONO());
    AudioBuffer test_input2(&test_sample2, 1, AudioFormat::MONO());
    AudioBuffer test_output(1, AudioFormat::MONO());
    AudioBuffer test_output_large(100, AudioFormat::MONO());

    // put by RingBufferPool::DEFAULT_ID get by test_id without preleminary put
    CPPUNIT_ASSERT(rbPool_->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT(rbPool_->getAvailableData(test_output, test_id) == 0);

    // put by RingBufferPool::DEFAULT_ID, get by test_id
    mainRingBuffer->put(test_input1);
    CPPUNIT_ASSERT(rbPool_->availableForGet(test_id) == 1);

    // get by RingBufferPool::DEFAULT_ID without preliminary input
    CPPUNIT_ASSERT(rbPool_->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT(rbPool_->getData(test_output_large, test_id) == 0);

    // put by test_id get by test_id
    testRingBuffer->put(test_input2);
    CPPUNIT_ASSERT(rbPool_->availableForGet(test_id) == 1);
    CPPUNIT_ASSERT(rbPool_->getData(test_output_large, test_id) == 1);
    CPPUNIT_ASSERT(rbPool_->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT((*test_output_large.getChannel(0))[0] == test_sample2);

    // get by false id
    CPPUNIT_ASSERT(rbPool_->getData(test_output_large, false_id) == 0);

    rbPool_->unBindCallID(test_id, RingBufferPool::DEFAULT_ID);
}

void RingBufferPoolTest::testDiscardFlush()
{
    TITLE();
    std::string test_id = "flush discard";

    auto mainRingBuffer = rbPool_->getRingBuffer(RingBufferPool::DEFAULT_ID);
    auto testRingBuffer = rbPool_->createRingBuffer(test_id);

    rbPool_->bindCallID(test_id, RingBufferPool::DEFAULT_ID);

    SFLAudioSample test_sample1 = 12;
    AudioBuffer test_input1(&test_sample1, 1, AudioFormat::MONO());

    testRingBuffer->put(test_input1);
    rbPool_->discard(1, RingBufferPool::DEFAULT_ID);

    rbPool_->discard(1, test_id);

    mainRingBuffer->put(test_input1);

    rbPool_->discard(1, test_id);

    rbPool_->unBindCallID(test_id, RingBufferPool::DEFAULT_ID);
}

void RingBufferPoolTest::testConference()
{
    TITLE();

    std::string test_id1 = "participant A";
    std::string test_id2 = "participant B";

    auto mainRingBuffer = rbPool_->getRingBuffer(RingBufferPool::DEFAULT_ID);
    auto testRingBuffer1 = rbPool_->createRingBuffer(test_id1);
    auto testRingBuffer2 = rbPool_->createRingBuffer(test_id2);

    // test bind Participant A with default
    rbPool_->bindCallID(test_id1, RingBufferPool::DEFAULT_ID);

    // test bind Participant B with default
    rbPool_->bindCallID(test_id2, RingBufferPool::DEFAULT_ID);

    // test bind Participant A with Participant B
    rbPool_->bindCallID(test_id1, test_id2);

    SFLAudioSample testint = 12;
    AudioBuffer testbuf(&testint, 1, AudioFormat::MONO());

    // put data test ring buffers
    mainRingBuffer->put(testbuf);

    // put data test ring buffers
    testRingBuffer1->put(testbuf);
    testRingBuffer2->put(testbuf);
}

RingBufferPoolTest::RingBufferPoolTest()
    : CppUnit::TestCase("Audio Layer Tests") , rbPool_(new RingBufferPool)
{}
