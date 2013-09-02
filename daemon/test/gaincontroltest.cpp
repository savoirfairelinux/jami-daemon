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

#include "gaincontroltest.h"
#include "audio/gaincontrol.h"
#include "config/sfl_config.h"

void GainControlTest::testGainProcessing()
{
#if 0
    int fileSize;
    SFLDataFormat buf[SFL_GAIN_BUFFER_LENGTH];
#endif

    // Sampling rate is 8000
    // Target level is 0 dB
    GainControl gcontrol(8000, 0.0);

    /*
    fstream inputFile("testgaininput.raw", fstream::in);
    fstream outputFile("testgainoutput.raw", fstream::out);

    inputFile.seekg(0, ios::end);
    fileSize = inputFile.tellg();
    inputFile.seekg(0, ios::beg);

    while(fileSize > 0) {
        inputFile.read(reinterpret_cast<char *>(inbuf), BUFFER_LENGTH * sizeof(SFLDataFormat));

        gcontrol.process(buf, BUFFER_LENGTH);

        outputFile.write(reinterpret_cast<char *>(outbuf), BUFFER_LENGTH * sizeof(SFLDataFormat));

        fileSize -= (BUFFER_LENGTH * sizeof(SFLDataFormat));
    }

    inputFile.close();
    outputFile.close();
    */

    CPPUNIT_ASSERT(true);
}

