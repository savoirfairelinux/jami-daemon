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


#include "delaydetectiontest.h"

// Returns the number of elements in a, calculated at compile-time
#define ARRAYSIZE(a) \
      ((sizeof(a) / sizeof(*(a))) / \
         static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
#endif

#include <iostream>
#include <math.h>
#include <string.h>

void DelayDetectionTest::setUp() {}

void DelayDetectionTest::tearDown() {}

void DelayDetectionTest::testCrossCorrelation()
{
    float signal[10] = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0,  7.0, 8.0, 9.0};
    float ref[3] = {0.0, 1.0, 2.0};

    float result[10];
    float expected[10] = {0.0, 0.89442719, 1.0, 0.95618289, 0.91350028, 0.88543774, 0.86640023, 0.85280287, 0.8426548, 0.83480969};

    CPPUNIT_ASSERT(delaydetect_.correlate(ref, ref, 3) == 5.0);
    CPPUNIT_ASSERT(delaydetect_.correlate(signal, signal, 10) == 285.0);

    delaydetect_.crossCorrelate(ref, signal, result, 3, 10);

    float tmp;

    for (int i = 0; i < ARRAYSIZE(result); i++) {
        tmp = result[i] - expected[i];

        if (tmp < 0.0)
            CPPUNIT_ASSERT(tmp > -0.001);
        else
            CPPUNIT_ASSERT(tmp < 0.001);
    }
}

void DelayDetectionTest::testCrossCorrelationDelay()
{
    float signal[10] = {0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 0.0};
    float ref[3] = {0.0, 1.0, 0.0};

    float result[10];

    delaydetect_.crossCorrelate(ref, signal, result, 3, 10);

    float expected[10] = {0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0};

}

void DelayDetectionTest::testFirFilter()
{
    const float decimationCoefs[] = {-0.09870257, 0.07473655, 0.05616626, 0.04448337, 0.03630817, 0.02944626,
        0.02244098, 0.01463477, 0.00610982, -0.00266367, -0.01120109, -0.01873722,
        -0.02373243, -0.02602213, -0.02437806, -0.01869834, -0.00875287, 0.00500204,
        0.02183252, 0.04065763, 0.06015944, 0.0788299, 0.09518543, 0.10799179,
        0.1160644,  0.12889288, 0.1160644, 0.10799179, 0.09518543, 0.0788299,
        0.06015944, 0.04065763, 0.02183252, 0.00500204, -0.00875287, -0.01869834,
        -0.02437806, -0.02602213, -0.02373243, -0.01873722, -0.01120109, -0.00266367,
        0.00610982, 0.01463477, 0.02244098, 0.02944626, 0.03630817, 0.04448337,
        0.05616626,  0.07473655, -0.09870257};
    std::vector<double> ird(decimationCoefs, decimationCoefs + sizeof(decimationCoefs) / sizeof(float));

    const float bandpassCoefs[] = {0.06278034, -0.0758545, -0.02274943, -0.0084497, 0.0702427, 0.05986113,
        0.06436469, -0.02412049, -0.03433526, -0.07568665, -0.03214543, -0.07236507,
        -0.06979052, -0.12446371, -0.05530828, 0.00947243, 0.15294699, 0.17735563,
        0.15294699, 0.00947243, -0.05530828, -0.12446371, -0.06979052, -0.07236507,
        -0.03214543, -0.07568665, -0.03433526, -0.02412049,  0.06436469, 0.05986113,
        0.0702427, -0.0084497, -0.02274943, -0.0758545, 0.06278034};
    std::vector<double> irb(bandpassCoefs, bandpassCoefs + sizeof(bandpassCoefs) / sizeof(float));

    float impulse[100];
    memset(impulse, 0, sizeof(impulse))
    impulse[0] = 1.0;

    FirFilter decimationFilter_(ird);
    FirFilter bandpassFilter_(irb);

    float impulseresponse[100];
    memset(impulseresponse, 0, sizeof impulseresponse);

    // compute impulse response
    for (int i = 0; i < ARRAYSIZE(impulse); i++)
        impulseresponse[i] = decimationFilter_.getOutputSample(impulse[i]);

    for (int i = 0; i < ARRAYSIZE(decimationCoefs); ++i) {
        float tmp = decimationCoefs[i] - impulseresponse[i];

        if (tmp < 0.0)
            CPPUNIT_ASSERT(tmp > -0.000001);
        else
            CPPUNIT_ASSERT(tmp < 0.000001);
    }


    for (size_t i = 0; i < ARRAYSIZE(impulseresponse); ++i)
        impulseresponse[i] = bandpassFilter_.getOutputSample(impulse[i]);

    for (size_t i = 0; i < ARRAYSIZE(bandpassCoefs); ++i) {
        tmp = bandpassCoefs[i] - impulseresponse[i];

        if (tmp < 0.0)
            CPPUNIT_ASSERT(tmp > -0.000001);
        else
            CPPUNIT_ASSERT(tmp < 0.000001);
    }
}

void DelayDetectionTest::testIntToFloatConversion()
{
    SFLDataFormat data[32768 * 2];
    float converted[ARRAYSIZE(data)];

    for (int i = -32768; i < 32768; i++)
        data[i + 32768] = i;

    delaydetect_.convertInt16ToFloat32(data, converted, ARRAYSIZE(data));

    for (int i = -32768; i < 0; i++) {
        CPPUNIT_ASSERT(converted[i + 32768] >= -1.0);
        CPPUNIT_ASSERT(converted[i + 32768] <= 0.0);
    }

    for (int i = 0; i < 32768; i++) {
        CPPUNIT_ASSERT(converted[i + 32768] >= 0.0);
        CPPUNIT_ASSERT(converted[i + 32768] <= 1.0);
    }
}

void DelayDetectionTest::testDownSamplingData()
{
    SFLDataFormat data[32768 * 2];
    float converted[ARRAYSIZE(data)];
    float resampled[ARRAYSIZE(data)];

    for (int i = -32768; i < 32768; i++)
        data[i + 32768] = i;

    delaydetect_.convertInt16ToFloat32(data, converted, 32768 * 2);

    delaydetect_.downsampleData(converted, resampled, 32768 * 2, 8);

    for (size_t i = 0; i < 32768 / 8; ++i) {
        CPPUNIT_ASSERT(resampled[i] >= -1.0);
        CPPUNIT_ASSERT(resampled[i] <= 0.0);
    }

    for (size_t i = 32768 / 8 + 1; i < 32768 / 4; i++) {
        CPPUNIT_ASSERT(resampled[i] >= 0.0);
        CPPUNIT_ASSERT(resampled[i] <= 1.0);
    }


}


void DelayDetectionTest::testDelayDetection()
{
    SFLDataFormat spkr[WINDOW_SIZE];
    memset(spkr, 0, sizeof spkr);
    for (size_t i = 0; i < 5; ++i)
        spkr[i] = 32000;

    SFLDataFormat mic[DELAY_BUFF_SIZE];
    memset(mic, 0, sizeof mic);
    for (size_t delay = 100; delay < 105; ++delay)
        mic[delay] = 32000;

    delaydetect_.putData(spkr, ARRAYSIZE(spkr));
    delaydetect_.process(mic, ARRAYSIZE(mic));
}
