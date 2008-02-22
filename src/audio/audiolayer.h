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
     * @param indexIn
     * @param indexOut
     * @param sampleRate
     * @param frameSize
     * @param flag - 0 --> open playback and capture ; 1 --> open playback only ; 2 --> open capture only
     */
    bool openDevice(int, int, int, int, int);
    void startStream(void);
    void stopStream(void);
    void sleep(int);
    bool isPlaybackActive( void );
    bool isCaptureActive( void );
    bool isStreamActive(void);
    bool isStreamStopped(void);
    void closeStream();

    int playSamples(void* buffer, int toCopy);
    int playRingTone( void* buffer, int toCopy);
    int putUrgent(void* buffer, int toCopy);
    int canGetMic();
    int getMic(void *, int);
    std::vector<std::string> get_sound_cards( void );
    std::string buildDeviceTopo( std::string prefixe, int suffixe);
    std::vector<std::string> getHardware( int flag );

    int audioCallback (const void *, void *, unsigned long);

    void setErrorMessage(const std::string& error) { _errorMessage = error; }
    std::string getErrorMessage() { return _errorMessage; }

    /**
     * Get the sample rate of audiolayer
     * accessor only
     */
    int getIndexIn() { return _indexIn; }
    int getIndexOut() { return _indexOut; }
    unsigned int getSampleRate() { return _sampleRate; }
    unsigned int getFrameSize() { return _frameSize; }
    int getDeviceCount();

    void playSinusWave();


    // NOW
    //void selectPreferedApi(PaHostApiTypeId apiTypeID, int& outputDeviceIndex, int& inputDeviceIndex);

    //std::vector<std::string> getAudioDeviceList(PaHostApiTypeId apiTypeID, int ioDeviceMask);


    //AudioDevice* getAudioDeviceInfo(int index, int ioDeviceMask);

    enum IODEVICE {InputDevice=0x01, OutputDevice=0x02 };

    /**
     * Toggle echo testing on/off
     */
    void toggleEchoTesting();

  private:
    bool open_device( std::string , std::string , int); 
    int write( void* , int );
    int read( void*, int );
    bool is_playback_active( void );
    bool is_capture_active( void );
    void handle_xrun_state( void );
    void get_devices_info( void );
    std::string get_alsa_version( void );
    std::vector<std::string> parse_sound_cards( std::string& );
    ManagerImpl* _manager; // augment coupling, reduce indirect access
    // a audiolayer can't live without manager

    snd_pcm_t* _playback_handle;
    snd_pcm_t* _capture_handle;
    bool device_closed;

    /**
     * Portaudio indexes of audio devices on which stream has been opened 
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

