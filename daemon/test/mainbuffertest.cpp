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
#include "audio/ringbuffer.h"
#include "logger.h"
#include "test_utils.h"

typedef std::map<std::string, std::shared_ptr<RingBuffer> > RingBufferMap;
typedef std::map<std::string, std::shared_ptr<CallIDSet> > CallIDMap;

void MainBufferTest::testRingBufferCreation()
{
    TITLE();
    std::string test_id = "1234";
    std::string null_id = "null id";

    RingBuffer* test_ring_buffer;
    RingBufferMap::iterator iter;

    // test mainbuffer ringbuffer map size
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.empty());
    mainbuffer_->createRingBuffer(test_id);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id).get();
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 1);

    // test mainbuffer_->getRingBuffer method
    CPPUNIT_ASSERT(test_ring_buffer != NULL);
    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(null_id) == NULL);
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 1);
    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(test_id).get() == test_ring_buffer);

    // test mainbuffer_ ringBufferMap_
    iter = mainbuffer_->ringBufferMap_.find(null_id);
    CPPUNIT_ASSERT(iter == mainbuffer_->ringBufferMap_.end());
    iter = mainbuffer_->ringBufferMap_.find(test_id);
    CPPUNIT_ASSERT(iter->first == test_id);
    CPPUNIT_ASSERT(iter->second.get() == test_ring_buffer);
    CPPUNIT_ASSERT(iter->second == mainbuffer_->getRingBuffer(test_id));

    // test creating twice a buffer (should not create it)
    mainbuffer_->createRingBuffer(test_id);
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 1);
    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(test_id).get() == test_ring_buffer);

    // test remove ring buffer
    mainbuffer_->removeRingBuffer(null_id);
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 1);
    mainbuffer_->removeRingBuffer(test_id);
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.empty());
    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(test_id) == NULL);

    iter = mainbuffer_->ringBufferMap_.find(test_id);
    CPPUNIT_ASSERT(iter == mainbuffer_->ringBufferMap_.end());

}


void MainBufferTest::testRingBufferReadPointer()
{
    TITLE();
    std::string call_id = "call id";
    std::string read_id = "read id";
    std::string null_id = "null id";
    std::string other_id = "other id";

    RingBuffer* test_ring_buffer;

    // test ring buffer read pointers (one per participant)
    mainbuffer_->createRingBuffer(call_id);
    test_ring_buffer = mainbuffer_->getRingBuffer(call_id).get();
    CPPUNIT_ASSERT(test_ring_buffer->hasNoReadPointers());
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(read_id) == (int) NULL);

    // create a read pointer
    test_ring_buffer->createReadPointer(read_id);
    CPPUNIT_ASSERT(!test_ring_buffer->hasNoReadPointers());
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(null_id) == (int) NULL);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(read_id) == 0);

    // store read pointer
    test_ring_buffer->storeReadPointer(4, read_id);
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(read_id) == 4);

    // recreate the same read pointer (should not add a pointer neither chage its value)
    test_ring_buffer->createReadPointer(read_id);
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    test_ring_buffer->storeReadPointer(8, read_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(read_id) == 8);

    // test getSmallest read pointer (to get the length available to put data in the buffer)
    test_ring_buffer->createReadPointer(other_id);
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    test_ring_buffer->storeReadPointer(4, other_id);
    CPPUNIT_ASSERT(test_ring_buffer->getSmallestReadPointer() == 4);

    // remove read pointers
    test_ring_buffer->removeReadPointer(other_id);
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    test_ring_buffer->removeReadPointer(read_id);
    CPPUNIT_ASSERT(test_ring_buffer->hasNoReadPointers());
}


void MainBufferTest::testCallIDSet()
{
    TITLE();

    std::string test_id = "set id";
    std::string false_id = "false set id";
    // CallIDSet* callid_set = 0;

    CallIDMap::iterator iter_map;
    CallIDSet::iterator iter_set;

    std::string call_id_1 = "call id 1";
    std::string call_id_2 = "call id 2";

    // test initial settings
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.empty());
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.empty());
    iter_map = mainbuffer_->callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map == mainbuffer_->callIDMap_.end());

    // test callidset creation
    mainbuffer_->createCallIDSet(test_id);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 1);
    iter_map = mainbuffer_->callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->first == test_id);
    CPPUNIT_ASSERT(iter_map->second == mainbuffer_->getCallIDSet(test_id));

    CPPUNIT_ASSERT(mainbuffer_->getCallIDSet(false_id) == NULL);
    CPPUNIT_ASSERT(mainbuffer_->getCallIDSet(test_id) != NULL);

    // Test callIDSet add call_ids
    mainbuffer_->addCallIDtoSet(test_id, call_id_1);
    iter_map = mainbuffer_->callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 1);
    iter_set = iter_map->second->find(call_id_1);
    CPPUNIT_ASSERT(*iter_set == call_id_1);

    // test add second call id to set
    mainbuffer_->addCallIDtoSet(test_id, call_id_2);
    iter_map = mainbuffer_->callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 2);
    iter_set = iter_map->second->find(call_id_2);
    CPPUNIT_ASSERT(*iter_set == call_id_2);

    // test add a call id twice
    mainbuffer_->addCallIDtoSet(test_id, call_id_2);
    iter_map = mainbuffer_->callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 2);
    iter_set = iter_map->second->find(call_id_2);
    CPPUNIT_ASSERT(*iter_set == call_id_2);

    // test remove a call id
    mainbuffer_->removeCallIDfromSet(test_id, call_id_2);
    iter_map = mainbuffer_->callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 1);
    iter_set = iter_map->second->find(call_id_1);
    CPPUNIT_ASSERT(*iter_set == call_id_1);
    iter_set = iter_map->second->find(call_id_2);
    CPPUNIT_ASSERT(iter_set == iter_map->second->end());

    // test remove a call id twice
    mainbuffer_->removeCallIDfromSet(test_id, call_id_2);
    iter_map = mainbuffer_->callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 1);
    iter_set = iter_map->second->find(call_id_1);
    CPPUNIT_ASSERT(*iter_set == call_id_1);
    iter_set = iter_map->second->find(call_id_2);
    CPPUNIT_ASSERT(iter_set == iter_map->second->end());

    // Test removeCallIDSet
    mainbuffer_->removeCallIDSet(false_id);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 1);
    mainbuffer_->removeCallIDSet(test_id);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.empty());

    iter_map = mainbuffer_->callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map == mainbuffer_->callIDMap_.end());
}


void MainBufferTest::testRingBufferInt()
{
    TITLE();

    SFLAudioSample testsample1 = 12;
    SFLAudioSample testsample2[] = {13, 14, 15, 16, 17, 18};

    AudioBuffer testbuf1(&testsample1, 1, AudioFormat::MONO); // 1 sample, 1 channel
    AudioBuffer testbuf2(testsample2, 3, AudioFormat::STEREO); // 3 samples, 2 channels

    // test with default ring buffer
    mainbuffer_->createRingBuffer(MainBuffer::DEFAULT_ID);
    RingBuffer *test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();

    // initial state
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 0);

    // add some data
    test_ring_buffer->put(testbuf1); // +1 sample
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 0);

    // add some other data
    test_ring_buffer->put(testbuf2); // +3 samples
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 4);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 0);

    AudioBuffer testget(&testsample1, 1, AudioFormat::MONO);

    // get some data (without any read pointers)
    CPPUNIT_ASSERT(test_ring_buffer->hasNoReadPointers());
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 4);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(MainBuffer::DEFAULT_ID) == 4);
    CPPUNIT_ASSERT(test_ring_buffer->get(testget, MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 4);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(MainBuffer::DEFAULT_ID) == 4);
    CPPUNIT_ASSERT((*testget.getChannel(0))[0] == testsample1);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 0);

    // get some data (with a read pointer)
    CPPUNIT_ASSERT(test_ring_buffer->hasNoReadPointers());
    test_ring_buffer->createReadPointer(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(MainBuffer::DEFAULT_ID) == 0);

    // add some data
    test_ring_buffer->put(testbuf1); // +1 sample
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 4);

    // add some other data
    test_ring_buffer->put(testbuf2); // +3 samples
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 4);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 4);

    CPPUNIT_ASSERT(test_ring_buffer->get(testget, MainBuffer::DEFAULT_ID) == 1);

    // test flush data
    test_ring_buffer->put(testbuf1);

    test_ring_buffer->flush(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 9);

    // test flush data
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    test_ring_buffer->put(testbuf1);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(MainBuffer::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 9);

    test_ring_buffer->discard(1, MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 10);
}


void MainBufferTest::testRingBufferNonDefaultID()
{
    TITLE();

    std::string test_id = "test_int";

    SFLAudioSample testsample1 = 12;
    SFLAudioSample testsample2[] = {13, 14, 15, 16, 17, 18};

    AudioBuffer testbuf1(&testsample1, 1, AudioFormat::MONO); // 1 sample, 1 channel
    AudioBuffer testbuf2(testsample2, 3, AudioFormat::STEREO); // 3 samples, 2 channels

    // test putData, getData with arbitrary read pointer id
    mainbuffer_->createRingBuffer(MainBuffer::DEFAULT_ID);
    RingBuffer* test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    test_ring_buffer->createReadPointer(test_id);

    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 0);

    test_ring_buffer->put(testbuf1);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 0);

    test_ring_buffer->put(testbuf2);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 4);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 0);

    AudioBuffer testget(1, AudioFormat::MONO);
    AudioBuffer testgetlarge(100, AudioFormat::MONO);

    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id) == 4);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_id) == 4);
    CPPUNIT_ASSERT(test_ring_buffer->get(testget, test_id) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id) == 3);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_id) == 3);
    CPPUNIT_ASSERT((*testget.getChannel(0))[0] == testsample1);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 1);

    CPPUNIT_ASSERT(test_ring_buffer->get(testgetlarge, test_id) == 3);
    CPPUNIT_ASSERT((*testgetlarge.getChannel(0))[1] == testsample2[2]);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 4);


    // test flush data
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    test_ring_buffer->put(testbuf1);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_id) == 1);

    test_ring_buffer->flush(test_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 5);

    // test flush data
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    test_ring_buffer->put(testbuf1);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_id) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 5);

    test_ring_buffer->discard(1, test_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 6);

    test_ring_buffer->removeReadPointer(test_id);
}

/*
void MainBufferTest::testRingBufferFloat()
{
    TITLE();
    float testfloat1 = 12.5;
    float testfloat2 = 13.4;

    mainbuffer_->createRingBuffer(MainBuffer::DEFAULT_ID);
    RingBuffer* test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID);
    test_ring_buffer->createReadPointer(MainBuffer::DEFAULT_ID);


    test_ring_buffer->put(&testfloat1, sizeof(float));
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == sizeof(float));

    test_ring_buffer->put(&testfloat2, sizeof(float));
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 2*sizeof(float));

    float testget;

    CPPUNIT_ASSERT(test_ring_buffer->get(&testget, sizeof(float), MainBuffer::DEFAULT_ID) == sizeof(float));
    CPPUNIT_ASSERT(test_ring_buffer->getLength(MainBuffer::DEFAULT_ID) == sizeof(float));
    CPPUNIT_ASSERT(testget == testfloat1);

    CPPUNIT_ASSERT(test_ring_buffer->get(&testget, sizeof(float), MainBuffer::DEFAULT_ID) == sizeof(float));
    CPPUNIT_ASSERT(testget == testfloat2);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(MainBuffer::DEFAULT_ID) == 0);

    test_ring_buffer->put(&testfloat1, sizeof(float));
    test_ring_buffer->flush(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);

}*/


void MainBufferTest::testTwoPointer()
{
    TITLE();
    mainbuffer_->createRingBuffer(MainBuffer::DEFAULT_ID);
    RingBuffer* input_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    input_buffer->createReadPointer(MainBuffer::DEFAULT_ID);
    RingBuffer* output_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();

    SFLAudioSample test_sample = 12;

    AudioBuffer test_input(&test_sample, 1, AudioFormat::MONO);
    AudioBuffer test_output(1, AudioFormat::MONO);

    input_buffer->put(test_input);
    CPPUNIT_ASSERT(output_buffer->get(test_output, MainBuffer::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(test_sample == (*test_output.getChannel(0))[0]);

}

void MainBufferTest::testBindUnbindBuffer()
{
    TITLE();

    std::string test_id1 = "bind unbind 1";
    std::string test_id2 = "bind unbind 2";

    RingBufferMap::iterator iter_buffer;
    CallIDMap::iterator iter_idset;
    CallIDSet::iterator iter_id;

    ReadPointer::iterator iter_readpointer;

    RingBuffer* ringbuffer;

    // test initial state with no ring brffer created
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 0);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 0);

    iter_buffer = mainbuffer_->ringBufferMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer == mainbuffer_->ringBufferMap_.end());
    iter_idset = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_idset == mainbuffer_->callIDMap_.end());

    // bind test_id1 with MainBuffer::DEFAULT_ID (both buffer not already created)
    mainbuffer_->bindCallID(test_id1, MainBuffer::DEFAULT_ID);

    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 2);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 2);

    iter_buffer = mainbuffer_->ringBufferMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(test_id1));

    iter_idset = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);

    iter_idset = mainbuffer_->callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_id == MainBuffer::DEFAULT_ID);

    ringbuffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);


    // unbind test_id1 with MainBuffer::DEFAULT_ID
    mainbuffer_->unBindCallID(test_id1, MainBuffer::DEFAULT_ID);

    DEBUG("%i", (int)(mainbuffer_->ringBufferMap_.size()));
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 0);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 0);

    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID) == NULL);
    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(test_id1) == NULL);


    // bind test_id2 with MainBuffer::DEFAULT_ID (MainBuffer::DEFAULT_ID already created)
    // calling it twice not supposed to break anything
    mainbuffer_->bindCallID(test_id1, MainBuffer::DEFAULT_ID);
    mainbuffer_->bindCallID(test_id1, MainBuffer::DEFAULT_ID);

    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 2);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 2);

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer == mainbuffer_->ringBufferMap_.end());
    iter_idset = mainbuffer_->callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset == mainbuffer_->callIDMap_.end());

    mainbuffer_->bindCallID(test_id2, MainBuffer::DEFAULT_ID);
    mainbuffer_->bindCallID(test_id2, MainBuffer::DEFAULT_ID);

    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 3);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 3);

    iter_buffer = mainbuffer_->ringBufferMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(test_id1));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer->first == test_id2);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(test_id2));

    iter_idset = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_id == test_id2);

    iter_idset = mainbuffer_->callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_id == MainBuffer::DEFAULT_ID);

    iter_idset = mainbuffer_->callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_id == MainBuffer::DEFAULT_ID);

    ringbuffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_->getRingBuffer(test_id2).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    // bind test_id1 with test_id2 (both testid1 and test_id2 already created)
    // calling it twice not supposed to break anything
    mainbuffer_->bindCallID(test_id1, test_id2);
    mainbuffer_->bindCallID(test_id1, test_id2);

    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 3);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 3);

    iter_buffer = mainbuffer_->ringBufferMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(test_id1));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer->first == test_id2);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(test_id2));

    iter_idset = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_id == test_id2);

    iter_idset = mainbuffer_->callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_id == MainBuffer::DEFAULT_ID);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_id == test_id2);

    iter_idset = mainbuffer_->callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_id == MainBuffer::DEFAULT_ID);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);

    ringbuffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_->getRingBuffer(test_id2).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    // unbind test_id1 with test_id2
    // calling it twice not supposed to break anything
    mainbuffer_->unBindCallID(test_id1, test_id2);
    mainbuffer_->unBindCallID(test_id1, test_id2);

    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 3);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 3);

    iter_buffer = mainbuffer_->ringBufferMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(test_id1));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer->first == test_id2);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(test_id2));

    iter_idset = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_id == test_id2);

    iter_idset = mainbuffer_->callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_id == MainBuffer::DEFAULT_ID);

    iter_idset = mainbuffer_->callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_id == MainBuffer::DEFAULT_ID);

    ringbuffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_->getRingBuffer(test_id2).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);


    DEBUG("ok1");

    // unbind test_id1 with test_id2
    // calling it twice not supposed to break anything
    mainbuffer_->unBindCallID(MainBuffer::DEFAULT_ID, test_id2);
    mainbuffer_->unBindCallID(MainBuffer::DEFAULT_ID, test_id2);

    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 2);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 2);

    iter_buffer = mainbuffer_->ringBufferMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(test_id1));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer == mainbuffer_->ringBufferMap_.end());

    iter_idset = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(iter_id == iter_idset->second->end());

    iter_idset = mainbuffer_->callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_id == MainBuffer::DEFAULT_ID);

    iter_idset = mainbuffer_->callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset == mainbuffer_->callIDMap_.end());

    ringbuffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer == ringbuffer->readpointers_.end());

    ringbuffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer == ringbuffer->readpointers_.end());

    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(test_id2) == NULL);


    mainbuffer_->unBindCallID(MainBuffer::DEFAULT_ID, test_id1);

    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 0);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 0);

    // test unbind all function
    mainbuffer_->bindCallID(MainBuffer::DEFAULT_ID, test_id1);
    mainbuffer_->bindCallID(MainBuffer::DEFAULT_ID, test_id2);
    mainbuffer_->bindCallID(test_id1, test_id2);
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 3);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 3);

    mainbuffer_->unBindAll(test_id2);
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 2);
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 2);

    iter_buffer = mainbuffer_->ringBufferMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_->getRingBuffer(test_id1));

    iter_buffer = mainbuffer_->ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer == mainbuffer_->ringBufferMap_.end());

    iter_idset = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(iter_id == iter_idset->second->end());

    iter_idset = mainbuffer_->callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_id == MainBuffer::DEFAULT_ID);

    iter_idset = mainbuffer_->callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset == mainbuffer_->callIDMap_.end());

    ringbuffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer == ringbuffer->readpointers_.end());

    ringbuffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(not ringbuffer->hasNoReadPointers());
    iter_readpointer = ringbuffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer == ringbuffer->readpointers_.end());


}

void MainBufferTest::testGetPutDataByID()
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
    CPPUNIT_ASSERT(mainbuffer_->availableForGetByID(MainBuffer::DEFAULT_ID, test_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_->getDataByID(test_output, MainBuffer::DEFAULT_ID, test_id) == 0);

    // put by MainBuffer::DEFAULT_ID, get by test_id
    mainbuffer_->putData(test_input1, MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(mainbuffer_->availableForGetByID(MainBuffer::DEFAULT_ID, test_id) == 1);

    // get by MainBuffer::DEFAULT_ID without preliminary input
    CPPUNIT_ASSERT(mainbuffer_->availableForGetByID(test_id, MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(mainbuffer_->getDataByID(test_output_large, test_id, MainBuffer::DEFAULT_ID) == 0);

    // put by test_id get by test_id
    mainbuffer_->putData(test_input2, test_id);
    CPPUNIT_ASSERT(mainbuffer_->availableForGetByID(test_id, MainBuffer::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(mainbuffer_->getDataByID(test_output_large, test_id, MainBuffer::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(mainbuffer_->availableForGetByID(test_id, MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT((*test_output_large.getChannel(0))[0] == test_sample2);

    // put/get by false id
    mainbuffer_->putData(test_input2, false_id);
    CPPUNIT_ASSERT(mainbuffer_->getDataByID(test_output_large, false_id, false_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_->getDataByID(test_output_large, MainBuffer::DEFAULT_ID, false_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_->getDataByID(test_output_large, false_id, MainBuffer::DEFAULT_ID) == 0);

    mainbuffer_->unBindCallID(test_id, MainBuffer::DEFAULT_ID);
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
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output, test_id) == 0);

    // put by MainBuffer::DEFAULT_ID, get by test_id
    mainbuffer_->putData(test_input1, MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 1);
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output, test_id) == 1);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 0);
    CPPUNIT_ASSERT(test_sample1 == (*test_output.getChannel(0))[0]);

    // get by MainBuffer::DEFAULT_ID without preleminary put
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output, MainBuffer::DEFAULT_ID) == 0);

    // put by test_id, get by MainBuffer::DEFAULT_ID
    mainbuffer_->putData(test_input2, test_id);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(MainBuffer::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(mainbuffer_->getData(test_output, MainBuffer::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_sample2 == (*test_output.getChannel(0))[0]);

    mainbuffer_->unBindCallID(test_id, MainBuffer::DEFAULT_ID);

}


void MainBufferTest::testDiscardFlush()
{
    TITLE();
    std::string test_id = "flush discard";
    // mainbuffer_->createRingBuffer(test_id);
    mainbuffer_->bindCallID(test_id, MainBuffer::DEFAULT_ID);

    SFLAudioSample test_sample1 = 12;
    AudioBuffer test_input1(&test_sample1, 1, AudioFormat::MONO);

    mainbuffer_->putData(test_input1, test_id);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(MainBuffer::DEFAULT_ID) == 1);
    mainbuffer_->discard(1, MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(MainBuffer::DEFAULT_ID) == 0);

    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 0);
    mainbuffer_->discard(1, test_id);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id) == 0);

    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(test_id)->getReadPointer(MainBuffer::DEFAULT_ID) == 1);
    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(test_id)->getReadPointer(test_id) == 0);


    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID)->getReadPointer(test_id) == 0);
    mainbuffer_->putData(test_input1, MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID)->getReadPointer(test_id) == 0);

    mainbuffer_->discard(1, test_id);
    CPPUNIT_ASSERT(mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID)->getReadPointer(test_id) == 1);

    mainbuffer_->unBindCallID(test_id, MainBuffer::DEFAULT_ID);
}


void MainBufferTest::testReadPointerInit()
{
    TITLE();
    std::string test_id = "test read pointer init";

    mainbuffer_->bindCallID(test_id, MainBuffer::DEFAULT_ID);

    RingBuffer* test_ring_buffer = mainbuffer_->getRingBuffer(test_id).get();

    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 0);
    test_ring_buffer->storeReadPointer(30, MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(MainBuffer::DEFAULT_ID) == 30);

    test_ring_buffer->createReadPointer(test_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 0);
    test_ring_buffer->storeReadPointer(10, test_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 10);
    test_ring_buffer->removeReadPointer(test_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == (int) NULL);
    test_ring_buffer->removeReadPointer("false id");

    // mainbuffer_->removeRingBuffer(test_id);
    mainbuffer_->unBindCallID(test_id, MainBuffer::DEFAULT_ID);
}


void MainBufferTest::testRingBufferSeveralPointers()
{
    TITLE();

    std::string test_id = "test multiple read pointer";
    mainbuffer_->createRingBuffer(test_id);
    RingBuffer* test_ring_buffer = mainbuffer_->getRingBuffer(test_id).get();

    std::string test_pointer1 = "test pointer 1";
    std::string test_pointer2 = "test pointer 2";

    test_ring_buffer->createReadPointer(test_pointer1);
    test_ring_buffer->createReadPointer(test_pointer2);

    SFLAudioSample testint1 = 12;
    SFLAudioSample testint2 = 13;
    SFLAudioSample testint3 = 14;
    SFLAudioSample testint4 = 15;
    AudioBuffer test_input1(&testint1, 1, AudioFormat::MONO);
    AudioBuffer test_input2(&testint2, 1, AudioFormat::MONO);
    AudioBuffer test_input3(&testint3, 1, AudioFormat::MONO);
    AudioBuffer test_input4(&testint4, 1, AudioFormat::MONO);

    AudioBuffer test_output(1, AudioFormat::MONO);

    test_ring_buffer->put(test_input1);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_pointer1) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_pointer2) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer1) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer2) == 1);

    test_ring_buffer->put(test_input2);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 2);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_pointer1) == 2);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_pointer2) == 2);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer1) == 2);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer2) == 2);

    test_ring_buffer->put(test_input3);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 3);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_pointer1) == 3);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_pointer2) == 3);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer1) == 3);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer2) == 3);

    test_ring_buffer->put(test_input4);
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 4);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_pointer1) == 4);
    CPPUNIT_ASSERT(test_ring_buffer->getLength(test_pointer2) == 4);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer1) == 4);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer2) == 4);


    CPPUNIT_ASSERT(test_ring_buffer->get(test_output, test_pointer1) == 1);
    CPPUNIT_ASSERT((*test_output.getChannel(0))[0] == testint1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer1) == 3);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer2) == 4);

    CPPUNIT_ASSERT(test_ring_buffer->get(test_output, test_pointer2) == 1);
    CPPUNIT_ASSERT((*test_output.getChannel(0))[0] == testint1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer1) == 3);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_pointer2) == 3);

    // However, we should no be alowed to read in our own ring buffer
    // if we are either an AudioLayer or an RTP session
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 4);

    // However, we should no be alowed to read in our own ring buffer
    // if we are either an AudioLayer or an RTP session
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 4);

    // However, we should no be alowed to read in our own ring buffer
    // if we are either an AudioLayer or an RTP session
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 4);

    CPPUNIT_ASSERT(test_ring_buffer->discard(1, test_pointer1) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->discard(1, test_pointer2) == 1);

    test_ring_buffer->removeReadPointer(test_pointer1);
    test_ring_buffer->removeReadPointer(test_pointer2);

    mainbuffer_->removeRingBuffer(test_id);
}



void MainBufferTest::testConference()
{
    TITLE();

    std::string test_id1 = "participant A";
    std::string test_id2 = "participant B";

    RingBufferMap::iterator iter_ringbuffermap;
    ReadPointer::iterator iter_readpointer;
    CallIDMap::iterator iter_callidmap;
    CallIDSet::iterator iter_callidset;

    // test initial setup
    // ringbuffers
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 0);
    RingBuffer* test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(test_ring_buffer == NULL);

    // callidmap
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 0);
    iter_callidmap = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_callidmap == mainbuffer_->callIDMap_.end());


    // test bind Participant A with default
    mainbuffer_->bindCallID(test_id1, MainBuffer::DEFAULT_ID);
    // ringbuffers
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 2);
    test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    iter_readpointer = test_ring_buffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    iter_readpointer = test_ring_buffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    // callidmap
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 2);
    iter_callidmap = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_callidmap->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 1);
    iter_callidset = iter_callidmap->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_callidset == test_id1);
    iter_callidmap = mainbuffer_->callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id1);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 1);
    iter_callidset = iter_callidmap->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_callidset == MainBuffer::DEFAULT_ID);

    // test bind Participant B with default
    mainbuffer_->bindCallID(test_id2, MainBuffer::DEFAULT_ID);
    // ringbuffers
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 3);
    test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    iter_readpointer = test_ring_buffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = test_ring_buffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    iter_readpointer = test_ring_buffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id2).get();
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    iter_readpointer = test_ring_buffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    // callidmap
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 3);
    iter_callidmap = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_callidmap->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 2);
    iter_callidset = iter_callidmap->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_callidset == test_id1);
    iter_callidset = iter_callidmap->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_callidset == test_id2);
    iter_callidmap = mainbuffer_->callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id1);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 1);
    iter_callidset = iter_callidmap->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_callidset == MainBuffer::DEFAULT_ID);
    iter_callidmap = mainbuffer_->callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id2);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 1);
    iter_callidset = iter_callidmap->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_callidset == MainBuffer::DEFAULT_ID);


    // test bind Participant A with Participant B
    mainbuffer_->bindCallID(test_id1, test_id2);
    // ringbuffers
    CPPUNIT_ASSERT(mainbuffer_->ringBufferMap_.size() == 3);
    test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    iter_readpointer = test_ring_buffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = test_ring_buffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    iter_readpointer = test_ring_buffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = test_ring_buffer->readpointers_.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id2).get();
    CPPUNIT_ASSERT(not test_ring_buffer->hasNoReadPointers());
    iter_readpointer = test_ring_buffer->readpointers_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = test_ring_buffer->readpointers_.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    // callidmap
    CPPUNIT_ASSERT(mainbuffer_->callIDMap_.size() == 3);
    iter_callidmap = mainbuffer_->callIDMap_.find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_callidmap->first == MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 2);
    iter_callidset = iter_callidmap->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_callidset == test_id1);
    iter_callidset = iter_callidmap->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_callidset == test_id2);
    iter_callidmap = mainbuffer_->callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id1);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 2);
    iter_callidset = iter_callidmap->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_callidset == MainBuffer::DEFAULT_ID);
    iter_callidset = iter_callidmap->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_callidset == test_id2);
    iter_callidmap = mainbuffer_->callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id2);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 2);
    iter_callidset = iter_callidmap->second->find(MainBuffer::DEFAULT_ID);
    CPPUNIT_ASSERT(*iter_callidset == MainBuffer::DEFAULT_ID);
    iter_callidset = iter_callidmap->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_callidset == test_id1);


    // test putData default
    SFLAudioSample testint = 12;
    AudioBuffer testbuf(&testint, 1, AudioFormat::MONO);

    CPPUNIT_ASSERT(mainbuffer_->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id2) == 0);
    // put data test ring buffers
    mainbuffer_->putData(testbuf, MainBuffer::DEFAULT_ID);
    test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id1) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id2) == 1);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id1).get();
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id2).get();
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id1) == 0);
    // test mainbuffer availforget (get data even if some participant missing)
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id1) == 1);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id2) == 1);
    //putdata test ring buffers
    mainbuffer_->putData(testbuf, test_id1);
    test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id1) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id2) == 1);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id1).get();
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id2).get();
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(MainBuffer::DEFAULT_ID) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id1) == 0);

    mainbuffer_->putData(testbuf, test_id2);
    test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id1) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id2) == 1);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id1).get();
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id2).get();
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id1) == 1);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id2) == 1);

    // test getData default id (audio layer)
    test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id1) == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id2) == 1);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id1).get();
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id2).get();
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id1) == 1);
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id2) == 1);
    // test getData test_id1 (audio layer)
    test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    CPPUNIT_ASSERT(test_ring_buffer->putLength() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->availableForGet(test_id2) == 1);
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id1).get();
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id2).get();

    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_->availableForGet(test_id2) == 1);
    // test getData test_id2 (audio layer)
    test_ring_buffer = mainbuffer_->getRingBuffer(MainBuffer::DEFAULT_ID).get();
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id1).get();
    test_ring_buffer = mainbuffer_->getRingBuffer(test_id2).get();
}

MainBufferTest::MainBufferTest() : CppUnit::TestCase("Audio Layer Tests"), mainbuffer_(new MainBuffer) {}
