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

#ifndef AUDIORECORDER_H_
#define AUDIORECORDER_H_

#include "noncopyable.h"

#include <thread>
#include <atomic>
#include <string>

class MainBuffer;
class AudioRecord;

class AudioRecorder {

    public:
        AudioRecorder(AudioRecord  *arec, MainBuffer &mb);
        ~AudioRecorder();
        std::string getRecorderID() const {
            return recorderId_;
        }

        /**
         * Set the record to the current audio format.
         * Should be called before start() at least once.
         */
        void init();

        /**
         * Call to start recording.
         */
        void start();

    private:
        NON_COPYABLE(AudioRecorder);
        void run();

        static int count_;
        std::string recorderId_;
        MainBuffer &mbuffer_;
        AudioRecord *arecord_;
        std::atomic<bool> running_;
        std::thread thread_;
};

#endif
