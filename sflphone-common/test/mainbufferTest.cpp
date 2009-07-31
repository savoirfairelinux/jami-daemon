/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 */

#include <stdio.h>
#include <sstream>
#include <ccrtp/rtp.h>
#include <assert.h>
#include <string>
#include <cstring>
#include <math.h>
#include <dlfcn.h>
#include <iostream>
#include <sstream>


#include "mainbufferTest.h"

#include <unistd.h>


using std::cout;
using std::endl;


void MainBufferTest::setUp()
{

    
}


void MainBufferTest::tearDown()
{

}


void MainBufferTest::testRingBufferCreation()
{
    _debug("MainBufferTest::testRingBufferCreation()\n");

    CallID test_id = "1234";
    RingBuffer* test_ring_buffer = _mainbuffer.createRingBuffer(test_id);

    CPPUNIT_ASSERT(test_ring_buffer != NULL);
    CPPUNIT_ASSERT(_mainbuffer.getRingBuffer("null id") == NULL);
    CPPUNIT_ASSERT(_mainbuffer.getRingBuffer(test_id) == test_ring_buffer);

    CPPUNIT_ASSERT(_mainbuffer.removeRingBuffer("null id") == false);
    CPPUNIT_ASSERT(_mainbuffer.removeRingBuffer(test_id) == true);

}


void MainBufferTest::testCallIDSetCreation()
{
    _debug("MainBufferTest::testCallIDSetCreation()\n");

    CallID test_id = "1234";
    CallID false_id = "false id";
    CallIDSet* callid_set = 0;

    CPPUNIT_ASSERT(_mainbuffer.createCallIDSet(test_id) == true);
    callid_set = _mainbuffer.getCallIDSet(false_id);
    CPPUNIT_ASSERT(callid_set == NULL);
    callid_set = _mainbuffer.getCallIDSet(test_id);
    CPPUNIT_ASSERT(callid_set != NULL);

    CPPUNIT_ASSERT(_mainbuffer.removeCallIDSet(false_id) == false);
    CPPUNIT_ASSERT(_mainbuffer.removeCallIDSet(test_id) == true);
}


void MainBufferTest::testRingBufferInt()
{

    _debug("MainBufferTest::testRingbufferInt()\n");

    CallID test_id = "test_int";
    RingBuffer* test_ring_buffer = _mainbuffer.createRingBuffer(test_id);
    
    int testint1 = 12;
    int testint2 = 13;

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint2, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 2*sizeof(int));

    int testget;

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == sizeof(int));
    CPPUNIT_ASSERT(testget == testint1);

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(testget == testint2);
    CPPUNIT_ASSERT(test_ring_buffer->getLen() == 0);

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    test_ring_buffer->flush();
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 0);

    _mainbuffer.removeRingBuffer(test_id);
}


void MainBufferTest::testRingBufferFloat()
{

    _debug("MainBufferTest::testRingBufferFloat()\n");

    CallID test_id = "test_float";
    RingBuffer* test_ring_buffer = _mainbuffer.createRingBuffer(test_id);

    float testfloat1 = 12.5;
    float testfloat2 = 13.4;

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

    _mainbuffer.removeRingBuffer(test_id);
}


void MainBufferTest::testTwoPointer()
{

    _debug("MainBufferTest::testTwoPointer()\n");

    CallID test_id = "two pointer";
    RingBuffer* input_buffer = _mainbuffer.createRingBuffer(test_id);
    RingBuffer* output_buffer = _mainbuffer.getRingBuffer(test_id);

    int test_input = 12;
    int test_output;

    CPPUNIT_ASSERT(input_buffer->Put(&test_input, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(output_buffer->Get(&test_output, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(test_input == test_output);

    _mainbuffer.removeRingBuffer(test_id);
}


void MainBufferTest::testGetPutData()
{

    _debug("MainBufferTest::testGetPutData()\n");
    
    CallID test_id = "getData putData";
    CallID false_id = "false id";
    _mainbuffer.createRingBuffer(test_id);

    int test_input1 = 12;
    int test_input2 = 13;
    int test_output;

    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(_mainbuffer.getDataByID(&test_output, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_input1 == test_output);

    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input2, sizeof(int), 100, test_id) == sizeof(int));
    CPPUNIT_ASSERT(_mainbuffer.getDataByID(&test_output, sizeof(int), 100, test_id) == sizeof(int));
    CPPUNIT_ASSERT(test_input2 == test_output);

    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input2, sizeof(int), 100, false_id) == 0);
    CPPUNIT_ASSERT(_mainbuffer.getDataByID(&test_input2, sizeof(int), 100, false_id) == 0);

    _mainbuffer.removeRingBuffer(test_id);
}



void MainBufferTest::testGetDataAndCallID()
{

    _debug("MainBufferTest::testGetDataAndCallID()\n");
    
    CallID test_id = "incoming rtp session";
    _mainbuffer.createRingBuffer(test_id);
    _mainbuffer.bindCallID(test_id);

    int test_input1 = 12;
    int test_input2 = 13;
    int test_output;

    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input1, sizeof(int), 100, test_id) == sizeof(int));
    CPPUNIT_ASSERT(_mainbuffer.getData(&test_output, sizeof(int), 100, default_id) == sizeof(int));
    CPPUNIT_ASSERT(test_input1 == test_output);

    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input2, sizeof(int), 100, default_id) == sizeof(int));
    CPPUNIT_ASSERT(_mainbuffer.getData(&test_output, sizeof(int), 100, test_id) == sizeof(int));
    CPPUNIT_ASSERT(test_input2 == test_output);

    _mainbuffer.removeRingBuffer(test_id);
    _mainbuffer.unBindCallID(test_id);
}


void MainBufferTest::testAvailForGetPut()
{

    _debug("MainBufferTest::testAvailForGetPut()\n");

    CallID test_id = "avail for get";
    _mainbuffer.createRingBuffer(test_id);
    _mainbuffer.bindCallID(test_id);

    int test_input1 = 12;
    int test_output_size;
    int init_size;

    init_size = _mainbuffer.availForPut(test_id);
    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input1, sizeof(int), 100, test_id) == sizeof(int));
    test_output_size = _mainbuffer.availForPut(test_id);
    CPPUNIT_ASSERT(test_output_size == (init_size - (int)sizeof(int)));

    init_size = _mainbuffer.availForGetByID(test_id);
    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input1, sizeof(int), 100, test_id) == sizeof(int));
    test_output_size = _mainbuffer.availForGetByID(test_id);
    CPPUNIT_ASSERT(test_output_size == (init_size + (int)sizeof(int)));

    init_size = _mainbuffer.availForGet();
    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input1, sizeof(int), 100, test_id) == sizeof(int));
    test_output_size = _mainbuffer.availForGet();
    CPPUNIT_ASSERT(test_output_size == (init_size + (int)sizeof(int)));

    init_size = _mainbuffer.availForGet(test_id);
    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input1, sizeof(int), 100) == sizeof(int));
    test_output_size = _mainbuffer.availForGet(test_id);
    CPPUNIT_ASSERT(test_output_size == (init_size + (int)sizeof(int)));

    _mainbuffer.removeRingBuffer(test_id);
    _mainbuffer.unBindCallID(test_id);

}


void MainBufferTest::testDiscardFlush()
{

    _debug("MainBufferTest::testDiscardFlush()\n");

    CallID test_id = "flush discard";
    _mainbuffer.createRingBuffer(test_id);
    _mainbuffer.bindCallID(test_id);

    int test_input1 = 12;
    int test_output_size;
    int init_size;

    init_size = _mainbuffer.availForGet(test_id);
    CPPUNIT_ASSERT(_mainbuffer.putData(&test_input1, sizeof(int), 100) == sizeof(int));
    test_output_size = _mainbuffer.availForGet(test_id);
    CPPUNIT_ASSERT(test_output_size == (init_size + (int)sizeof(int)));

    _mainbuffer.discard(sizeof(int), test_id);
    test_output_size = _mainbuffer.availForGet(test_id);
    CPPUNIT_ASSERT(test_output_size == init_size);

    _mainbuffer.removeRingBuffer(test_id);
    _mainbuffer.unBindCallID(test_id);

}


void MainBufferTest::testReadPointerInit()
{

    _debug("MainBufferTest::testReadPointerInit()\n");

    CallID test_id = "test read pointer init";
    RingBuffer* test_ring_buffer = _mainbuffer.createRingBuffer(test_id);

    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 0);
    test_ring_buffer->storeReadPointer(30);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer() == 30);

    test_ring_buffer->createReadPointer(test_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 0);
    test_ring_buffer->storeReadPointer(10, test_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == 10);
    test_ring_buffer->removeReadPointer(test_id);
    CPPUNIT_ASSERT(test_ring_buffer->getReadPointer(test_id) == (int)NULL);
    test_ring_buffer->removeReadPointer("false id");

    _mainbuffer.removeRingBuffer(test_id);
}


void MainBufferTest::testRingBufferSeveralPointers()
{

    _debug("MainBufferTest::testRingBufferSeveralPointers\n");

    CallID test_id = "test multiple read pointer";
    RingBuffer* test_ring_buffer = _mainbuffer.createRingBuffer(test_id);

    CallID test_pointer1 = "test pointer 1";
    CallID test_pointer2 = "test pointer 2";

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
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer2) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint2, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer1) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer2) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 2*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint3, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer1) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer2) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 3*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint4, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->putLen() == 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer1) == 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->getLen(test_pointer2) == 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 4*sizeof(int));


    CPPUNIT_ASSERT(test_ring_buffer->Get(&testoutput, sizeof(int), 100, test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(testoutput == testint1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 4*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 4*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testoutput, sizeof(int), 100, test_pointer2) == sizeof(int));
    CPPUNIT_ASSERT(testoutput == testint1);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 3*sizeof(int));
    
    // AvailForPut() is ok but AvailForGet(default_id) is not ok
    // However, we should no be alowed to read in our own ring buffer 
    // if we are either an AudioLayer or and RTP session
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 4*sizeof(int));
    
    CPPUNIT_ASSERT(test_ring_buffer->Get(&testoutput, sizeof(int), 100, test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(testoutput == testint2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 3*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 3*sizeof(int));

    // AvailForPut() is ok but AvailForGet(default_id) is not ok
    // However, we should no be alowed to read in our own ring buffer 
    // if we are either an AudioLayer or and RTP session
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 4*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testoutput, sizeof(int), 100, test_pointer2) == sizeof(int));
    CPPUNIT_ASSERT(testoutput == testint2);
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 2*sizeof(int));
    
    // AvailForPut() is ok but AvailForGet(default_id) is not ok
    // However, we should no be alowed to read in our own ring buffer 
    // if we are either an AudioLayer or and RTP session
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet() == 4*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Discard(sizeof(int), test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - 2*sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == 2*sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Discard(sizeof(int), test_pointer2) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForPut() == initPutLen - sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer1) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->AvailForGet(test_pointer2) == sizeof(int));

    _mainbuffer.removeRingBuffer(test_id);
}
