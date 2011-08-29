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
        AudioLayer (int type);

        /**
         * Destructor
         */
        virtual ~AudioLayer (void);

        /**
         * Check if no devices are opened, otherwise close them.
         * Then open the specified devices by calling the private functions open_device
         * @param indexIn	The number of the card chosen for capture
         * @param indexOut	The number of the card chosen for playback
         * @param sampleRate  The sample rate
         * @param stream	  To indicate which kind of stream you want to open
         *			  SFL_PCM_CAPTURE
         *			  SFL_PCM_PLAYBACK
         *			  SFL_PCM_BOTH
         * @param plugin	  The alsa plugin ( dmix , default , front , surround , ...)
         */
        virtual void openDevice (int indexIn, int indexOut, int indexRing, int sampleRate, int stream , const std::string &plugin) = 0;

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
         * Get the index of the audio card for capture
         * @return int The index of the card used for capture
         *			0 for the first available card on the system, 1 ...
         */
        int getIndexIn() const {
            return indexIn_;
        }

        /**
         * Get the index of the audio card for playback
         * @return int The index of the card used for playback
         *			0 for the first available card on the system, 1 ...
         */
        int getIndexOut() const {
            return indexOut_;
        }

        /**
             * Get the index of the audio card for ringtone (could be differnet from playback)
             * @return int The index of the card used for ringtone
             *			0 for the first available card on the system, 1 ...
             */
        int getIndexRing() const {
            return indexRing_;
        }

        /**
         * Get the sample rate of the audio layer
         * @return unsigned int The sample rate
         *			    default: 44100 HZ
         */
        unsigned int getSampleRate() const {
            return audioSampleRate_;
        }

        /**
             * Get the layer type for this instance (either Alsa or PulseAudio)
             * @return unsigned int The layer type
             *
             */
        int getLayerType (void) const {
            return layerType_;
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
         * Set the audio recorder
         */
        void setRecorderInstance (Recordable* rec) {
            recorder_ = rec;
        }

        /**
         * Get the audio recorder
         */
        Recordable* getRecorderInstance (void) const {
            return recorder_;
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
         * Drop the pending frames and close the capture device
         */
        virtual void closeCaptureStream (void) = 0;

        /**
         * Drop the pending frames and close the playback device
         */
        virtual void closePlaybackStream (void) = 0;
 
        /**
	 * Wether or not the audio layer stream is started
         */
        bool isStarted_;

        /**
         * Urgent ring buffer used for ringtones
         */
        RingBuffer urgentRingBuffer_;

        /**
         * A pointer to the recordable instance (may be a call or a conference)
         */
        Recordable* recorder_;

        /**
         * Number of audio cards on which capture stream has been opened
         */
        int indexIn_;

        /**
         * Number of audio cards on which playback stream has been opened
         */
        int indexOut_;

        /**
         * Number of audio cards on which ringtone stream has been opened
         */
        int indexRing_;

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

    private:

        const int layerType_;


        /**
         * Time of the last incoming call notification
         */
        time_t lastNotificationTime_;
};

#endif // _AUDIO_LAYER_H_
