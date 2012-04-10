/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "mainbuffertest.h"

void MainBufferTest::testRingBufferCreation()
{
    TITLE();
    std::string test_id = "1234";
    std::string null_id = "null id";

    RingBuffer* test_ring_buffer;
    RingBufferMap::iterator iter;

    // test mainbuffer ringbuffer map size
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.empty());
    test_ring_buffer = mainbuffer_.createRingBuffer(test_id);
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 1);

    // test mainbuffer_.getRingBuffer method
    CPPUNIT_ASSERT(test_ring_buffer != NULL);
    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(null_id) == NULL);
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 1);
    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(test_id) == test_ring_buffer);

    // test mainbuffer_ ringBufferMap_
    iter = mainbuffer_.ringBufferMap_.find(null_id);
    CPPUNIT_ASSERT(iter == mainbuffer_.ringBufferMap_.end());
    iter = mainbuffer_.ringBufferMap_.find(test_id);
    CPPUNIT_ASSERT(iter->first == test_id);
    CPPUNIT_ASSERT(iter->second == test_ring_buffer);
    CPPUNIT_ASSERT(iter->second == mainbuffer_.getRingBuffer(test_id));

    // test creating twice a buffer (should not create it)
    mainbuffer_.createRingBuffer(test_id);
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 1);
    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(test_id) == test_ring_buffer);

    // test remove ring buffer
    CPPUNIT_ASSERT(mainbuffer_.removeRingBuffer(null_id));
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 1);
    CPPUNIT_ASSERT(mainbuffer_.removeRingBuffer(test_id));
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.empty());
    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(test_id) == NULL);

    iter = mainbuffer_.ringBufferMap_.find(test_id);
    CPPUNIT_ASSERT(iter == mainbuffer_.ringBufferMap_.end());

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
    test_ring_buffer = mainbuffer_.createRingBuffer(call_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(read_id) == (int) NULL);

    // create a read pointer
    test_ring_buffer->createReadPointer(read_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(null_id) == (int) NULL);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(read_id) == 0);

    // store read pointer
    test_ring_buffer->storeReadPointer(4, read_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(read_id) == 4);

    // recreate the same read pointer (should not add a pointer neither chage its value)
    test_ring_buffer->createReadPointer(read_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 1);
    test_ring_buffer->storeReadPointer(8, read_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(read_id) == 8);

    // test getSmallest read pointer (to get the length available to put data in the buffer)
    test_ring_buffer->createReadPointer(other_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 2);
    test_ring_buffer->storeReadPointer(4, other_id);
    CPPUNIT_ASSERT(test_ring_buffer->getSmallestReadPointer() == 4);

    // remove read pointers
    test_ring_buffer->removeReadPointer(other_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 1);
    test_ring_buffer->removeReadPointer(read_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 0);
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
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.empty());
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.empty());
    iter_map = mainbuffer_.callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map ==mainbuffer_.callIDMap_.end());

    // test callidset creation
    mainbuffer_.createCallIDSet(test_id);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 1);
    iter_map = mainbuffer_.callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->first == test_id);
    CPPUNIT_ASSERT(iter_map->second == mainbuffer_.getCallIDSet(test_id));

    CPPUNIT_ASSERT(mainbuffer_.getCallIDSet(false_id) == NULL);
    CPPUNIT_ASSERT(mainbuffer_.getCallIDSet(test_id) != NULL);


    // Test callIDSet add call_ids
    mainbuffer_.addCallIDtoSet(test_id, call_id_1);
    iter_map = mainbuffer_.callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 1);
    iter_set = iter_map->second->find(call_id_1);
    CPPUNIT_ASSERT(*iter_set == call_id_1);

    // test add second call id to set
    mainbuffer_.addCallIDtoSet(test_id, call_id_2);
    iter_map = mainbuffer_.callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 2);
    iter_set = iter_map->second->find(call_id_2);
    CPPUNIT_ASSERT(*iter_set == call_id_2);

    // test add a call id twice
    mainbuffer_.addCallIDtoSet(test_id, call_id_2);
    iter_map = mainbuffer_.callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 2);
    iter_set = iter_map->second->find(call_id_2);
    CPPUNIT_ASSERT(*iter_set == call_id_2);

    // test remove a call id
    mainbuffer_.removeCallIDfromSet(test_id, call_id_2);
    iter_map = mainbuffer_.callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 1);
    iter_set = iter_map->second->find(call_id_1);
    CPPUNIT_ASSERT(*iter_set == call_id_1);
    iter_set = iter_map->second->find(call_id_2);
    CPPUNIT_ASSERT(iter_set == iter_map->second->end());

    // test remove a call id twice
    mainbuffer_.removeCallIDfromSet(test_id, call_id_2);
    iter_map = mainbuffer_.callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map->second->size() == 1);
    iter_set = iter_map->second->find(call_id_1);
    CPPUNIT_ASSERT(*iter_set == call_id_1);
    iter_set = iter_map->second->find(call_id_2);
    CPPUNIT_ASSERT(iter_set == iter_map->second->end());

    // Test removeCallIDSet
    CPPUNIT_ASSERT(!mainbuffer_.removeCallIDSet(false_id));
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 1);
    CPPUNIT_ASSERT(mainbuffer_.removeCallIDSet(test_id));
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.empty());

    iter_map = mainbuffer_.callIDMap_.find(test_id);
    CPPUNIT_ASSERT(iter_map == mainbuffer_.callIDMap_.end());
}


void MainBufferTest::testRingBufferInt()
{
    TITLE();

    // CallID test_id = "test_int";

    int testint1 = 12;
    int testint2 = 13;

    // test with default ring buffer
    RingBuffer* test_ring_buffer = mainbuffer_.createRingBuffer(default_id);

    // initial state
    int init_put_size = test_ring_buffer->AvailForPut();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 0);

    // add some data
    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 0);

    // add some other data
    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint2, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - 2* (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 0);

    int testget = (int) NULL;

    // get some data (without any read pointers)
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(int)) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == 2*sizeof(int));
    CPPUNIT_ASSERT(testget == (int) NULL);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - 2* (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 0);

    // get some data (with a read pointer)
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 0);
    test_ring_buffer->createReadPointer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == 0);

    // add some data
    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 2*sizeof(int));

    // add some other data
    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint2, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - 2* (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 2*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(int), 100, default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == sizeof(int));
    CPPUNIT_ASSERT(testget == testint1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 3*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(testget == testint2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_size);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 4*sizeof(int));


    // test flush data
    init_put_size = test_ring_buffer->AvailForPut();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == sizeof(int));


    test_ring_buffer->flush();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_size);
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 5*sizeof(int));

    // test flush data
    init_put_size = test_ring_buffer->AvailForPut();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 5*sizeof(int));

    test_ring_buffer->Discard(sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_size);
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 6*sizeof(int));


}


void MainBufferTest::testRingBufferNonDefaultID()
{
    TITLE();

    std::string test_id = "test_int";

    int testint1 = 12;
    int testint2 = 13;
    int init_put_size;


    // test putData, getData with arbitrary read pointer id
    RingBuffer* test_ring_buffer = mainbuffer_.createRingBuffer(default_id);
    test_ring_buffer->createReadPointer(test_id);

    init_put_size = test_ring_buffer->AvailForPut();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(default_id) == 0);

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(default_id) == 0);

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint2, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - 2* (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(default_id) == 0);

    int testget;

    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_id) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(int), test_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_id) == sizeof(int));
    CPPUNIT_ASSERT(testget == testint1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(int), 100, test_id) == sizeof(int));
    CPPUNIT_ASSERT(testget == testint2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_size);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 2*sizeof(int));


    // test flush data
    init_put_size = test_ring_buffer->AvailForPut();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_id) == sizeof(int));


    test_ring_buffer->flush(test_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_size);
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 3*sizeof(int));

    // test flush data
    init_put_size = test_ring_buffer->AvailForPut();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (init_put_size - (int) sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 3*sizeof(int));

    test_ring_buffer->Discard(sizeof(int), test_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_size);
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 4*sizeof(int));

    test_ring_buffer->removeReadPointer(test_id);

}


void MainBufferTest::testRingBufferFloat()
{
    TITLE();
    float testfloat1 = 12.5;
    float testfloat2 = 13.4;

    RingBuffer* test_ring_buffer = mainbuffer_.createRingBuffer(default_id);
    test_ring_buffer->createReadPointer(default_id);


    CPPUNIT_ASSERT(test_ring_buffer->Put(&testfloat1, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(float));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testfloat2, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 2*sizeof(float));

    float testget;

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == sizeof(float));
    CPPUNIT_ASSERT(testget == testfloat1);

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(testget == testfloat2);
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == 0);

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testfloat1, sizeof(float)) == sizeof(float));
    test_ring_buffer->flush();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);

}


void MainBufferTest::testTwoPointer()
{
    TITLE();
    RingBuffer* input_buffer = mainbuffer_.createRingBuffer(default_id);
    input_buffer->createReadPointer(default_id);
    RingBuffer* output_buffer = mainbuffer_.getRingBuffer(default_id);

    int test_input = 12;
    int test_output;

    CPPUNIT_ASSERT(input_buffer->Put(&test_input, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(output_buffer->Get(&test_output, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(test_input == test_output);

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
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 0);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 0);

    iter_buffer = mainbuffer_.ringBufferMap_.find(default_id);
    CPPUNIT_ASSERT(iter_buffer == mainbuffer_.ringBufferMap_.end());
    iter_idset = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_idset == mainbuffer_.callIDMap_.end());

    // bind test_id1 with default_id (both buffer not already created)
    mainbuffer_.bindCallID(test_id1);

    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 2);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 2);

    iter_buffer = mainbuffer_.ringBufferMap_.find(default_id);
    CPPUNIT_ASSERT(iter_buffer->first == default_id);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(default_id));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(test_id1));

    iter_idset = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);

    iter_idset = mainbuffer_.callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(default_id);
    CPPUNIT_ASSERT(*iter_id == default_id);

    ringbuffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);


    // unbind test_id1 with default_id
    mainbuffer_.unBindCallID(test_id1);

    DEBUG("%i", (int)(mainbuffer_.ringBufferMap_.size()));
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 0);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 0);

    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(default_id) == NULL);
    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(test_id1) == NULL);


    // bind test_id2 with default_id (default_id already created)
    // calling it twice not supposed to break anything
    mainbuffer_.bindCallID(test_id1, default_id);
    mainbuffer_.bindCallID(test_id1, default_id);

    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 2);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 2);

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer == mainbuffer_.ringBufferMap_.end());
    iter_idset = mainbuffer_.callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset == mainbuffer_.callIDMap_.end());

    mainbuffer_.bindCallID(test_id2, default_id);
    mainbuffer_.bindCallID(test_id2, default_id);

    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 3);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 3);

    iter_buffer = mainbuffer_.ringBufferMap_.find(default_id);
    CPPUNIT_ASSERT(iter_buffer->first == default_id);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(default_id));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(test_id1));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer->first == test_id2);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(test_id2));

    iter_idset = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_id == test_id2);

    iter_idset = mainbuffer_.callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(default_id);
    CPPUNIT_ASSERT(*iter_id == default_id);

    iter_idset = mainbuffer_.callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(default_id);
    CPPUNIT_ASSERT(*iter_id == default_id);

    ringbuffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 2);
    iter_readpointer = ringbuffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    // bind test_id1 with test_id2 (both testid1 and test_id2 already created)
    // calling it twice not supposed to break anything
    mainbuffer_.bindCallID(test_id1, test_id2);
    mainbuffer_.bindCallID(test_id1, test_id2);

    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 3);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 3);

    iter_buffer = mainbuffer_.ringBufferMap_.find(default_id);
    CPPUNIT_ASSERT(iter_buffer->first == default_id);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(default_id));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(test_id1));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer->first == test_id2);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(test_id2));

    iter_idset = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_id == test_id2);

    iter_idset = mainbuffer_.callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(default_id);
    CPPUNIT_ASSERT(*iter_id == default_id);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_id == test_id2);

    iter_idset = mainbuffer_.callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(default_id);
    CPPUNIT_ASSERT(*iter_id == default_id);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);

    ringbuffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 2);
    iter_readpointer = ringbuffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 2);
    iter_readpointer = ringbuffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 2);
    iter_readpointer = ringbuffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    // unbind test_id1 with test_id2
    // calling it twice not supposed to break anything
    mainbuffer_.unBindCallID(test_id1, test_id2);
    mainbuffer_.unBindCallID(test_id1, test_id2);

    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 3);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 3);

    iter_buffer = mainbuffer_.ringBufferMap_.find(default_id);
    CPPUNIT_ASSERT(iter_buffer->first == default_id);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(default_id));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(test_id1));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer->first == test_id2);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(test_id2));

    iter_idset = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_idset->second->size() == 2);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_id == test_id2);

    iter_idset = mainbuffer_.callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(default_id);
    CPPUNIT_ASSERT(*iter_id == default_id);

    iter_idset = mainbuffer_.callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(default_id);
    CPPUNIT_ASSERT(*iter_id == default_id);

    ringbuffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 2);
    iter_readpointer = ringbuffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);

    ringbuffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);


    DEBUG("ok1");

    // unbind test_id1 with test_id2
    // calling it twice not supposed to break anything
    mainbuffer_.unBindCallID(default_id, test_id2);
    mainbuffer_.unBindCallID(default_id, test_id2);

    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 2);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 2);

    iter_buffer = mainbuffer_.ringBufferMap_.find(default_id);
    CPPUNIT_ASSERT(iter_buffer->first == default_id);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(default_id));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(test_id1));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer == mainbuffer_.ringBufferMap_.end());

    iter_idset = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(iter_id == iter_idset->second->end());

    iter_idset = mainbuffer_.callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(default_id);
    CPPUNIT_ASSERT(*iter_id == default_id);

    iter_idset = mainbuffer_.callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset == mainbuffer_.callIDMap_.end());

    ringbuffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer == ringbuffer->_readpointer.end());

    ringbuffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer == ringbuffer->_readpointer.end());

    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(test_id2) == NULL);


    mainbuffer_.unBindCallID(default_id, test_id1);

    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 0);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 0);

    // test unbind all function
    mainbuffer_.bindCallID(default_id, test_id1);
    mainbuffer_.bindCallID(default_id, test_id2);
    mainbuffer_.bindCallID(test_id1, test_id2);
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 3);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 3);

    mainbuffer_.unBindAll(test_id2);
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 2);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 2);

    iter_buffer = mainbuffer_.ringBufferMap_.find(default_id);
    CPPUNIT_ASSERT(iter_buffer->first == default_id);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(default_id));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_buffer->first == test_id1);
    CPPUNIT_ASSERT(iter_buffer->second == mainbuffer_.getRingBuffer(test_id1));

    iter_buffer = mainbuffer_.ringBufferMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_buffer == mainbuffer_.ringBufferMap_.end());

    iter_idset = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_id == test_id1);
    iter_id = iter_idset->second->find(test_id2);
    CPPUNIT_ASSERT(iter_id == iter_idset->second->end());

    iter_idset = mainbuffer_.callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_idset->second->size() == 1);
    iter_id = iter_idset->second->find(default_id);
    CPPUNIT_ASSERT(*iter_id == default_id);

    iter_idset = mainbuffer_.callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_idset == mainbuffer_.callIDMap_.end());

    ringbuffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer == ringbuffer->_readpointer.end());

    ringbuffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(ringbuffer != NULL);
    CPPUNIT_ASSERT(ringbuffer->getNbReadPointer() == 1);
    iter_readpointer = ringbuffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = ringbuffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer == ringbuffer->_readpointer.end());


}

void MainBufferTest::testGetPutDataByID()
{
    TITLE();
    std::string test_id = "getData putData";
    std::string false_id = "false id";

    mainbuffer_.bindCallID(test_id);

    int test_input1 = 12;
    int test_input2 = 13;
    int test_output;

    int avail_for_put_testid;
    int avail_for_put_defaultid;

    // put by default_id get by test_id without preleminary put
    avail_for_put_defaultid = mainbuffer_.availForPut();
    CPPUNIT_ASSERT(mainbuffer_.availForGetByID(default_id, test_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.getDataByID(&test_output, sizeof(int), default_id, test_id) == 0);

    // put by default_id, get by test_id
    CPPUNIT_ASSERT(mainbuffer_.availForPut() == avail_for_put_defaultid);
    CPPUNIT_ASSERT(mainbuffer_.putData(&test_input1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForPut() == (avail_for_put_defaultid - (int) sizeof(int)));
    CPPUNIT_ASSERT(mainbuffer_.availForGetByID(default_id, test_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.getDataByID(&test_output, sizeof(int), 100, default_id, test_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGetByID(default_id, test_id) == 0);
    CPPUNIT_ASSERT(test_input1 == test_output);

    // get by default_id without preliminary input
    avail_for_put_testid = mainbuffer_.availForPut(test_id);
    CPPUNIT_ASSERT(mainbuffer_.availForGetByID(test_id, default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.getDataByID(&test_output, sizeof(int), 100, test_id, default_id) == 0);

    // pu by test_id get by test_id
    CPPUNIT_ASSERT(mainbuffer_.availForPut(test_id) == avail_for_put_defaultid);
    CPPUNIT_ASSERT(mainbuffer_.putData(&test_input2, sizeof(int), test_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGetByID(test_id, default_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.getDataByID(&test_output, sizeof(int), 100, test_id, default_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGetByID(test_id, default_id) == 0);
    CPPUNIT_ASSERT(test_input2 == test_output);

    // put/get by false id
    CPPUNIT_ASSERT(mainbuffer_.putData(&test_input2, sizeof(int), false_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.getDataByID(&test_input2, sizeof(int), 100, false_id, false_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.getDataByID(&test_input2, sizeof(int), 100, default_id, false_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.getDataByID(&test_input2, sizeof(int), 100, false_id, default_id) == 0);

    mainbuffer_.unBindCallID(test_id);

}


void MainBufferTest::testGetPutData()
{
    TITLE();

    std::string test_id = "incoming rtp session";

    mainbuffer_.bindCallID(test_id);

    int test_input1 = 12;
    int test_input2 = 13;
    int test_output;

    int avail_for_put_testid;
    int avail_for_put_defaultid;

    // get by test_id without preleminary put
    avail_for_put_defaultid = mainbuffer_.availForPut();
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.getData(&test_output, sizeof(int), 100,  test_id) == 0);

    // put by default_id, get by test_id
    CPPUNIT_ASSERT(mainbuffer_.availForPut() == avail_for_put_defaultid);
    CPPUNIT_ASSERT(mainbuffer_.putData(&test_input1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForPut() == (avail_for_put_defaultid - (int) sizeof(int)));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.getData(&test_output, sizeof(int), 100, test_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id) == 0);
    CPPUNIT_ASSERT(test_input1 == test_output);

    // get by default_id without preleminary put
    avail_for_put_testid = mainbuffer_.availForPut(test_id);
    CPPUNIT_ASSERT(mainbuffer_.availForGet() == 0);
    CPPUNIT_ASSERT(mainbuffer_.getData(&test_output, sizeof(int)) == 0);

    // put by test_id, get by default_id
    CPPUNIT_ASSERT(mainbuffer_.availForPut(test_id) == avail_for_put_testid);
    CPPUNIT_ASSERT(mainbuffer_.putData(&test_input2, sizeof(int), test_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForPut(test_id) == (avail_for_put_testid - (int) sizeof(int)));
    CPPUNIT_ASSERT(mainbuffer_.availForGet() == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.getData(&test_output, sizeof(int), 100) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet() == 0);
    CPPUNIT_ASSERT(test_input2 == test_output);

    mainbuffer_.unBindCallID(test_id);

}


void MainBufferTest::testDiscardFlush()
{
    TITLE();
    std::string test_id = "flush discard";
    // mainbuffer_.createRingBuffer(test_id);
    mainbuffer_.bindCallID(test_id);

    int test_input1 = 12;
    // int test_output_size;
    // int init_size;

    CPPUNIT_ASSERT(mainbuffer_.putData(&test_input1, sizeof(int), test_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet() == sizeof(int));
    mainbuffer_.discard(sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet() == 0);

    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id) == 0);
    mainbuffer_.discard(sizeof(int), test_id);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id) == 0);

    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(test_id)->getReadPointer(default_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(test_id)->getReadPointer(test_id) == 0);


    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(default_id)->getReadPointer(test_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.putData(&test_input1, sizeof(int), 100) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(default_id)->getReadPointer(test_id) == 0);

    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id) == sizeof(int));

    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(default_id)->getReadPointer(test_id) == 0);
    mainbuffer_.discard(sizeof(int), test_id);
    CPPUNIT_ASSERT(mainbuffer_.getRingBuffer(default_id)->getReadPointer(test_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id) == 0);

    // mainbuffer_.removeRingBuffer(test_id);
    mainbuffer_.unBindCallID(test_id);

}


void MainBufferTest::testReadPointerInit()
{
    TITLE();
    std::string test_id = "test read pointer init";
    // RingBuffer* test_ring_buffer = mainbuffer_.createRingBuffer(test_id);

    mainbuffer_.bindCallID(test_id);

    RingBuffer* test_ring_buffer = mainbuffer_.getRingBuffer(test_id);

    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 0);
    test_ring_buffer->storeReadPointer(30);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 30);

    test_ring_buffer->createReadPointer(test_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 0);
    test_ring_buffer->storeReadPointer(10, test_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 10);
    test_ring_buffer->removeReadPointer(test_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == (int) NULL);
    test_ring_buffer->removeReadPointer("false id");

    // mainbuffer_.removeRingBuffer(test_id);
    mainbuffer_.unBindCallID(test_id);
}


void MainBufferTest::testRingBufferSeveralPointers()
{
    TITLE();

    std::string test_id = "test multiple read pointer";
    RingBuffer* test_ring_buffer = mainbuffer_.createRingBuffer(test_id);

    std::string test_pointer1 = "test pointer 1";
    std::string test_pointer2 = "test pointer 2";

    test_ring_buffer->createReadPointer(test_pointer1);
    test_ring_buffer->createReadPointer(test_pointer2);

    int testint1 = 12;
    int testint2 = 13;
    int testint3 = 14;
    int testint4 = 15;

    int testoutput;

    int initPutLen = test_ring_buffer->AvailForPut();

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer2) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint2, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 2* (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer1) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer2) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 2*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint3, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 3* (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer1) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer2) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 3*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint4, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 4* (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer1) == 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer2) == 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 4*sizeof(int));


    CPPUNIT_ASSERT(test_ring_buffer->Get(&testoutput, sizeof(int), test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(testoutput == testint1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 4* (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 4*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testoutput, sizeof(int), test_pointer2) == sizeof(int));
    CPPUNIT_ASSERT(testoutput == testint1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 3* (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 3*sizeof(int));

    // AvailForPut() is ok but AvailForGet(default_id) is not ok
    // However, we should no be alowed to read in our own ring buffer
    // if we are either an AudioLayer or and RTP session
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 4*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testoutput, sizeof(int), 100, test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(testoutput == testint2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 3* (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 3*sizeof(int));

    // AvailForPut() is ok but AvailForGet(default_id) is not ok
    // However, we should no be alowed to read in our own ring buffer
    // if we are either an AudioLayer or and RTP session
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 4*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testoutput, sizeof(int), 100, test_pointer2) == sizeof(int));
    CPPUNIT_ASSERT(testoutput == testint2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 2* (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 2*sizeof(int));

    // AvailForPut() is ok but AvailForGet(default_id) is not ok
    // However, we should no be alowed to read in our own ring buffer
    // if we are either an AudioLayer or and RTP session
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 4*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Discard(sizeof(int), test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 2* (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 2*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Discard(sizeof(int), test_pointer2) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - (int) sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == sizeof(int));


    test_ring_buffer->removeReadPointer(test_pointer1);
    test_ring_buffer->removeReadPointer(test_pointer2);

    mainbuffer_.removeRingBuffer(test_id);
}



void MainBufferTest::testConference()
{
    TITLE();

    std::string test_id1 = "participant A";
    std::string test_id2 = "participant B";
    RingBuffer* test_ring_buffer;

    RingBufferMap::iterator iter_ringbuffermap;
    ReadPointer::iterator iter_readpointer;
    CallIDMap::iterator iter_callidmap;
    CallIDSet::iterator iter_callidset;



    // test initial setup
    // ringbuffers
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer == NULL);

    // callidmap
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 0);
    iter_callidmap = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_callidmap == mainbuffer_.callIDMap_.end());


    // test bind Participant A with default
    mainbuffer_.bindCallID(test_id1);
    // ringbuffers
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 2);
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 1);
    iter_readpointer = test_ring_buffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 1);
    iter_readpointer = test_ring_buffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    // callidmap
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 2);
    iter_callidmap = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_callidmap->first == default_id);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 1);
    iter_callidset = iter_callidmap->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_callidset == test_id1);
    iter_callidmap = mainbuffer_.callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id1);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 1);
    iter_callidset = iter_callidmap->second->find(default_id);
    CPPUNIT_ASSERT(*iter_callidset == default_id);

    // test bind Participant B with default
    mainbuffer_.bindCallID(test_id2);
    // ringbuffers
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 3);
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 2);
    iter_readpointer = test_ring_buffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = test_ring_buffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 1);
    iter_readpointer = test_ring_buffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 1);
    iter_readpointer = test_ring_buffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    // callidmap
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 3);
    iter_callidmap = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_callidmap->first == default_id);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 2);
    iter_callidset = iter_callidmap->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_callidset == test_id1);
    iter_callidset = iter_callidmap->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_callidset == test_id2);
    iter_callidmap = mainbuffer_.callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id1);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 1);
    iter_callidset = iter_callidmap->second->find(default_id);
    CPPUNIT_ASSERT(*iter_callidset == default_id);
    iter_callidmap = mainbuffer_.callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id2);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 1);
    iter_callidset = iter_callidmap->second->find(default_id);
    CPPUNIT_ASSERT(*iter_callidset == default_id);


    // test bind Participant A with Participant B
    mainbuffer_.bindCallID(test_id1, test_id2);
    // ringbuffers
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 3);
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 2);
    iter_readpointer = test_ring_buffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = test_ring_buffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 2);
    iter_readpointer = test_ring_buffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = test_ring_buffer->_readpointer.find(test_id2);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id2);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->getNbReadPointer() == 2);
    iter_readpointer = test_ring_buffer->_readpointer.find(default_id);
    CPPUNIT_ASSERT(iter_readpointer->first == default_id);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    iter_readpointer = test_ring_buffer->_readpointer.find(test_id1);
    CPPUNIT_ASSERT(iter_readpointer->first == test_id1);
    CPPUNIT_ASSERT(iter_readpointer->second == 0);
    // callidmap
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 3);
    iter_callidmap = mainbuffer_.callIDMap_.find(default_id);
    CPPUNIT_ASSERT(iter_callidmap->first == default_id);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 2);
    iter_callidset = iter_callidmap->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_callidset == test_id1);
    iter_callidset = iter_callidmap->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_callidset == test_id2);
    iter_callidmap = mainbuffer_.callIDMap_.find(test_id1);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id1);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 2);
    iter_callidset = iter_callidmap->second->find(default_id);
    CPPUNIT_ASSERT(*iter_callidset == default_id);
    iter_callidset = iter_callidmap->second->find(test_id2);
    CPPUNIT_ASSERT(*iter_callidset == test_id2);
    iter_callidmap = mainbuffer_.callIDMap_.find(test_id2);
    CPPUNIT_ASSERT(iter_callidmap->first == test_id2);
    CPPUNIT_ASSERT(iter_callidmap->second->size() == 2);
    iter_callidset = iter_callidmap->second->find(default_id);
    CPPUNIT_ASSERT(*iter_callidset == default_id);
    iter_callidset = iter_callidmap->second->find(test_id1);
    CPPUNIT_ASSERT(*iter_callidset == test_id1);


    // test putData default
    int testint = 12;
    int init_put_defaultid;
    int init_put_id1;
    int init_put_id2;

    init_put_defaultid = mainbuffer_.getRingBuffer(default_id)->AvailForPut();
    init_put_id1 = mainbuffer_.getRingBuffer(test_id1)->AvailForPut();
    init_put_id2 = mainbuffer_.getRingBuffer(test_id2)->AvailForPut();
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == 0);
    // put data test ring buffers
    CPPUNIT_ASSERT(mainbuffer_.putData(&testint, sizeof(int)) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget (get data even if some participant missing)
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    //putdata test ring buffers
    CPPUNIT_ASSERT(mainbuffer_.putData(&testint, sizeof(int), 100, test_id1) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget (get data even if some participant missing)
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    //putdata test ring buffers
    CPPUNIT_ASSERT(mainbuffer_.putData(&testint, sizeof(int), 100, test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id2 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));


    int test_output;

    // test getData default id (audio layer)
    CPPUNIT_ASSERT(mainbuffer_.getData(&test_output, sizeof(int), 100) == sizeof(int));
    CPPUNIT_ASSERT(test_output == (testint + testint));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id2 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    // test getData test_id1 (audio layer)
    CPPUNIT_ASSERT(mainbuffer_.getData(&test_output, sizeof(int), 100, test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_output == (testint + testint));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    // test getData test_id2 (audio layer)
    CPPUNIT_ASSERT(mainbuffer_.getData(&test_output, sizeof(int), 100, test_id2) == sizeof(int));
    CPPUNIT_ASSERT(test_output == (testint + testint));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_defaultid);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == 0);


    // test putData default (for discarting)
    init_put_defaultid = mainbuffer_.getRingBuffer(default_id)->AvailForPut();
    init_put_id1 = mainbuffer_.getRingBuffer(test_id1)->AvailForPut();
    init_put_id2 = mainbuffer_.getRingBuffer(test_id2)->AvailForPut();
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == 0);
    // put data test ring buffers
    CPPUNIT_ASSERT(mainbuffer_.putData(&testint, sizeof(int), 100) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    //putdata test ring buffers
    CPPUNIT_ASSERT(mainbuffer_.putData(&testint, sizeof(int), 100, test_id1) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    //putdata test ring buffers
    CPPUNIT_ASSERT(mainbuffer_.putData(&testint, sizeof(int), 100, test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id2 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));

    // test discardData default id (audio layer)
    CPPUNIT_ASSERT(mainbuffer_.discard(sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_output == (testint + testint));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id2 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    // test discardData test_id1 (audio layer)
    CPPUNIT_ASSERT(mainbuffer_.discard(sizeof(int), test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_output == (testint + testint));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    // test discardData test_id2 (audio layer)
    CPPUNIT_ASSERT(mainbuffer_.discard(sizeof(int), test_id2) == sizeof(int));
    CPPUNIT_ASSERT(test_output == (testint + testint));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_defaultid);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == 0);


    // test putData default (for flushing)
    init_put_defaultid = mainbuffer_.getRingBuffer(default_id)->AvailForPut();
    init_put_id1 = mainbuffer_.getRingBuffer(test_id1)->AvailForPut();
    init_put_id2 = mainbuffer_.getRingBuffer(test_id2)->AvailForPut();
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == 0);
    // put data test ring buffers
    CPPUNIT_ASSERT(mainbuffer_.putData(&testint, sizeof(int), 100) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    //putdata test ring buffers
    CPPUNIT_ASSERT(mainbuffer_.putData(&testint, sizeof(int), 100, test_id1) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    //putdata test ring buffers
    CPPUNIT_ASSERT(mainbuffer_.putData(&testint, sizeof(int), 100, test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id2 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));

    // test flush default id (audio layer)
    mainbuffer_.flush();
    CPPUNIT_ASSERT(test_output == (testint + testint));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    DEBUG("%i", test_ring_buffer->putLen());
    test_ring_buffer->debug();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id2 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == sizeof(int));
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == sizeof(int));
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    // test flush test_id1 (audio layer)
    mainbuffer_.flush(test_id1);
    CPPUNIT_ASSERT(test_output == (testint + testint));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_defaultid - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == (int)(init_put_id1 - sizeof(int)));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == sizeof(int));
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == sizeof(int));
    // test flush test_id2 (audio layer)
    mainbuffer_.flush(test_id2);
    CPPUNIT_ASSERT(test_output == (testint + testint));
    test_ring_buffer = mainbuffer_.getRingBuffer(default_id);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_defaultid);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id1);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id2) == 0);
    test_ring_buffer = mainbuffer_.getRingBuffer(test_id2);
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == init_put_id2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(default_id) == 0);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_id1) == 0);
    // test mainbuffer availforget
    CPPUNIT_ASSERT(mainbuffer_.availForGet(default_id) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id1) == 0);
    CPPUNIT_ASSERT(mainbuffer_.availForGet(test_id2) == 0);


    mainbuffer_.unBindCallID(test_id1, test_id2);
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 3);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 3);

    mainbuffer_.unBindCallID(test_id1);
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.size() == 2);
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.size() == 2);

    mainbuffer_.unBindCallID(test_id2);
    CPPUNIT_ASSERT(mainbuffer_.ringBufferMap_.empty());
    CPPUNIT_ASSERT(mainbuffer_.callIDMap_.empty());
}
