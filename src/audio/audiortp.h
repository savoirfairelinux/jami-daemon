/*
 *  Copyright (C) 2004-2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __AUDIO_RTP_H__
#define __AUDIO_RTP_H__

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ccrtp/rtp.h>
#include <cc++/numbers.h>

#include <samplerate.h>
#include "../global.h"

#define UP_SAMPLING 0
#define DOWN_SAMPLING 1

/**
 * @file audiortp.h
 * @brief Manage the real-time data transport in a SIP call
 */

class SIPCall;

///////////////////////////////////////////////////////////////////////////////
// Two pair of sockets
///////////////////////////////////////////////////////////////////////////////
class AudioRtpRTX : public ost::Thread, public ost::TimerPort {
  public:
    /**
     * Constructor
     * @param sipcall The pointer on the SIP call
     * @param sym     Tells whether or not the voip links are symmetric
     */
    AudioRtpRTX (SIPCall* sipcall, bool sym);

    /**
     * Destructor
     */
    ~AudioRtpRTX();

    /** For incoming call notification */ 
    ost::Time *time;

    /** Thread associated method */    
    virtual void run ();

  private:
    /** A SIP call */
    SIPCall* _ca;

    /** RTP session to send data */
    ost::RTPSession *_sessionSend;
    
    /** RTP session to receive data */
    ost::RTPSession *_sessionRecv;

    /** RTP symmetric session ( receive and send data in the same session ) */
    ost::SymmetricRTPSession *_session;

    /** Semaphore */
    ost::Semaphore _start;

    /** Is the session symmetric or not */
    bool _sym;

    /** When we receive data, we decode it inside this buffer */
    int16* _receiveDataDecoded;

    /** Buffers used for send data from the mic */
    unsigned char* _sendDataEncoded;
    
    /** Downsampled int16 buffer */
    int16* _intBufferDown;

    /** After that we send the data inside this buffer if there is a format conversion or rate conversion */
    /** Also use for getting mic-ringbuffer data */
    SFLDataFormat* _dataAudioLayer;

    /** Downsampled float buffer */
    float32* _floatBufferDown;

    /** Upsampled float buffer */
    float32* _floatBufferUp;

    /** libsamplerate converter for incoming voice */
    SRC_STATE*    _src_state_spkr;

    /** libsamplerate converter for outgoing voice */
    SRC_STATE*    _src_state_mic;

    /** libsamplerate error */
    int _src_err;

    /** Variables to process audio stream: sample rate for playing sound (typically 44100HZ) */
    int _layerSampleRate;  

    /** Sample rate of the codec we use to encode and decode (most of time 8000HZ) */
    int _codecSampleRate; 

    /** Length of the sound frame we capture in ms(typically 20ms) */
    int _layerFrameSize; 

    /**
     * Init the RTP session. Create either symmetric or double sessions to manage data transport
     * Set the payloads according to the manager preferences
     */
    void initAudioRtpSession(void);
    
    /**
     * Get the data from the mic, encode it and send it through the RTP session
     * @param timestamp	To manage time and synchronizing
     */ 		 	
    void sendSessionFromMic(int timestamp);
    
    /**
     * Get the data from the RTP packets, decode it and send it to the sound card
     * @param countTime	To manage time and synchronizing
     */		 	
    void receiveSessionForSpkr(int& countTime);

    /**
     * Init the buffers used for processing sound data
     */ 
    void initBuffers(void);

    /**
     * Call the appropriate function, up or downsampling
     * @param sampleRate_codec	The sample rate of the codec selected to encode/decode the data
     * @param nbSamples		Number of samples to process
     * @param status  Type of resampling
     *		      UPSAMPLING
     *		      DOWNSAMPLING
     * @return int The number of samples after process
     */ 
    int reSampleData(int sampleRate_codec, int nbSamples, int status);
    
    /**
     * Upsample the data from the clock rate of the codec to the sample rate of the layer
     * @param sampleRate_codec	The sample rate of the codec selected to encode/decode the data
     * @param nbSamples		Number of samples to process
     * @return int The number of samples after process
     */
    int upSampleData(int sampleRate_codec, int nbSamples);
    
    /**
     * Downsample the data from the sample rate of the layer to the clock rate of the codec
     * @param sampleRate_codec	The sample rate of the codec selected to encode/decode the data
     * @param nbSamples		Number of samples to process
     * @return int The number of samples after process
     */
    int downSampleData(int sampleRate_codec, int nbSamples);

    /** The audio codec used during the session */
    AudioCodec* _audiocodec;	
};

///////////////////////////////////////////////////////////////////////////////
// Main class rtp
///////////////////////////////////////////////////////////////////////////////
class AudioRtp {
  public:
    /**
     * Constructor
     */
    AudioRtp();
    
    /**
     * Destructor
     */
    ~AudioRtp();

    /**
     * Create a brand new RTP session by calling the AudioRtpRTX constructor
     * @param ca A pointer on a SIP call
     */
    int createNewSession (SIPCall *ca);
    
    /**
     * Close a RTP session and kills the remaining threads
     */
    void closeRtpSession( void );

  private:
    /** The RTP thread */
    AudioRtpRTX* _RTXThread;
    
    /** Symmetric session or not */
    bool _symmetric;

    /** Mutex */
    ost::Mutex _threadMutex;
};

#endif // __AUDIO_RTP_H__
