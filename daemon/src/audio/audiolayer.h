/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author:  Jerome Oufella <jerome.oufella@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Authro: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef _AUDIO_LAYER_H
#define _AUDIO_LAYER_H

#include <cc++/thread.h> // for ost::Mutex
#include <sys/time.h>

#include "manager.h"
#include "ringbuffer.h"
#include "dcblocker.h"
#include "samplerateconverter.h"

/**
 * @file  audiolayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware.
 */

class MainBuffer;

namespace ost {
    class Time;
}

class AudioLayer
{
    private:
        //copy constructor
        AudioLayer (const AudioLayer& rh);

        // assignment operator
        AudioLayer& operator= (const AudioLayer& rh);

    public:
        /**
         * Constructor
         */
        AudioLayer ();

        /**
         * Destructor
         */
        virtual ~AudioLayer (void);

        /**
         * Start the capture stream and prepare the playback stream.
         * The playback starts accordingly to its threshold
         * ALSA Library API
         */
        virtual void startStream (void) = 0;

        /**
         * Stop the playback and capture streams.
         * Drops the pending frames and put the capture and playback handles to PREPARED state
         * ALSA Library API
         */
        virtual void stopStream (void) = 0;

        bool isStarted(void) const { return isStarted_; }

        /**
         * Send a chunk of data to the hardware buffer to start the playback
         * Copy data in the urgent buffer.
         * @param buffer The buffer containing the data to be played ( ringtones )
         * @param toCopy The size of the buffer
         */
        void putUrgent (void* buffer, int toCopy);

        void flushMain (void);

        void flushUrgent (void);


        /**
         * Get the sample rate of the audio layer
         * @return unsigned int The sample rate
         *			    default: 44100 HZ
         */
        unsigned int getSampleRate() const {
            return audioSampleRate_;
        }

        /**
             * Get a pointer to the application MainBuffer class.
         *
         * In order to send signal to other parts of the application, one must pass through the mainbuffer.
         * Audio instances must be registered into the MainBuffer and bound together via the ManagerImpl.
         *
             * @return MainBuffer* a pointer to the MainBuffer instance
             */
        MainBuffer* getMainBuffer (void) const {
            return Manager::instance().getMainBuffer();
        }

        /**
         * Get the mutex lock for the entire audio layer
         */
        ost::Mutex* getMutexLock (void) {
            return &mutex_;
        }

        void notifyincomingCall (void);

    protected:
 
        /**
         * Wether or not the audio layer stream is started
         */
        bool isStarted_;

        /**
         * Urgent ring buffer used for ringtones
         */
        RingBuffer urgentRingBuffer_;

        /**
         * Sample Rate SFLphone should send sound data to the sound card
         * The value can be set in the user config file- now: 44100HZ
         */
        unsigned int audioSampleRate_;

        /**
         * Lock for the entire audio layer
         */
        ost::Mutex mutex_;

        DcBlocker dcblocker_;

        AudioPreference &audioPref;

        SamplerateConverter *converter_;

    private:

        /**
         * Time of the last incoming call notification
         */
        time_t lastNotificationTime_;
};

#endif // _AUDIO_LAYER_H_
