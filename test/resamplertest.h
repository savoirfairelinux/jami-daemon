/*
 *  Copyright (C) 2012-2015 Savoir-faire Linux Inc.
 *
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
 */

#ifndef _RESAMPLER_TEST_
#define _RESAMPLER_TEST_

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

#include "audio/resampler.h"
#include "noncopyable.h"

#define MAX_BUFFER_LENGTH 40000
#define TMP_LOWSMPLR_BUFFER_LENGTH 160
#define TMP_HIGHSMPLR_BUFFER_LENGTH 320

namespace ring { namespace test {

class ResamplerTest : public CppUnit::TestCase {

    /**
     * Use cppunit library macros to add unit test the factory
     */
    CPPUNIT_TEST_SUITE(ResamplerTest);
    CPPUNIT_TEST(testUpsamplingRamp);
    CPPUNIT_TEST(testDownsamplingRamp);
    CPPUNIT_TEST(testUpsamplingTriangle);
    CPPUNIT_TEST(testDownsamplingTriangle);
    CPPUNIT_TEST(testUpsamplingSine);
    CPPUNIT_TEST(testDownsamplingSine);
    CPPUNIT_TEST_SUITE_END();

  public:
    ResamplerTest();

    /*
     * Code factoring - Common resources can be initialized here.
     * This method is called by unitcpp before each test
     */
    void setUp();

    /*
     * Code factoring - Common resources can be released here.
     * This method is called by unitcpp after each test
     */
    void tearDown();

    /*
     * Generate a ramp and upsamples it form 8kHz to 16kHz
     */
    void testUpsamplingRamp();

    /*
     * Generate a ramp and downsamples it from 16kHz to 8kHz
     */
    void testDownsamplingRamp();

    /*
     * Generate a triangular signal and upsamples it from 8kHz to 16kHz
     */
    void testUpsamplingTriangle();

    /*
     * Generate a triangular signal and downsamples it from 16kHz to 8kHz
     */
    void testDownsamplingTriangle();

    /*
     * Generate a sine signal and upsamples it from 8kHz to 16kHz
     */
    void testUpsamplingSine();

    /*
     * Generate a sine signal and downsamples it from 16kHz to 8kHz
     */
    void testDownsamplingSine();

private:
    NON_COPYABLE(ResamplerTest);

    /*
     * Generate a ramp to be stored in inputBuffer
     */
    void generateRamp();

    /*
     * Generate a triangular signal to be stored in inputBuffer
     */
    void generateTriangularSignal();

    /*
     * Generate a sine signal to be stored in inputBuffer
     */
    void generateSineSignal();

    /*
     * Perform upsampling on the whole input buffer
     */
    void performUpsampling(ring::Resampler &resampler);

    /*
     * Perform downsampling on the whold input buffer
     */
    void performDownsampling(ring::Resampler &resampler);

    /**
     * Used to store input samples
     */
    ring::AudioBuffer inputBuffer;

    /**
     * Used to receive output samples
     */
    ring::AudioBuffer outputBuffer;
};

/* Register the test module */
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ResamplerTest, "ResamplerTest");
CPPUNIT_TEST_SUITE_REGISTRATION(ResamplerTest);

}} // namespace ring::test

#endif // _RESAMPLER_TEST_
