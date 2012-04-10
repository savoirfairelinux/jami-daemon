/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "audiorecorder.h"
#include "mainbuffer.h"
#include <cassert>

int AudioRecorder::count_ = 0;

AudioRecorder::AudioRecorder(AudioRecord  *arec, MainBuffer *mb) : ost::Thread(),
    recorderId_(), mbuffer_(mb), arecord_(arec)
{
    assert(mb);

    ++count_;

    std::string id("processid_");

    // convert count into string
    std::string s;
    std::stringstream out;
    out << count_;
    s = out.str();

    recorderId_ = id.append(s);
}

/**
 * Reimplementation of run()
 */
void AudioRecorder::run()
{
    int bufferLength = 10000;
    SFLDataFormat buffer[bufferLength];

    while (isRunning()) {
        int availBytes = mbuffer_->availForGet(recorderId_);
        int toGet = (availBytes < bufferLength) ? availBytes : bufferLength;

        mbuffer_->getData(buffer, toGet, recorderId_);

        if (availBytes > 0)
            arecord_->recData(buffer, availBytes / sizeof(SFLDataFormat));

        sleep(20);
    }
}
