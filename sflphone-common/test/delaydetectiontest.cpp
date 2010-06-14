/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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


#include "delaydetectiontest.h"

#include <iostream>
#include <math.h>

void DelayDetectionTest::setUp() {}

void DelayDetectionTest::tearDown() {}

void DelayDetectionTest::testCrossCorrelation() 
{
    double signal[10] = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0,  7.0, 8.0, 9.0};
    double ref[3] = {0.0, 1.0, 2.0};

    double result[10];
    double expected[10] = {0.0, 0.89442719, 1.0, 0.95618289, 0.91350028, 0.88543774, 0.86640023, 0.85280287, 0.8426548, 0.83480969};

    CPPUNIT_ASSERT(_delaydetect.correlate(ref, ref, 3) == 5.0);
    CPPUNIT_ASSERT(_delaydetect.correlate(signal, signal, 10) == 285.0);
    
    _delaydetect.crossCorrelate(ref, signal, result, 3, 10);

    std::cout << std::endl;

    double tmp;
    for (int i = 0; i < 10; i++) {
        tmp = result[i]-expected[i];
        if(tmp < 0.0)
	  CPPUNIT_ASSERT (tmp > -0.001);
	else
	  CPPUNIT_ASSERT(tmp < 0.001);
    }


}

