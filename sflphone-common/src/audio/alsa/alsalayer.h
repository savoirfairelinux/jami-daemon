/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#ifndef _ALSA_LAYER_H
#define _ALSA_LAYER_H

#include "audio/audiolayer.h"
#include "audio/samplerateconverter.h"
#include "eventthread.h"
#include <alsa/asoundlib.h>

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

    bool closeLayer( void );

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
    bool openDevice(int indexIn, int indexOut, int indexRing, int sampleRate, int frameSize, int stream, std::string plugin);

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

    void audioCallback (void);

    bool isCaptureActive (void);

    /**
     * Get the echo canceller state
     * @return true if echo cancel activated
     */
    virtual bool getEchoCancelState(void) { return AudioLayer::_echocancelstate; }

    /**
     * Set the echo canceller state
     * @param state true if echocancel active, false elsewhere 
     */
    virtual void setEchoCancelState(bool state);
    
    /**
     * Get the noise suppressor state
     * @return true if noise suppressor activated
     */
    virtual bool getNoiseSuppressState(void) { return AudioLayer::_noisesuppressstate; }

    /**
     * Set the noise suppressor state
     * @param state true if noise suppressor active, false elsewhere
     */
    virtual void setNoiseSuppressState(bool state);

  private:
  
    // Copy Constructor
    AlsaLayer(const AlsaLayer& rh);

    // Assignment Operator
    AlsaLayer& operator=( const AlsaLayer& rh);

    bool is_playback_prepared (void) { return _is_prepared_playback; }
    bool is_capture_prepared (void) { return _is_prepared_capture; }
    void prepare_playback (void) { _is_prepared_playback = true; }
    void prepare_capture (void) { _is_prepared_capture = true; }
    bool is_capture_running (void) { return _is_running_capture; }
    bool is_playback_running (void) { return _is_running_playback; }
    void start_playback (void) { _is_running_playback = true; }
    void stop_playback (void) { _is_running_playback = false; _is_prepared_playback = false; }
    void start_capture (void) { _is_running_capture = true; }
    void stop_capture (void) { _is_running_capture = false; _is_prepared_capture = false; }
    void close_playback (void) { _is_open_playback = false; }
    void close_capture (void) { _is_open_capture = false; }
    void open_playback (void) { _is_open_playback = true; }
    void open_capture (void) { _is_open_capture = true; }
    bool is_capture_open (void) { return _is_open_capture; }
    bool is_playback_open (void) { return _is_open_playback; }
    
    /**
     * Drop the pending frames and close the capture device
     * ALSA Library API
     */
    void closeCaptureStream( void );
    void stopCaptureStream( void );
    void startCaptureStream( void );
    void prepareCaptureStream( void );

    void closePlaybackStream( void );
    void stopPlaybackStream( void );
    void startPlaybackStream( void );
    void preparePlaybackStream( void );

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
    bool open_device( std::string pcm_p, std::string pcm_c, std::string pcm_r,  int flag); 

    bool alsa_set_params( snd_pcm_t *pcm_handle, int type, int rate );

    /**
     * Copy a data buffer in the internal ring buffer
     * ALSA Library API
     * @param buffer The data to be copied
     * @param length The size of the buffer
     * @return int The number of frames actually copied
     */
    int write( void* buffer, int length, snd_pcm_t *handle);
    
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
    void handle_xrun_playback( snd_pcm_t *handle );

    void* adjustVolume( void* buffer , int len, int stream );
    
    /**
     * Handles to manipulate playback stream
     * ALSA Library API
     */
    snd_pcm_t* _PlaybackHandle;

    /**
     * Handles to manipulate ringtone stream
     *
     */
    snd_pcm_t *_RingtoneHandle;

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
     * name of the alsa audio plugin used
     */
    std::string _audioPlugin;

    /** Vector to manage all soundcard index - description association of the system */
    std::vector<HwIDPair> IDSoundCards;

    bool _is_prepared_playback;
    bool _is_prepared_capture;
    bool _is_running_playback;
    bool _is_running_capture;
    bool _is_open_playback;
    bool _is_open_capture;
    bool _trigger_request;
    
    AudioThread* _audioThread;

    /** Sample rate converter object */
    SamplerateConverter* _converter;

};

#endif // _ALSA_LAYER_H_
