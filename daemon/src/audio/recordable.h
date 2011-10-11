/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
        bool isRecording() {
            return recAudio.isRecording();
        }

        /**
         * This method must be implemented for this interface as calls and conferences
         * have different behavior.
         */
        virtual bool setRecording() = 0;

        /**
         * Stop recording
         */
        void stopRecording(void) {
            recAudio.stopRecording();
        }

        /**
         * Init the recording file name according to path specified in configuration
         */
        void initRecFileName(std::string filename);

        /**
         * Return the file path for this recording
         */
        std::string getFileName(void);

        /**
         * Set recording sampling rate.
         */
        void setRecordingSmplRate(int smplRate);

        /**
         * Return the recording sampling rate
             */
        int getRecordingSmplRate(void) const;

        /**
         * Virtual method to be implemented in order to the main
         * buffer to retreive the recorded id.
         */
        virtual std::string getRecFileId() const = 0;

        /**
         * An instance of audio recorder
         */
        AudioRecord recAudio;

        AudioRecorder recorder;

};

#endif
