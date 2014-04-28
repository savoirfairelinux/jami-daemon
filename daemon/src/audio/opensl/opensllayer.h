/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#ifndef _OPENSL_LAYER_H
#define _OPENSL_LAYER_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <vector>

#include "audio/audiolayer.h"

class AudioPreference;

#include "noncopyable.h"

class OpenSLThread;

#define ANDROID_BUFFER_QUEUE_LENGTH 2U
#define BUFFER_SIZE 512U

#define MAX_NUMBER_INTERFACES 5
#define MAX_NUMBER_INPUT_DEVICES 3

/**
 * @file  OpenSLLayer.h
 * @brief Main sound class for android. Manages the data transfers between the application and the hardware.
 */

class OpenSLLayer : public AudioLayer {
    public:
        /**
         * Constructor
         */
        OpenSLLayer(const AudioPreference &pref);

        /**
         * Destructor
         */
        ~OpenSLLayer();

        /**
         * Start the capture stream and prepare the playback stream.
         * The playback starts accordingly to its threshold
         */
        virtual void startStream();

        /**
         * Stop the playback and capture streams.
         * Drops the pending frames and put the capture and playback handles to PREPARED state
         */
        virtual void stopStream();

        /**
         * Scan the sound card available for capture on the system
         * @return std::vector<std::string> The vector containing the string description of the card
         */
        virtual std::vector<std::string> getCaptureDeviceList() const;

        /**
         * Scan the sound card available for capture on the system
         * @return std::vector<std::string> The vector containing the string description of the card
         */
        virtual std::vector<std::string> getPlaybackDeviceList() const;

        void init();

        void initAudioEngine();

        void shutdownAudioEngine();

        void initAudioPlayback();

        void initAudioCapture();

        void startAudioPlayback();

        void startAudioCapture();

        void stopAudioPlayback();

        void stopAudioCapture();

        virtual int getAudioDeviceIndex(const std::string&, DeviceType) const {
            return 0;
        }

        virtual std::string getAudioDeviceName(int, DeviceType) const {
            return "";
        }

    private:
        typedef std::vector<AudioBuffer> AudioBufferStack;


        bool audioBufferFillWithZeros(AudioBuffer &buffer);

        /**
         * Here fill the input buffer with tone or ringtone samples
         */
        bool audioPlaybackFillWithToneOrRingtone(AudioBuffer &buffer);

        bool audioPlaybackFillWithUrgent(AudioBuffer &buffer, size_t bytesAvail);

        size_t audioPlaybackFillWithVoice(AudioBuffer &buffer);

        void audioCaptureFillBuffer(AudioBuffer &buffer);


        /**
         * This is the main audio playabck callback called by the OpenSL layer
         */
        static void audioPlaybackCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

        /**
         * This is the main audio capture callback called by the OpenSL layer
         */
        static void audioCaptureCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

        /**
         * Get the index of the audio card for capture
         * @return int The index of the card used for capture
         *                     0 for the first available card on the system, 1 ...
         */
        virtual int getIndexCapture() const {
            return indexIn_;
        }

        /**
         * Get the index of the audio card for playback
         * @return int The index of the card used for playback
         *                     0 for the first available card on the system, 1 ...
         */
        virtual int getIndexPlayback() const {
            return indexOut_;
        }

        /**
         * Get the index of the audio card for ringtone (could be differnet from playback)
         * @return int The index of the card used for ringtone
         *                 0 for the first available card on the system, 1 ...
         */
        virtual int getIndexRingtone() const {
            return indexRing_;
        }

        AudioBuffer &getNextPlaybackBuffer(void) {
            return playbackBufferStack_[playbackBufferIndex_];
        }

        AudioBuffer &getNextRecordBuffer(void) {
            return recordBufferStack_[recordBufferIndex_];
        }

        void incrementPlaybackIndex(void) {
            playbackBufferIndex_ = (playbackBufferIndex_ + 1) % NB_BUFFER_PLAYBACK_QUEUE;
        }

        void incrementRecordIndex(void) {
            recordBufferIndex_ = (recordBufferIndex_ + 1) % NB_BUFFER_CAPTURE_QUEUE;
        }

        void CheckErr( SLresult res );

        void playback(SLAndroidSimpleBufferQueueItf queue);
        void capture(SLAndroidSimpleBufferQueueItf queue);

        void dumpAvailableEngineInterfaces();
        friend class OpenSLThread;

        static const int NB_BUFFER_PLAYBACK_QUEUE;

        static const int NB_BUFFER_CAPTURE_QUEUE;

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

        NON_COPYABLE(OpenSLLayer);

        virtual void updatePreference(AudioPreference &pref, int index, DeviceType type);

        OpenSLThread *audioThread_;

        /**
         * OpenSL standard object interface
         */
        SLObjectItf engineObject_;

        /**
         * OpenSL sound engine interface
         */
        SLEngineItf engineInterface_;

        /**
         * Output mix interface
         */
        SLObjectItf outputMixer_;
        SLObjectItf playerObject_;
        SLObjectItf recorderObject_;


        SLOutputMixItf outputMixInterface_;
        SLPlayItf playerInterface_;

        SLRecordItf recorderInterface_;

        SLAudioIODeviceCapabilitiesItf AudioIODeviceCapabilitiesItf;
        SLAudioInputDescriptor AudioInputDescriptor;

        /**
         * OpenSL playback buffer
         */
        SLAndroidSimpleBufferQueueItf playbackBufferQueue_;
        SLAndroidSimpleBufferQueueItf recorderBufferQueue_;

        int playbackBufferIndex_;
        int recordBufferIndex_;

        bool bufferIsFilled_;
        AudioFormat hardwareFormat_;
        size_t hardwareBuffSize_;

        AudioBufferStack playbackBufferStack_;
        AudioBufferStack recordBufferStack_;

};

#endif // _OPENSL_LAYER_H_
