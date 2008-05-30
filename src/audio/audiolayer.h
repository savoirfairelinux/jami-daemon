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

#include "../global.h"
#include "../manager.h"
#include "audiodevice.h"
#include "ringbuffer.h"

#include <cc++/thread.h> // for ost::Mutex
#include <vector>

#include <alsa/asoundlib.h>
#include <pulse/pulseaudio.h>

#include <iostream>
#include <istream>
#include <sstream>

#define FRAME_PER_BUFFER	160


/**
 * @file  audiolayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware. 
 */

class AudioLayer {
  public:
    /**
     * Constructor
     * @param manager An instance of managerimpl
     */
    AudioLayer( ManagerImpl* manager , int type )
      :	  _manager(manager)
	, _urgentBuffer( SIZEBUF )
	, _layerType( type )
    {
      _inChannel  = 1; // don't put in stereo
      _outChannel = 1; // don't put in stereo

      deviceClosed = true;

    }
    
    /**
     * Destructor
     */
    ~AudioLayer(void){}

    virtual void closeLayer( void ) = 0;

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
     * Check if both capture and playback are running
     * @return true if capture and playback are running
     *	       false otherwise
     */
    virtual bool isStreamActive(void) = 0;

    /**
     * Check if the capture is running
     * @return true if the state of the capture handle equals SND_PCM_STATE_RUNNING
     *	       false otherwise
     */
    virtual bool isCaptureActive( void ) = 0;

    /**
     * Send samples to the audio device. 
     * @param buffer The buffer containing the data to be played ( voice and DTMF )
     * @param toCopy The number of samples, in bytes
     * @param isTalking	If whether or not the conversation is running
     * @return int The number of bytes played
     */
    virtual int playSamples(void* buffer, int toCopy, bool isTalking) = 0;

    /**
     * Send a chunk of data to the hardware buffer to start the playback
     * Copy data in the urgent buffer. 
     * @param buffer The buffer containing the data to be played ( ringtones )
     * @param toCopy The size of the buffer
     * @return int  The number of bytes copied in the urgent buffer
     */
    virtual int putUrgent(void* buffer, int toCopy) = 0; 

    virtual int putMain( void* buffer, int toCopy) = 0;
    virtual int putInCache(char code, void* buffer, int toCopy) = 0;

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
     * Scan the sound card available on the system
     * @param stream To indicate whether we are looking for capture devices or playback devices
     *		   SFL_PCM_CAPTURE
     *		   SFL_PCM_PLAYBACK
     *		   SFL_PCM_BOTH
     * @return std::vector<std::string> The vector containing the string description of the card
     */
    virtual std::vector<std::string> getSoundCardsInfo( int stream ) = 0;

    /**
     * Check if the given index corresponds to an existing sound card and supports the specified streaming mode
     * @param card   An index
     * @param stream  The stream mode
     *		  SFL_PCM_CAPTURE
     *		  SFL_PCM_PLAYBACK
     *		  SFL_PCM_BOTH
     * @return bool True if it exists and supports the mode
     *		    false otherwise
     */
    virtual bool soundCardIndexExist( int card , int stream ) = 0;
    
    /**
     * An index is associated with its string description
     * @param description The string description
     * @return	int	  Its index
     */
    virtual int soundCardGetIndex( std::string description ) = 0;

    /**
     * Get the current audio plugin.
     * @return std::string  The name of the audio plugin
     */
    virtual std::string getAudioPlugin( void ) = 0; 

    virtual void reducePulseAppsVolume( void ) = 0;
    virtual void restorePulseAppsVolume( void ) = 0;
  
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

    /**
     * Get the current state. Conversation or not
     * @return bool true if playSamples has been called  
     *		    false otherwise
     */
    bool getCurrentState( void ) { return _talk; }

    int getLayerType( void ) { return _layerType; }

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
    RingBuffer _urgentBuffer;

    /**
     * Determine if both endpoints hang up.
     *	true if conversation is running
     *	false otherwise
     */
    bool _talk;
    
    /**
     * Enable to determine if the devices are opened or not
     *		  true if the devices are closed
     *		  false otherwise
     */
    bool deviceClosed;

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
