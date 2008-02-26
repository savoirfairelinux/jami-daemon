/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author:  Jerome Oufella <jerome.oufella@savoirfairelinux.com> 
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#include <cc++/thread.h> // for ost::Mutex
#include <boost/tokenizer.hpp>

#include "../global.h"
#include "audiodevice.h"

#include <vector>
#include <alsa/asoundlib.h>
#include <iostream>
#include <fstream>
#include <istream>
#include <sstream>
#define FRAME_PER_BUFFER	160

class ManagerImpl;

class AudioLayer {
  public:
    AudioLayer(ManagerImpl* manager);
    ~AudioLayer(void);

    /*
     * Check if no devices are opened, otherwise close them.
     * Then open the specified devices by calling the private functions open_device
     * @param indexIn	The number of the card choosen for capture
     * @param indexOut	The number of the card choosen for playback
     * @param sampleRate  The sample rate 
     * @param frameSize	  The frame size
     * @param flag	  To indicate which kind of stream you want to open
     *			  SFL_PCM_CAPTURE
     *			  SFL_PCM_PLAYBACK
     *			  SFL_PCM_BOTH
     */
    bool openDevice(int, int, int, int, int);

    /*
     * Start the capture stream. The playback starts according th its threshold
     * ALSA Library API
     */
    void startStream(void);

    /*
     * Stop the capture stream. The playback stops according to its threshold
     * Drops the pending frames and put the capture handle to PREPARED state
     * ALSA Library API
     */
    void stopStream(void);

    void sleep(int);
    
    /*
     * Check if the playback is running
     * @return true if the state of the playback handle equals SND_PCM_STATE_RUNNING
     *	       false otherwise
     */
    bool isPlaybackActive( void );

    /*
     * Check if the capture is running
     * @return true if the state of the capture handle equals SND_PCM_STATE_RUNNING
     *	       false otherwise
     */
    bool isCaptureActive( void );

    /*
     * Check if both capture and playback are running
     * @return true if capture and playback are running
     *	       false otherwise
     */
    bool isStreamActive(void);

    /*
     * Check if both capture and playback are stopped
     * @return true if capture and playback are stopped
     *	       false otherwise
     */
    bool isStreamStopped(void);

    /*
     * Send samples to the audio device
     * @params buffer The buffer containing the data to be played
     * @params toCopy The number of samples, in bytes
     * @return int The number of bytes played
     */
    int playSamples(void* buffer, int toCopy);
    int playRingTone( void* buffer, int toCopy);
    int putUrgent(void* buffer, int toCopy);

    /*
     * Query the audio devices for number of bytes available in the hardware ring buffer
     * @return int The number of bytes available
     */
    int canGetMic();

    /*
     * Get data from the capture device
     * @param buffer The buffer for data
     * @param toCopy The number of bytes to get
     * @return int The number of bytes acquired ( 0 if an error occured)
     */
    int getMic(void * buffer, int toCopy);
    
    /*
     * Concatenate two strings. Used to build a valid pcm device name. 
     * @param plugin the alsa PCM name
     * @param card the sound card number
     * @param subdevice the subdevice number
     * @return std::string the concatenated string
     */
    std::string buildDeviceTopo( std::string plugin, int card, int subdevice );

    /*
     * Scan the sound card available on the system
     * @param flag To indicate whether we are looking for capture devices or playback devices
     *		   SFL_PCM_CAPTURE
     *		   SFL_PCM_PLAYBACK
     *		   SFL_PCM_BOTH
     * @return std::vector<std::string> The vector containing the string description of the card
     */
    std::vector<std::string> getSoundCardsInfo( int flag );


    void setErrorMessage(const std::string& error) { _errorMessage = error; }
    std::string getErrorMessage() { return _errorMessage; }


    /*
     * Get the index of the audio card for capture
     * @return _indexIn The index of the card used for capture
     *			0 for the first available card on the system, 1 ...
     */
    int getIndexIn() { return _indexIn; }

    /*
     * Get the index of the audio card for playback
     * @return _indexOut The index of the card used for playback
     *			0 for the first available card on the system, 1 ...
     */
    int getIndexOut() { return _indexOut; }

    /*
     * Get the sample rate of the audio layer
     * @return unsigned int The sample rate
     *			    default: 44100 HZ
     */
    unsigned int getSampleRate() { return _sampleRate; }

    /*
     * Get the frame size of the audio layer
     * @return unsigned int The frame size
     *			    default: 20 ms
     */
    unsigned int getFrameSize() { return _frameSize; }

    int getDeviceCount();

    /**
     * Toggle echo testing on/off
     */
    void toggleEchoTesting();

  private:

    /*
     * Open the specified device.
     * ALSA Library API
     * @param pcm_p The string name for the playback device
     *	      pcm_c The string name for the capture device
     *	      flag  To indicate which kind of stream you want to open
     *		    SFL_PCM_CAPTURE
     *		    SFL_PCM_PLAYBACK
     *		    SFL_PCM_BOTH
     * @return true if successful
     *	       false otherwise
     */
    bool open_device( std::string pcm_p, std::string pcm_c, int flag); 

    /*
     * Copy a data buffer in the internal ring buffer
     * ALSA Library API
     * @param buffer The data to be copied
     *	      length The size of the buffer
     * @return int The number of frames actually copied
     */
    int write( void* buffer, int length);
    
    /*
     * Read data from the internal ring buffer
     * ALSA Library API
     * @param buffer  The buffer to stock the read data
     *	      toCopy  The number of bytes to get
     * @return int The number of frames actually read
     */
    int read( void* buffer, int toCopy);
    
    /*
     * Recover from XRUN state for capture
     * ALSA Library API
     */
    void handle_xrun_state( void );

    ManagerImpl* _manager; // augment coupling, reduce indirect access
    // a audiolayer can't live without manager

    /*
     * Handle to manipulate capture and playback streams
     * ALSA Library API
     */
    snd_pcm_t* _playback_handle;
    snd_pcm_t* _capture_handle;
    
    /*
     * Enable to determine if the devices are opened or not
     *		  true if the devices are closed
     *		  false otherwise
     */
    bool device_closed;

    /**
     * Number of audio cards on which stream has been opened 
     */
    int _indexIn;
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
    unsigned int _inChannel; // mic

    /**
     * Output channel (stereo) should be 1 mono
     */
    unsigned int _outChannel; // speaker

    /**
     * Default volume for incoming RTP and Urgent sounds.
     */
    unsigned short _defaultVolume; // 100

    /**
     * Echo testing or not
     */
    bool _echoTesting;

    std::string _errorMessage;
    ost::Mutex _mutex;

    float *table_;
    int tableSize_;
    int leftPhase_;
};

#endif // _AUDIO_LAYER_H_

