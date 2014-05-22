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

#include "mainbuffertest.h"
#include "audio/mainbuffer.h"
#include "logger.h"
#include "test_utils.h"

void MainBufferTest::testBindUnbindBuffer()
{
    TITLE();

    std::string test_id1 = "bind unbind 1";
    std::string test_id2 = "bind unbind 2";

    // bind test_id1 with MainBuffer::DEFAULT_ID (both buffers not already created)
    mainbuffer_->bindCallID(test_id1, MainBuffer::DEFAULT_ID);

    // unbind test_id1 with MainBuffer::DEFAULT_ID
    mainbuffer_->unBindCallID(test_id1, MainBuffer::DEFAULT_ID);

    mainbuffer_->bindCallID(test_id1, MainBuffer::DEFAULT_ID);
    mainbuffer_->bindCallID(test_id1, MainBuffer::DEFAULT_ID);

    mainbuffer_->bindCallID(test_id2, MainBuffer::DEFAULT_ID);
    mainbuffer_->bindCallID(test_id2, MainBuffer::DEFAULT_ID);

    // bind test_id1 with test_id2 (both testid1 and test_id2 already created)
    // calling it twice not supposed to break anything
    mainbuffer_->bindCallID(test_id1, test_id2);
    mainbuffer_->bindCallID(test_id1, test_id2);

    mainbuffer_->unBindCallID(test_id1, test_id2);
    mainbuffer_->unBindCallID(test_id1, test_id2);

    mainbuffer_->unBindCallID(MainBuffer::DEFAULT_ID, test_id2);
    mainbuffer_->unBindCallID(MainBuffer::DEFAULT_ID, test_id2);

    mainbuffer_->unBindCallID(MainBuffer::DEFAULT_ID, test_id1);

    // test unbind all function
    mainbuffer_->bindCallID(MainBuffer::DEFAULT_ID, test_id1);
    mainbuffer_->bindCallID(MainBuffer::DEFAULT_ID, test_id2);
    mainbuffer_->bindCallID(test_id1, test_id2);

    mainbuffer_->unBindAll(test_id2);
}

void MainBufferTest::testGetPutData()
{
    TITLE();

    std::string test_id = "incoming rtp session";

    mainbuffer_->bindCallID(test_id, MainBuffer::DEFAULT_ID);

    SFLAudioSample test_sample1 = 12;
    SFLAudioSample test_sample2 = 13;

    AudioBuffer test_input1(&test_sample1, 1, AudioFormat::MONO);
    AudioBuffer test_input2(&test_sample2, 1, AudioFormat::MONO);
    AudioBuffer test_output(100, AudioFormat::MONO);

    // get by test_id without preleminary put
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output, test_id) == 0);

    // put by MainBuffer::DEFAULT_ID, get by test_id
    mainbuffer_->putData(test_input1, MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output, test_id) == 1);
    CPPUNIT_ASSERT(test_sample1 == (*test_output.getChannel(0))[0]);

    // get by MainBuffer::DEFAULT_ID without preleminary put
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output, MainBuffer::DEFAULT_ID) == 0);

    // put by test_id, get by MainBuffer::DEFAULT_ID
    mainbuffer_->putData(test_input2, test_id);
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output, MainBuffer::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(test_sample2 == (*test_output.getChannel(0))[0]);

    mainbuffer_->unBindCallID(test_id, MainBuffer::DEFAULT_ID);

}

void MainBufferTest::testGetAvailableData()
{
    TITLE();
    std::string test_id = "getData putData";
    std::string false_id = "false id";

    mainbuffer_->bindCallID(test_id, MainBuffer::DEFAULT_ID);

    SFLAudioSample test_sample1 = 12;
    SFLAudioSample test_sample2 = 13;

    AudioBuffer test_input1(&test_sample1, 1, AudioFormat::MONO);
    AudioBuffer test_input2(&test_sample2, 1, AudioFormat::MONO);
    AudioBuffer test_output(1, AudioFormat::MONO);
    AudioBuffer test_output_large(100, AudioFormat::MONO);

    // put by MainBuffer::DEFAULT_ID get by test_id without preleminary put
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_->getAvailableData(test_output, test_id) == 0);

    // put by MainBuffer::DEFAULT_ID, get by test_id
    mainbuffer_->putData(test_input1, MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 1);

    // get by MainBuffer::DEFAULT_ID without preliminary input
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output_large, test_id) == 0);

    // put by test_id get by test_id
    mainbuffer_->putData(test_input2, test_id);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 1);
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output_large, test_id) == 1);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT((*test_output_large.getChannel(0))[0] == test_sample2);

    // put/get by false id
    mainbuffer_->putData(test_input2, false_id);
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output_large, false_id) == 0);

    mainbuffer_->unBindCallID(test_id, MainBuffer::DEFAULT_ID);
}



void MainBufferTest::testDiscardFlush()
{
    TITLE();
    std::string test_id = "flush discard";

    mainbuffer_->bindCallID(test_id, MainBuffer::DEFAULT_ID);

    SFLAudioSample test_sample1 = 12;
    AudioBuffer test_input1(&test_sample1, 1, AudioFormat::MONO);

    mainbuffer_->putData(test_input1, test_id);
    mainbuffer_->discard(1, MainBuffer::DEFAULT_ID);

    mainbuffer_->discard(1, test_id);

    mainbuffer_->putData(test_input1, MainBuffer::DEFAULT_ID);

    mainbuffer_->discard(1, test_id);

    mainbuffer_->unBindCallID(test_id, MainBuffer::DEFAULT_ID);
}

void MainBufferTest::testConference()
{
    TITLE();

    std::string test_id1 = "participant A";
    std::string test_id2 = "participant B";

    // test bind Participant A with default
    mainbuffer_->bindCallID(test_id1, MainBuffer::DEFAULT_ID);

    // test bind Participant B with default
    mainbuffer_->bindCallID(test_id2, MainBuffer::DEFAULT_ID);
    // test bind Participant A with Participant B
    mainbuffer_->bindCallID(test_id1, test_id2);
    // test putData default
    SFLAudioSample testint = 12;
    AudioBuffer testbuf(&testint, 1, AudioFormat::MONO);

    // put data test ring buffers
    mainbuffer_->putData(testbuf, MainBuffer::DEFAULT_ID);
    //putdata test ring buffers
    mainbuffer_->putData(testbuf, test_id1);

    mainbuffer_->putData(testbuf, test_id2);
}

MainBufferTest::MainBufferTest() : CppUnit::TestCase("Audio Layer Tests"), mainbuffer_(new MainBuffer) {}
