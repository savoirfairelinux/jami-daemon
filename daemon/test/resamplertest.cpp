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

#include <iostream>
#include <algorithm>
#include <math.h>

#include "resamplertest.h"

void ResamplerTest::setUp()
{

}

void ResamplerTest::tearDown()
{

}

void ResamplerTest::testUpsamplingRamp()
{
    std::tr1::array<SFLDataFormat, 160> tmpInputBuffer;
    std::tr1::array<SFLDataFormat, 320> tmpOutputBuffer;

    // generate input samples and store them in inputBuffer
    generateRamp();

    std::cout << "Test Upsampling Ramp" << std::endl;
    converter = new SamplerateConverter(44100);

    CPPUNIT_ASSERT(converter != NULL);

    std::copy(inputBuffer.begin(), inputBuffer.begin() + 160, tmpInputBuffer.begin());
    converter->resample(tmpInputBuffer.data(), tmpOutputBuffer.data(), tmpOutputBuffer.size(), 8000, 16000, tmpInputBuffer.size());

    std::cout << "Output Buffer" << std::endl;
    for(int i = 0; i < tmpOutputBuffer.size(); i++)
        std::cout << tmpOutputBuffer[i] << ", ";
    std::cout << std::endl;

    std::cout << "Input Buffer" << std::endl;
    for(int i = 0; i < tmpInputBuffer.size(); i++)
        std::cout << tmpInputBuffer[i] << ", ";
    std::cout << std::endl;

    delete converter;
}

void ResamplerTest::testDownsamplingRamp()
{
    std::tr1::array<SFLDataFormat, 320> tmpInputBuffer;
    std::tr1::array<SFLDataFormat, 160> tmpOutputBuffer;

    generateRamp();

    std::cout << "Test Downsampling Ramp" << std::endl;
    converter = new SamplerateConverter(44100);

    CPPUNIT_ASSERT(converter != NULL);

    std::copy(inputBuffer.begin(), inputBuffer.begin() + 320, tmpInputBuffer.begin());
    converter->resample(tmpInputBuffer.data(), tmpOutputBuffer.data(), tmpOutputBuffer.size(), 16000, 8000, tmpInputBuffer.size());

    std::cout << "Output Buffer" << std::endl;
    for(int i = 0; i < tmpOutputBuffer.size(); i++)
        std::cout << tmpOutputBuffer[i] << ", ";
    std::cout << std::endl;

    std::cout << "Input Buffer" << std::endl;
    for(int i = 0; i < tmpInputBuffer.size(); i++)
        std::cout << tmpInputBuffer[i] << ", ";
    std::cout << std::endl;

    delete converter;
}

void ResamplerTest::testUpsamplingTriangle()
{
    std::tr1::array<SFLDataFormat, 160> tmpInputBuffer;
    std::tr1::array<SFLDataFormat, 320> tmpOutputBuffer;

    generateTriangularSignal();

    std::cout << "Test Upsampling Triangle" << std::endl;
    converter = new SamplerateConverter(44100);

    CPPUNIT_ASSERT(converter != NULL);

    std::copy(inputBuffer.begin(), inputBuffer.begin() + 160, tmpInputBuffer.begin());
    converter->resample(tmpInputBuffer.data(), tmpOutputBuffer.data(), tmpOutputBuffer.size(), 8000, 16000, tmpInputBuffer.size());

    delete converter;
}

void ResamplerTest::testDownsamplingTriangle()
{
    std::tr1::array<SFLDataFormat, 320> tmpInputBuffer;
    std::tr1::array<SFLDataFormat, 160> tmpOutputBuffer;

    generateTriangularSignal();

    std::cout << "Test Downsampling Triangle" << std::endl;
    converter = new SamplerateConverter(44100);

    CPPUNIT_ASSERT(converter != NULL);

    std::copy(inputBuffer.begin(), inputBuffer.end() + 320, tmpInputBuffer.begin());
    converter->resample(tmpInputBuffer.data(), tmpOutputBuffer.data(), tmpOutputBuffer.size(), 16000, 8000, tmpInputBuffer.size());

    delete converter;
}

void ResamplerTest::testUpsamplingSine()
{
    std::tr1::array<SFLDataFormat, 160> tmpInputBuffer;
    std::tr1::array<SFLDataFormat, 320> tmpOutputBuffer;

    // generate input samples and store them in inputBuffer
    generateSineSignal();

    std::cout << "Test Upsampling Sine" << std::endl;
    converter = new SamplerateConverter(44100);

    CPPUNIT_ASSERT(converter != NULL);

    std::copy(inputBuffer.begin(), inputBuffer.begin() + 160, tmpInputBuffer.begin());
    converter->resample(tmpInputBuffer.data(), tmpOutputBuffer.data(), tmpOutputBuffer.size(), 8000, 16000, tmpInputBuffer.size());

    delete converter;
}

void ResamplerTest::testDownsamplingSine()
{
    std::tr1::array<SFLDataFormat, 320> tmpInputBuffer;
    std::tr1::array<SFLDataFormat, 160> tmpOutputBuffer;

    // generate input samples and store them in inputBuffer
    generateSineSignal();

    std::cout << "Test Downsampling Sine" << std::endl;
    converter = new SamplerateConverter(44100);

    CPPUNIT_ASSERT(converter != NULL);

    std::copy(inputBuffer.begin(), inputBuffer.begin() + 320, tmpInputBuffer.begin());
    converter->resample(tmpInputBuffer.data(), tmpOutputBuffer.data(), tmpOutputBuffer.size(), 8000, 16000, tmpInputBuffer.size());

    delete converter;
}

void ResamplerTest::generateRamp()
{
    for(int i = 0; i < inputBuffer.size(); i++) {
        inputBuffer[i] = (SFLDataFormat)i;
    }
}

void ResamplerTest::generateTriangularSignal()
{
    for(int i = 0; i < inputBuffer.size(); i++) {
        inputBuffer[i] = (SFLDataFormat)(i*10);
    }
}

void ResamplerTest::generateSineSignal()
{
    for(int i = 0; i < inputBuffer.size(); i++) {
        //inputBuffer[i] = (SFLDataFormat)(
    }
}
