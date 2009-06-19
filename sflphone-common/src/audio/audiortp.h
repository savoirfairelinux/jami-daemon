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

#include "../global.h"
// #include "plug-in/audiorecorder/audiorecord.h"
#include "../samplerateconverter.h"

#define UP_SAMPLING 0
#define DOWN_SAMPLING 1



class AudioRtpException: public std::exception
{
  virtual const char* what() const throw()
  {
    return "AudioRtpException occured";
  }
};


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

    /**
     * Audio recording object
     */
    // AudioRecord recAudio;

    /** A SIP call */
    SIPCall* _ca;

   /**
     * Update RTP session media info as received from SDP negociation 
     */
    void setRtpSessionMedia(void);

    /**
     * Update RTP session remote ip destination as received from sip transaction 
     */
    void setRtpSessionRemoteIp(void); 

    
    friend class RtpTest;

  private:

    // copy constructor
    AudioRtpRTX(const AudioRtpRTX& rh);
  
    // assignment operator
    AudioRtpRTX& operator=(const AudioRtpRTX& rh);

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

    /** Mic-data related buffers */
    SFLDataFormat* micData;
    SFLDataFormat* micDataConverted;
    unsigned char* micDataEncoded;

    /** Speaker-data related buffers */
    SFLDataFormat* spkrDataDecoded;
    SFLDataFormat* spkrDataConverted;

    /** Sample rate converter object */
    SamplerateConverter* converter;

    /** audio layer */
    AudioLayer *_audiolayer;

    /** Variables to process audio stream: sample rate for playing sound (typically 44100HZ) */
    int _layerSampleRate;  

    /** Sample rate of the codec we use to encode and decode (most of time 8000HZ) */
    int _codecSampleRate;

    /** Length of the sound frame we capture in ms (typically 20ms) */
    int _layerFrameSize; 

    /** Codecs frame size in samples (20 ms => 882 at 44.1kHz)
        The exact value is stored in the codec */
    int _codecFrameSize;

    /** Speaker buffer length in samples once the data are resampled
     *  (used for mixing and recording)
     */
    int _nSamplesSpkr; 

    /** Mic buffer length in samples once the data are resampled
     *  (used for mixing and recording)
     */
    int _nSamplesMic;
    
    /**
     * Maximum number of sample for audio buffers (mic and spkr)
     */
    int nbSamplesMax; 
    
    /**
     * Init the RTP session. Create either symmetric or double sessions to manage data transport
     * Set the payloads according to the manager preferences
     */
    void initAudioRtpSession(void);
    
    /**
     * Return the lenth the codec frame in ms 
     */
    float computeCodecFrameSize(int codecSamplePerFrame, int codecClockRate);

    /**
     * Compute nb of byte to get coresponding to X ms at audio layer frame size (44.1 khz) 
     */
    int computeNbByteAudioLayer(float codecFrameSize);


    int processDataEncode();


    void processDataDecode(unsigned char* spkrData, unsigned int size, int& countTime);


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
    int reSampleData(SFLDataFormat *input, SFLDataFormat *output,int sampleRate_codec, int nbSamples, int status);
    
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
    void createNewSession (SIPCall *ca);
    
    /**
     * Start the AudioRtpRTX thread created with createNewSession
     */
    int start(void);
    
    /**
     * Close a RTP session and kills the remaining threads
     */
    bool closeRtpSession( void );

    /**
     * Return a pointer to the RTP thead
     */
    AudioRtpRTX* getRTX();

    /**
     * Start recording
     */
    void setRecording ();

    friend class RtpTest; 

  private:
    // copy constructor
    AudioRtp(const AudioRtp& rh);
  
    // assignment operator
    AudioRtp& operator=(const AudioRtp& rh);

    /** The RTP thread */
    AudioRtpRTX* _RTXThread;
    
    /** Symmetric session or not */
    bool _symmetric;

    /** Mutex */
    ost::Mutex _threadMutex;
};

#endif // __AUDIO_RTP_H__
