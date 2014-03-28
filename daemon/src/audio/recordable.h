/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#ifndef RECORDABLE_H
#define RECORDABLE_H

#include "audiorecord.h"
#include "audiorecorder.h"

class Recordable {

    public:

        Recordable();
        virtual ~Recordable();

        /**
         * Return recording state (true/false)
         */
        bool isRecording() const {
            return recAudio_.isRecording();
        }

        /**
         * This method must be implemented for this interface as calls and conferences
         * have different behavior.
         * Implementations must call the super method.
         */
        virtual bool toggleRecording() {
            if (not isRecording())
                recorder_.init();
            return recAudio_.toggleRecording();
        }

        /**
         * Stop recording
         */
        void stopRecording() {
            recAudio_.stopRecording();
        }

        /**
         * Init the recording file name according to path specified in configuration
         */
        void initRecFilename(const std::string &filename);

        /**
         * Return the file path for this recording
         */
        virtual std::string getFilename() const;

        /**
         * Set recording sampling rate.
         */
        void setRecordingFormat(AudioFormat format);

    protected:
        AudioRecord recAudio_;
        AudioRecorder recorder_;
};

#endif
