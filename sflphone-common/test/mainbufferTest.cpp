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
    CallID test_id = "1234";
    RingBuffer* test_ring_buffer = _mainbuffer.createRingBuffer(test_id);

    CPPUNIT_ASSERT(test_ring_buffer != NULL);
    CPPUNIT_ASSERT(_mainbuffer.getRingBuffer("null id") == NULL);
    CPPUNIT_ASSERT(_mainbuffer.getRingBuffer(test_id) == test_ring_buffer);

    CPPUNIT_ASSERT(_mainbuffer.removeRingBuffer("null id") == false);
    CPPUNIT_ASSERT(_mainbuffer.removeRingBuffer(test_id) == true);

}


void MainBufferTest::testRingbufferInt()
{

    CallID test_id = "test_int";
    RingBuffer* test_ring_buffer = _mainbuffer.createRingBuffer(test_id);
    
    int testint1 = 12;
    int testint2 = 13;

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->Len() == sizeof(int));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint2, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->Len() == 2*sizeof(int));

    int testget;

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(test_ring_buffer->Len() == sizeof(int));
    CPPUNIT_ASSERT(testget == testint1);

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(int)) == sizeof(int));
    CPPUNIT_ASSERT(testget == testint2);
    CPPUNIT_ASSERT(test_ring_buffer->Len() == 0);

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testint1, sizeof(int)) == sizeof(int));
    test_ring_buffer->flush();
    CPPUNIT_ASSERT(test_ring_buffer->Len() == 0);

    _mainbuffer.removeRingBuffer(test_id);
}


void MainBufferTest::testRingbufferFloat()
{

    CallID test_id = "test_float";
    RingBuffer* test_ring_buffer = _mainbuffer.createRingBuffer(test_id);

    float testfloat1 = 12.5;
    float testfloat2 = 13.4;

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testfloat1, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(test_ring_buffer->Len() == sizeof(float));

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testfloat2, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(test_ring_buffer->Len() == 2*sizeof(float));

    float testget;

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(test_ring_buffer->Len() == sizeof(float));
    CPPUNIT_ASSERT(testget == testfloat1);

    CPPUNIT_ASSERT(test_ring_buffer->Get(&testget, sizeof(float)) == sizeof(float));
    CPPUNIT_ASSERT(testget == testfloat2);
    CPPUNIT_ASSERT(test_ring_buffer->Len() == 0);

    CPPUNIT_ASSERT(test_ring_buffer->Put(&testfloat1, sizeof(float)) == sizeof(float));
    test_ring_buffer->flush();
    CPPUNIT_ASSERT(test_ring_buffer->Len() == 0);

    _mainbuffer.removeRingBuffer(test_id);
}


void MainBufferTest::testTwoPointer()
{


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
    
    CallID test_id = "incoming rtp session";
    _mainbuffer.createRingBuffer(test_id);

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
}


void MainBufferTest::testAvailForGetPut()
{

    CallID test_id = "avail for get";
    _mainbuffer.createRingBuffer(test_id);

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

}


void MainBufferTest::testDiscardFlush()
{

    CallID test_id = "flush discard";
    _mainbuffer.createRingBuffer(test_id);

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

}
