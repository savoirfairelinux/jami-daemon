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
