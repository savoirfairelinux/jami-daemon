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

#include "echocanceltest.h"
#include "config/sfl_config.h"

using namespace std;

EchoCancelTest::EchoCancelTest() : echoCanceller_() {}

void EchoCancelTest::testEchoCancelProcessing()
{
    const int nbSamples = 160;
    int inputFileLength = 0;
    int remainingLength = 0;

    SFLDataFormat micData[1000];
    SFLDataFormat spkrData[1000];
    SFLDataFormat echoCancelData[1000];

    // near end input with echo
    ifstream micFile("sample_no_echo_8kHz_16bit.raw", ifstream::in);
    // far end input to train filter
    ifstream spkrFile("sample_ecno_500ms_8kHz_16bit.raw", ifstream::in);
    // echo cancelled output
    ofstream echoCancelFile("sample_echocancel_500ms_8kHz_16bit.raw", ofstream::out);

    micFile.seekg(0, ios::end);
    inputFileLength = micFile.tellg() / sizeof(SFLDataFormat);
    micFile.seekg(0, ios::beg);

    remainingLength = inputFileLength;

    while (remainingLength >= nbSamples) {
        micFile.read(reinterpret_cast<char *>(micData), nbSamples * sizeof(SFLDataFormat));
        spkrFile.read(reinterpret_cast<char *>(spkrData), nbSamples * sizeof(SFLDataFormat));

        echoCanceller_.putData(spkrData, nbSamples);
        echoCanceller_.process(micData, echoCancelData, nbSamples);

        echoCancelFile.write(reinterpret_cast<char *>(echoCancelData), nbSamples * sizeof(SFLDataFormat));

        remainingLength -= nbSamples;
    }

    CPPUNIT_ASSERT(true);
}

