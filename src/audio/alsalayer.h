/*
 *  Copyright (C) 2007 - 2008 Savoir-Faire Linux inc.
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

#ifndef _ALSA_LAYER_H
#define _ALSA_LAYER_H

#include "audiolayer.h"

class RingBuffer;
class ManagerImpl;

/** Associate a sound card index to its string description */
typedef std::pair<int , std::string> HwIDPair;

/**
 * @file  AlsaLayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware. 
 */

class AlsaLayer : public AudioLayer {
  public:
    /**
     * Constructor
     * @param manager An instance of managerimpl
     */
    AlsaLayer( ManagerImpl* manager );  

    /**
     * Destructor
     */
    ~AlsaLayer(void);

    void closeLayer( void );

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
     * Get the current audio plugin.
     * @return std::string  The name of the audio plugin
     */
    std::string getAudioPlugin( void ) { return _audioPlugin; }

    /**
     * UNUSED in ALSA layer
     */
    int putInCache( char code, void *buffer, int toCopy );


    /**
     * UNUSED in ALSA layer
     */
    void reducePulseAppsVolume( void );

    /**
     * UNUSED in ALSA layer
     */
    void restorePulseAppsVolume( void ); 

    /**
     * UNUSED in ALSA layer
     */
    void setPlaybackVolume( double volume );

  private:
  
    // Copy Constructor
    AlsaLayer(const AlsaLayer& rh);

    // Assignment Operator
    AlsaLayer& operator=( const AlsaLayer& rh);

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
     * Volume is controlled by the application. Data buffer are modified here to adjust to the right volume selected by the user on the main interface
     * @param buffer  The buffer to adjust
     * @param len The number of bytes
     * @param stream  The stream mode ( PLAYBACK - CAPTURE )
     */
    void * adjustVolume( void * , int , int);

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


    /** Vector to manage all soundcard index - description association of the system */
    std::vector<HwIDPair> IDSoundCards;

};

#endif // _ALSA_LAYER_H_
