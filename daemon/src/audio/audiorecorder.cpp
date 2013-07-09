/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#include "audiorecorder.h"
#include "mainbuffer.h"
#include "logger.h"
#include <sstream>
#include <unistd.h>
#include <cassert>
#include <tr1/array>

int AudioRecorder::count_ = 0;

AudioRecorder::AudioRecorder(AudioRecord  *arec, MainBuffer *mb) :
    recorderId_(), mbuffer_(mb), arecord_(arec), running_(false), thread_(0)
{
    assert(mb);

    ++count_;

    std::string id("processid_");

    // convert count into string
    std::string s;
    std::ostringstream out;
    out << count_;
    s = out.str();

    recorderId_ = id.append(s);
}

AudioRecorder::~AudioRecorder()
{
    running_ = false;

    if (thread_)
        pthread_join(thread_, NULL);
}

void AudioRecorder::start()
{
    running_ = true;
    pthread_create(&thread_, NULL, &runCallback, this);
}

void *
AudioRecorder::runCallback(void *data)
{
    AudioRecorder *context = static_cast<AudioRecorder*>(data);
    context->run();
    return NULL;
}

/**
 * Reimplementation of run()
 */
void AudioRecorder::run()
{
    const size_t BUFFER_LENGTH = 10000;
    std::tr1::array<SFLDataFormat, BUFFER_LENGTH> buffer;
    buffer.assign(0);

    while (running_) {
        const size_t availableBytes = mbuffer_->availableForGet(recorderId_);
        mbuffer_->getData(buffer.data(), std::min(availableBytes, buffer.size()), recorderId_);

        if (availableBytes > 0)
            arecord_->recData(buffer.data(), availableBytes / sizeof(SFLDataFormat));

        usleep(20000); // 20 ms
    }
}
