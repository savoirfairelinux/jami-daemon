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

#ifndef _PULSE_LAYER_H
#define _PULSE_LAYER_H

#include "audio/audiolayer.h"
#include "audio/samplerateconverter.h"
#include "audio/dcblocker.h"
#include "audiostream.h"

#include <pulse/pulseaudio.h>
#include <pulse/stream.h>

#include <stdlib.h>

#include <list>
#include <string>

#define PLAYBACK_STREAM_NAME	    "SFLphone playback"
#define CAPTURE_STREAM_NAME	    "SFLphone capture"
#define RINGTONE_STREAM_NAME        "SFLphone ringtone"

class RingBuffer;
class ManagerImpl;

typedef std::list<std::string> DeviceList;

class PulseLayer : public AudioLayer {
  public:
    PulseLayer(ManagerImpl* manager);
    ~PulseLayer(void);

    void openLayer( void );
    
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
    bool openDevice(int indexIn, int indexOut, int indexRing, int sampleRate, int frameSize , int stream, std::string plugin) ;

    DeviceList* getSinkList(void) { return &_sinkList; }

    DeviceList* getSourceList(void) { return &_sourceList; }

    void updateSinkList(void);

    void updateSourceList(void);

    bool inSinkList(std::string deviceName);

    bool inSourceList(std::string deviceName);

    void startStream(void);

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
    int getMic(void *, int);
    
    static void overflow ( pa_stream* s, void* userdata );
    static void underflow ( pa_stream* s, void* userdata );
    static void stream_state_callback( pa_stream* s, void* user_data );	
    static void context_state_callback( pa_context* c, void* user_data );
    // static void stream_suspended_callback ( pa_stream* s, void* userdata );	

    bool isCaptureActive (void){return true;}

    /**
     * UNUSED in pulseaudio layer
     */
    //std::vector<std::string> getSoundCardsInfo( int stream UNUSED ) { 
      //std::vector<std::string> tmp;
      //return tmp; 
    //}

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

    /**
     * Accessor
     * @return AudioStream* The pointer on the ringtone AudioStream object
     */
    AudioStream* getRingtoneStream(){ return ringtone;}

    int getSpkrVolume( void ) { return spkrVolume; }
    void setSpkrVolume( int value ) { spkrVolume = value; }

    int getMicVolume( void ) { return micVolume; }
    void setMicVolume( int value ) { micVolume = value; }

    void processPlaybackData( void );

    void processCaptureData( void );

    void processRingtoneData( void );

    void processData(void);

    /**
     * Get the echo canceller state
     * @return true if echo cancel activated
     */
    bool getEchoCancelState(void) { return AudioLayer::_echocancelstate; }

    /**
     * Set the echo canceller state
     * @param state true if echocancel active, false elsewhere 
     */
    void setEchoCancelState(bool state);
    
    /**
     * Get the noise suppressor state
     * @return true if noise suppressor activated
     */
    bool getNoiseSuppressState(void) { return AudioLayer::_noisesuppressstate; }

    /**
     * Set the noise suppressor state
     * @param state true if noise suppressor active, false elsewhere
     */
    void setNoiseSuppressState(bool state);
    
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
    void readFromMic( void );
    void writeToSpeaker( void );
    void ringtoneToSpeaker( void );
    
    /**
     * Create the audio streams into the given context
     * @param c	The pulseaudio context
     */ 
    bool createStreams( pa_context* c );

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
    bool disconnectAudioStream( void );

    /**
     * Get some information about the pulseaudio server
     */
    void serverinfo( void );

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
     * A special stream object to handle specific playback stream for ringtone
     */
    AudioStream* ringtone;

    /** Sample rate converter object */
    SamplerateConverter * _converter;

    bool is_started;

    int spkrVolume;
    int micVolume;

    /*
    ofstream *captureFile;
    ofstream *captureRsmplFile;
    ofstream *captureFilterFile;
    */

    DeviceList _sinkList;

    DeviceList _sourceList;

    // private:

    int byteCounter;

public: 

    friend class AudioLayerTest;
};

#endif // _PULSE_LAYER_H_

