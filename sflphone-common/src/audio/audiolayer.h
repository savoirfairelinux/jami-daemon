/*
 *  Copyright (C) 2004-2008 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author:  Jerome Oufella <jerome.oufella@savoirfairelinux.com> 
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 */

#ifndef _AUDIO_LAYER_H
#define _AUDIO_LAYER_H

#include "global.h"
#include "audiodevice.h"
#include "ringbuffer.h"


#include <cc++/thread.h> // for ost::Mutex


#define FRAME_PER_BUFFER	160

/**
 * @file  audiolayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware. 
 */

class ManagerImpl;

class AudioLayer {

    private:

        //copy constructor
        AudioLayer(const AudioLayer& rh);

        // assignment operator
        AudioLayer& operator=(const AudioLayer& rh);

    public:
        /**
         * Constructor
         * @param manager An instance of managerimpl
         */
        AudioLayer( ManagerImpl* manager , int type )
            : _defaultVolume(100)
			  , _layerType( type )
              , _manager(manager)
              , _voiceRingBuffer( SIZEBUF )
              , _urgentRingBuffer( SIZEBUF)
              , _micRingBuffer( SIZEBUF )
              , _indexIn ( 0 )
              , _indexOut ( 0 )
              , _sampleRate ( 0 )
              , _frameSize ( 0 )
              , _inChannel( 1 )
              , _outChannel ( 1 )
              , _errorMessage ( 0 )
              , _mutex ()
    {

    }

        /**
         * Destructor
         */
        virtual ~AudioLayer(void) {} 

        virtual bool closeLayer( void ) = 0;

        /**
         * Check if no devices are opened, otherwise close them.
         * Then open the specified devices by calling the private functions open_device
         * @param indexIn	The number of the card choosen for capture
         * @param indexOut	The number of the card choosen for playback
         * @param sampleRate  The sample rate 
         * @param frameSize	  The frame size
         * @param stream	  To indicate which kind of stream you want to open
         *			  SFL_PCM_CAPTURE
         *			  SFL_PCM_PLAYBACK
         *			  SFL_PCM_BOTH
         * @param plugin	  The alsa plugin ( dmix , default , front , surround , ...)
         */
        virtual bool openDevice(int indexIn, int indexOut, int sampleRate, int frameSize, int stream , std::string plugin) = 0;

        /**
         * Start the capture stream and prepare the playback stream. 
         * The playback starts accordingly to its threshold
         * ALSA Library API
         */
        virtual void startStream(void) = 0;

        /**
         * Stop the playback and capture streams. 
         * Drops the pending frames and put the capture and playback handles to PREPARED state
         * ALSA Library API
         */
        virtual void stopStream(void) = 0;

        /**
         * Query the capture device for number of bytes available in the hardware ring buffer
         * @return int The number of bytes available
         */
        virtual int canGetMic() = 0;

        /**
         * Get data from the capture device
         * @param buffer The buffer for data
         * @param toCopy The number of bytes to get
         * @return int The number of bytes acquired ( 0 if an error occured)
         */
        virtual int getMic(void * buffer, int toCopy) = 0;

        /**
         * Send a chunk of data to the hardware buffer to start the playback
         * Copy data in the urgent buffer. 
         * @param buffer The buffer containing the data to be played ( ringtones )
         * @param toCopy The size of the buffer
         * @return int  The number of bytes copied in the urgent buffer
         */
        int putUrgent(void* buffer, int toCopy);

        /**
         * Put voice data in the main sound buffer
         * @param buffer    The buffer containing the voice data ()
         * @param toCopy    The size of the buffer
         * @return int      The number of bytes copied
         */
        int putMain(void* buffer, int toCopy);

        void flushMain (void);

        void flushUrgent (void);

        /**
         * Flush the mic ringbuffer
         */
        void flushMic();

        virtual bool isCaptureActive (void) = 0;

        /**
         * Write accessor to the error state
         * @param error The error code
         *		    Could be: ALSA_PLAYBACK_DEVICE
         *			      ALSA_CAPTURE_DEVICE
         */  
        void setErrorMessage(const int& error) { _errorMessage = error; }

        /**
         * Read accessor to the error state
         * @return int  The error code
         */
        int getErrorMessage() { return _errorMessage; }

        /**
         * Get the index of the audio card for capture
         * @return int The index of the card used for capture
         *			0 for the first available card on the system, 1 ...
         */
        int getIndexIn() { return _indexIn; }

        /**
         * Get the index of the audio card for playback
         * @return int The index of the card used for playback
         *			0 for the first available card on the system, 1 ...
         */
        int getIndexOut() { return _indexOut; }

        /**
         * Get the sample rate of the audio layer
         * @return unsigned int The sample rate
         *			    default: 44100 HZ
         */
        unsigned int getSampleRate() { return _sampleRate; }

        /**
         * Get the frame size of the audio layer
         * @return unsigned int The frame size
         *			    default: 20 ms
         */
        unsigned int getFrameSize() { return _frameSize; }

        int getLayerType( void ) { return _layerType; }

        /**
         * Default volume for incoming RTP and Urgent sounds.
         */
        unsigned short _defaultVolume; // 100

    protected:

        int _layerType;

        /**
         * Drop the pending frames and close the capture device
         */
        virtual void closeCaptureStream( void ) = 0;

        /**
         * Drop the pending frames and close the playback device
         */
        virtual void closePlaybackStream( void ) = 0;

        /** Augment coupling, reduce indirect access */
        ManagerImpl* _manager; 

        /**
         * Urgent ring buffer used for ringtones
         */
        RingBuffer _voiceRingBuffer;
        RingBuffer _urgentRingBuffer;
        RingBuffer _micRingBuffer;

        /**
         * Number of audio cards on which capture stream has been opened 
         */
        int _indexIn;

        /**
         * Number of audio cards on which playback stream has been opened 
         */
        int _indexOut;

        /**
         * Sample Rate SFLphone should send sound data to the sound card 
         * The value can be set in the user config file- now: 44100HZ
         */
        unsigned int _sampleRate;

        /**
         * Length of the sound frame we capture or read in ms
         * The value can be set in the user config file - now: 20ms
         */	 		
        unsigned int _frameSize;

        /**
         * Input channel (mic) should be 1 mono
         */
        unsigned int _inChannel; 

        /**
         * Output channel (stereo) should be 1 mono
         */
        unsigned int _outChannel; 

        /** Contains the current error code */
        int _errorMessage;

        ost::Mutex _mutex;
};

#endif // _AUDIO_LAYER_H_
