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

class RingBuffer;
class ManagerImpl;

class PulseLayer : public AudioLayer {
  public:
    PulseLayer(ManagerImpl* manager);
    ~PulseLayer(void);

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
     * Check if the capture is running
     * @return true if the state of the capture handle equals SND_PCM_STATE_RUNNING
     *	       false otherwise
     */
    bool isCaptureActive( void ) { return true; }
    bool isStreamActive (void); 

    void flushMain();
    int putMain(void* buffer, int toCopy);
    int putUrgent(void* buffer, int toCopy);
    int putInCache( char code, void* buffer , int toCopy );
    int canGetMic();
    int getMic(void *, int);
    void flushMic();

    /**
     * Send samples to the audio device. 
     * @param buffer The buffer containing the data to be played ( voice and DTMF )
     * @param toCopy The number of samples, in bytes
     * @param isTalking	If whether or not the conversation is running
     * @return int The number of bytes played
     */
    int playSamples(void* buffer, int toCopy, bool isTalking) ;

    static void audioCallback ( pa_stream* s, size_t bytes, void* userdata );
    static void overflow ( pa_stream* s, void* userdata );
    static void underflow ( pa_stream* s, void* userdata );
    static void stream_state_callback( pa_stream* s, void* user_data );	

    static void context_state_callback( pa_context* c, void* user_data );	

    /**
     * Scan the sound card available on the system
     * @param stream To indicate whether we are looking for capture devices or playback devices
     *		   SFL_PCM_CAPTURE
     *		   SFL_PCM_PLAYBACK
     *		   SFL_PCM_BOTH
     * @return std::vector<std::string> The vector containing the string description of the card
     */
    std::vector<std::string> getSoundCardsInfo( int stream ) { 
      std::vector<std::string> tmp;
      return tmp; 
    }

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
    bool soundCardIndexExist( int card , int stream ) { return true; }
    
    /**
     * An index is associated with its string description
     * @param description The string description
     * @return	int	  Its index
     */
    int soundCardGetIndex( std::string description ) { return 0;}

    /**
     * Get the current audio plugin.
     * @return std::string  The name of the audio plugin
     */
    std::string getAudioPlugin( void ) { return "default"; }
    

  private:
    /**
     * Drop the pending frames and close the capture device
     */
    void closeCaptureStream( void );

    void processData( void );
    void createStreams( pa_context* c );
    /**
     * Drop the pending frames and close the playback device
     */
    void closePlaybackStream( void );

    void connectPulseServer( void );

    /** Ringbuffers for data */
    RingBuffer _mainSndRingBuffer;
    RingBuffer _urgentRingBuffer;
    RingBuffer _micRingBuffer;

    /** PulseAudio streams and context */
    pa_context* context;
    pa_threaded_mainloop* m;

    AudioStream* playback;
    AudioStream* record;
    //AudioStream* cache;

    //pa_stream* playback;
    //pa_stream* record;
};

#endif // _PULSE_LAYER_H_

