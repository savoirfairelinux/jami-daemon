/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include "audiorecord.h"
#include "mainbuffer.h"
#include "logger.h"

#include <chrono>
#include <sstream>
#include <unistd.h>

int AudioRecorder::count_ = 0;

AudioRecorder::AudioRecorder(AudioRecord  *arec, MainBuffer &mb) :
    recorderId_(), mbuffer_(mb), arecord_(arec), running_(false), thread_()
{
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

    if (thread_.joinable())
        thread_.join();
}

void AudioRecorder::init() {
    if (!arecord_->isRecording()) {
        arecord_->setSndFormat(mbuffer_.getInternalAudioFormat());
    }
}

void AudioRecorder::start()
{
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&AudioRecorder::run, this);
}

/**
 * Reimplementation of run()
 */
void AudioRecorder::run()
{
    static const size_t BUFFER_LENGTH = 10000;
    static const std::chrono::milliseconds SLEEP_TIME(20); // 20 ms

    AudioBuffer buffer(BUFFER_LENGTH, mbuffer_.getInternalAudioFormat());

    while (running_) {
        const size_t availableSamples = mbuffer_.availableForGet(recorderId_);
        buffer.resize(std::min(availableSamples, BUFFER_LENGTH));
        mbuffer_.getData(buffer, recorderId_);

        if (availableSamples > 0)
            arecord_->recData(buffer);

        std::this_thread::sleep_for(SLEEP_TIME);
    }
}
