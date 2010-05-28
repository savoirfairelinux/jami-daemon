/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include "global.h"
#include "audiodevice.h"
#include "ringbuffer.h"
#include "mainbuffer.h"

#include <cc++/thread.h> // for ost::Mutex

#include "dcblocker.h"
#include "speexechocancel.h"
#include "echocancel.h"


/**
 * @file  audiolayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware. 
 */

class Recordable;
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
	      , _urgentRingBuffer( SIZEBUF, default_id )
              , _indexIn ( 0 )
              , _indexOut ( 0 )
              , _audioSampleRate ( 0 )
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
         * @param indexIn	The number of the card chosen for capture
         * @param indexOut	The number of the card chosen for playback
         * @param sampleRate  The sample rate 
         * @param frameSize	  The frame size
         * @param stream	  To indicate which kind of stream you want to open
         *			  SFL_PCM_CAPTURE
         *			  SFL_PCM_PLAYBACK
         *			  SFL_PCM_BOTH
         * @param plugin	  The alsa plugin ( dmix , default , front , surround , ...)
         */
        virtual bool openDevice(int indexIn, int indexOut, int indexRing, int sampleRate, int frameSize, int stream , std::string plugin) = 0;

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
        int putMain(void* buffer, int toCopy, CallID call_id = default_id);

        void flushMain (void);

        void flushUrgent (void);


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
         * Get the index of the audio card for ringtone (could be differnet from playback)
         * @return int The index of the card used for ringtone
         *			0 for the first available card on the system, 1 ...
         */
	int getIndexRing() {return _indexRing; }

        /**
         * Get the sample rate of the audio layer
         * @return unsigned int The sample rate
         *			    default: 44100 HZ
         */
        unsigned int getSampleRate() { return _audioSampleRate; }

        /**
         * Get the frame size of the audio layer
         * @return unsigned int The frame size
         *			    default: 20 ms
         */
        unsigned int getFrameSize() { return _frameSize; }

	/**
         * Get the layer type for this instance (either Alsa or PulseAudio)
         * @return unsigned int The layer type
         *
         */
        int getLayerType( void ) { return _layerType; }

	/**
         * Get a pointer to the application MainBuffer class.
	 *
	 * In order to send signal to other parts of the application, one must pass through the mainbuffer.
	 * Audio instances must be registered into the MainBuffer and bound together via the ManagerImpl.
	 *
         * @return MainBuffer* a pointer to the MainBuffer instance
         */
	MainBuffer* getMainBuffer( void ) { return _mainBuffer; }

	/**
	 * Set the mainbuffer once the audiolayer is created
	 */
	void setMainBuffer( MainBuffer* mainbuffer ) { _mainBuffer = mainbuffer; }

        /**
         * Default volume for incoming RTP and Urgent sounds.
         */
        unsigned short _defaultVolume; // 100

	
	/**
	 * Set the audio recorder
	 */
	inline void setRecorderInstance (Recordable* rec) {_recorder = NULL; _recorder = rec;}

	/**
	 * Get the audio recorder
	 */
	inline Recordable* getRecorderInstance (void) {return _recorder;}

	/**
	 * Get the echo canceller state
	 * @return true if echo cancel activated
         */
	virtual bool getEchoCancelState(void) = 0;

	/**
	 * Set the echo canceller state
	 * @param state true if echocancel active, false elsewhere 
	 */
	virtual void setEchoCancelState(bool state) = 0;

	/**
	 * Get the noise suppressor state
	 * @return true if noise suppressor activated
	 */
	virtual bool getNoiseSuppressState(void) = 0;

	/**
	 * Set the noise suppressor state
	 * @param state true if noise suppressor active, false elsewhere
	 */
	virtual void setNoiseSuppressState(bool state) = 0;

	/**
	 * Get the mutex lock for the entire audio layer 
	 */
	inline ost::Mutex* getMutexLock(void) { return &_mutex; }


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
        RingBuffer _urgentRingBuffer;

	/**
	 * Instance of the MainBuffer for the whole application
	 *
	 * In order to send signal to other parts of the application, one must pass through the mainbuffer.
	 * Audio instances must be registered into the MainBuffer and bound together via the ManagerImpl.
	 *
	 */ 
	MainBuffer* _mainBuffer;

	/**
	 * A pointer to the recordable instance (may be a call or a conference)
	 */
	Recordable* _recorder;

        /**
         * Number of audio cards on which capture stream has been opened 
         */
        int _indexIn;

        /**
         * Number of audio cards on which playback stream has been opened 
         */
        int _indexOut;

	/**
	 * Number of audio cards on which ringtone stream has been opened
	 */
	int _indexRing;

        /**
         * Sample Rate SFLphone should send sound data to the sound card 
         * The value can be set in the user config file- now: 44100HZ
         */
        unsigned int _audioSampleRate;

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

	/**
	 * Lock for the entire audio layer
	 */ 
        ost::Mutex _mutex;

	EchoCancel *_echoCancel;
	AudioProcessing *_echoCanceller; 

	DcBlocker *_dcblocker;
	AudioProcessing *_audiofilter;

	bool _echocancelstate;

	bool _noisesuppressstate;

};

#endif // _AUDIO_LAYER_H_
