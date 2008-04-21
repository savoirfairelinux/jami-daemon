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
#include "audiodevice.h"
#include "ringbuffer.h"

#include <cc++/thread.h> // for ost::Mutex
#include <vector>
#include <alsa/asoundlib.h>
#include <iostream>
#include <istream>
#include <sstream>
#define FRAME_PER_BUFFER	160

class RingBuffer;
class ManagerImpl;

/** Associate a sound card index to its string description */
typedef std::pair<int , std::string> HwIDPair;

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
    AudioLayer(ManagerImpl* manager);
    
    /**
     * Destructor
     */
    ~AudioLayer(void);

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
    bool openDevice(int indexIn, int indexOut, int sampleRate, int frameSize, int stream, std::string plugin);

    /**
     * Start the capture stream and prepare the playback stream. 
     * The playback starts accordingly to its threshold
     * ALSA Library API
     */
    void startStream(void);

    /**
     * Stop the playback and capture streams. 
     * Drops the pending frames and put the capture and playback handles to PREPARED state
     * ALSA Library API
     */
    void stopStream(void);
    
    /**
     * Check if the playback is running
     * @return true if the state of the playback handle equals SND_PCM_STATE_RUNNING
     *	       false otherwise
     */
    bool isPlaybackActive( void );

    /**
     * Check if the capture is running
     * @return true if the state of the capture handle equals SND_PCM_STATE_RUNNING
     *	       false otherwise
     */
    bool isCaptureActive( void );

    /**
     * Check if both capture and playback are running
     * @return true if capture and playback are running
     *	       false otherwise
     */
    bool isStreamActive(void);

    /**
     * Check if both capture and playback are stopped
     * @return true if capture and playback are stopped
     *	       false otherwise
     */
    bool isStreamStopped(void);

    /**
     * Send samples to the audio device. 
     * @param buffer The buffer containing the data to be played ( voice and DTMF )
     * @param toCopy The number of samples, in bytes
     * @param isTalking	If whether or not the conversation is running
     * @return int The number of bytes played
     */
    int playSamples(void* buffer, int toCopy, bool isTalking);

    /**
     * Send a chunk of data to the hardware buffer to start the playback
     * Copy data in the urgent buffer. 
     * @param buffer The buffer containing the data to be played ( ringtones )
     * @param toCopy The size of the buffer
     * @return int  The number of bytes copied in the urgent buffer
     */
    int putUrgent(void* buffer, int toCopy);

    /**
     * Query the capture device for number of bytes available in the hardware ring buffer
     * @return int The number of bytes available
     */
    int canGetMic();

    /**
     * Get data from the capture device
     * @param buffer The buffer for data
     * @param toCopy The number of bytes to get
     * @return int The number of bytes acquired ( 0 if an error occured)
     */
    int getMic(void * buffer, int toCopy);
    
    /**
     * Concatenate two strings. Used to build a valid pcm device name. 
     * @param plugin the alsa PCM name
     * @param card the sound card number
     * @param subdevice the subdevice number
     * @return std::string the concatenated string
     */
    std::string buildDeviceTopo( std::string plugin, int card, int subdevice );

    /**
     * Scan the sound card available on the system
     * @param stream To indicate whether we are looking for capture devices or playback devices
     *		   SFL_PCM_CAPTURE
     *		   SFL_PCM_PLAYBACK
     *		   SFL_PCM_BOTH
     * @return std::vector<std::string> The vector containing the string description of the card
     */
    std::vector<std::string> getSoundCardsInfo( int stream );

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
    bool soundCardIndexExist( int card , int stream );
    
    /**
     * An index is associated with its string description
     * @param description The string description
     * @return	int	  Its index
     */
    int soundCardGetIndex( std::string description );

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
     * Get the current audio plugin.
     * @return std::string  The name of the audio plugin
     */
    std::string getAudioPlugin( void ) { return _audioPlugin; }
    /**
     * Get the current state. Conversation or not
     * @return bool true if playSamples has been called  
     *		    false otherwise
     */
    bool getCurrentState( void ) { return _talk; }

    /**
     * Toggle echo testing on/off
     */
    void toggleEchoTesting();

  private:

    /**
     * Drop the pending frames and close the capture device
     * ALSA Library API
     */
    void closeCaptureStream( void );

    /**
     * Drop the pending frames and close the playback device
     * ALSA Library API
     */
    void closePlaybackStream( void );

    /**
     * Fill the alsa internal ring buffer with chunks of data
     */
    void fillHWBuffer( void) ;

    /**
     * Callback used for asynchronous playback.
     * Called when a certain amount of data is written ot the device
     * @param pcm_callback  The callback pointer
     */
    static void AlsaCallBack( snd_async_handler_t* pcm_callback);

    /**
     * Callback used for asynchronous playback.
     * Write tones buffer to the alsa internal ring buffer.
     */
    void playTones( void );

    /**
     * Open the specified device.
     * ALSA Library API
     * @param pcm_p The string name for the playback device
     * @param pcm_c The string name for the capture device
     * @param flag  To indicate which kind of stream you want to open
     *		    SFL_PCM_CAPTURE
     *		    SFL_PCM_PLAYBACK
     *		    SFL_PCM_BOTH
     * @return true if successful
     *	       false otherwise
     */
    bool open_device( std::string pcm_p, std::string pcm_c, int flag); 

    /**
     * Copy a data buffer in the internal ring buffer
     * ALSA Library API
     * @param buffer The data to be copied
     * @param length The size of the buffer
     * @return int The number of frames actually copied
     */
    int write( void* buffer, int length);
    
    /**
     * Read data from the internal ring buffer
     * ALSA Library API
     * @param buffer  The buffer to stock the read data
     * @param toCopy  The number of bytes to get
     * @return int The number of frames actually read
     */
    int read( void* buffer, int toCopy);
    
    /**
     * Recover from XRUN state for capture
     * ALSA Library API
     */
    void handle_xrun_capture( void );

    /**
     * Recover from XRUN state for playback
     * ALSA Library API
     */
    void handle_xrun_playback( void );
    
    /** Augment coupling, reduce indirect access */
    ManagerImpl* _manager; 

    /**
     * Handles to manipulate playback stream
     * ALSA Library API
     */
    snd_pcm_t* _PlaybackHandle;

    /**
     * Handles to manipulate capture stream
     * ALSA Library API
     */
    snd_pcm_t* _CaptureHandle;
    
    /**
     * Alsa parameter - Size of a period in the hardware ring buffer
     */
    snd_pcm_uframes_t _periodSize;

    /**
     * Handle on asynchronous event
     */
    snd_async_handler_t *_AsyncHandler;
    
    /**
     * Urgent ring buffer used for ringtones
     */
    RingBuffer _urgentBuffer;
    
    /**
     * Volume is controlled by the application. Data buffer are modified here to adjust to the right volume selected by the user on the main interface
     * @param buffer  The buffer to adjust
     * @param len The number of bytes
     * @param stream  The stream mode ( PLAYBACK - CAPTURE )
     */
    void * adjustVolume( void * , int , int);

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
     * name of the alsa audio plugin used
     */
    std::string _audioPlugin;

    /**
     * Input channel (mic) should be 1 mono
     */
    unsigned int _inChannel; 

    /**
     * Output channel (stereo) should be 1 mono
     */
    unsigned int _outChannel; 

    /**
     * Default volume for incoming RTP and Urgent sounds.
     */
    unsigned short _defaultVolume; // 100

    /**
     * Echo testing or not
     */
    bool _echoTesting;

    /** Vector to manage all soundcard index - description association of the system */
    std::vector<HwIDPair> IDSoundCards;

    /** Contains the current error code */
    int _errorMessage;

    ost::Mutex _mutex;
};

#endif // _AUDIO_LAYER_H_
