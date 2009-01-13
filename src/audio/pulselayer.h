/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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

#ifndef _PULSE_LAYER_H
#define _PULSE_LAYER_H

#include "audiolayer.h"
#include "audiostream.h"

#define PLAYBACK_STREAM_NAME	    "SFLphone out"
#define CAPTURE_STREAM_NAME	    "SFLphone in"

class RingBuffer;
class ManagerImpl;

class PulseLayer : public AudioLayer {
  public:
    PulseLayer(ManagerImpl* manager);
    ~PulseLayer(void);

    void closeLayer( void );

    void trigger_thread(void){}

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
    bool openDevice(int indexIn, int indexOut, int sampleRate, int frameSize , int stream, std::string plugin) ;

    void startStream(void);

    void stopStream(void);

    /**
     * UNUSED in pulseaudio layer
     */
    bool isCaptureActive( void ) { return true; }

    /**
     * UNUSED in pulseaudio layer
     */
    bool isStreamActive (void); 

    /**
     * Flush the main ringbuffer, reserved for the voice
     */
    void flushMain();
    
    int putUrgent(void* buffer, int toCopy);

    /**
     * UNUSED in pulseaudio layer
     */
    int putInCache( char code, void* buffer , int toCopy );

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
    int getMic(void *, int);
    
    /**
     * Flush the mic ringbuffer
     */
    void flushMic();

    /**
     * UNUSED in pulseaudio layer
     */
    int playSamples(void* buffer, int toCopy, bool isTalking) ;

    static void audioCallback ( pa_stream* s, size_t bytes, void* userdata );
    static void overflow ( pa_stream* s, void* userdata );
    static void underflow ( pa_stream* s, void* userdata );
    static void stream_state_callback( pa_stream* s, void* user_data );	
    static void context_state_callback( pa_context* c, void* user_data );	

    /**
     * UNUSED in pulseaudio layer
     */
    std::vector<std::string> getSoundCardsInfo( int stream UNUSED ) { 
      std::vector<std::string> tmp;
      return tmp; 
    }

    /**
     * UNUSED in pulseaudio layer
     */
    bool soundCardIndexExist( int card UNUSED, int stream UNUSED ) { return true; }
    
    /**
     * UNUSED in pulseaudio layer
     */
    int soundCardGetIndex( std::string description UNUSED ) { return 0;}

    /**
     * UNUSED in pulseaudio layer
     */
    std::string getAudioPlugin( void ) { return "default"; }
    
    /**
     * Reduce volume of every audio applications connected to the same sink
     */
    void reducePulseAppsVolume( void );
    
    /**
     * Restore the volume of every audio applications connected to the same sink to PA_VOLUME_NORM
     */
    void restorePulseAppsVolume( void );

    /**
     * Set the volume of a sink. 
     * @param index The index of the stream 
     * @param channels	The stream's number of channels
     * @param volume The new volume (between 0 and 100)
     */
    void setSinkVolume( int index, int channels, int volume );
    void setSourceVolume( int index, int channels, int volume );

    void setPlaybackVolume( int volume );
    void setCaptureVolume( int volume );

    /**
     * Accessor
     * @return AudioStream* The pointer on the playback AudioStream object
     */
    AudioStream* getPlaybackStream(){ return playback;}

    /**
     * Accessor
     * @return AudioStream* The pointer on the record AudioStream object
     */
    AudioStream* getRecordStream(){ return record;}

    int getSpkrVolume( void ) { return spkrVolume; }
    void setSpkrVolume( int value ) { spkrVolume = value; }

    int getMicVolume( void ) { return micVolume; }
    void setMicVolume( int value ) { micVolume = value; }

  private:
    // Copy Constructor
    PulseLayer(const PulseLayer& rh);

    // Assignment Operator
    PulseLayer& operator=( const PulseLayer& rh);


    /**
     * Drop the pending frames and close the capture device
     */
    void closeCaptureStream( void );

    /**
     * Write data from the ring buffer to the harware and read data from the hardware
     */
    void processData( void );
    void readFromMic( void );
    void writeToSpeaker( void );
    
    /**
     * Create the audio streams into the given context
     * @param c	The pulseaudio context
     */ 
    void createStreams( pa_context* c );

    /**
     * Drop the pending frames and close the playback device
     */
    void closePlaybackStream( void );

    /**
     * Establishes the connection with the local pulseaudio server
     */
    void connectPulseAudioServer( void );

    /**
     * Close the connection with the local pulseaudio server
     */
    void disconnectPulseAudioServer( void );

    /**
     * Get some information about the pulseaudio server
     */
    void serverinfo( void );

    /** 
     * Ringbuffer for incoming voice data (playback)
     */
    RingBuffer _mainSndRingBuffer;

    /** 
     * Ringbuffer for dtmf data
     */
    RingBuffer _urgentRingBuffer;

    /** 
     * Ringbuffer for outgoing voice data (mic)
     */
    RingBuffer _micRingBuffer;

    /** PulseAudio context and asynchronous loop */
    pa_context* context;
    pa_threaded_mainloop* m;
    
    /**
     * A stream object to handle the pulseaudio playback stream
     */
    AudioStream* playback;

    /**
     * A stream object to handle the pulseaudio capture stream
     */
    AudioStream* record;

    /**
     * A stream object to handle the pulseaudio upload stream
     */
    AudioStream* cache;

    int spkrVolume;
    int micVolume;

};

#endif // _PULSE_LAYER_H_

