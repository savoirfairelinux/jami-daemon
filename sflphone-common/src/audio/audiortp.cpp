/*
 *  Copyright (C) 2004-2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
 */

#include <cstdio>
#include <cstdlib>
#include <ccrtp/rtp.h>
#include <assert.h>
#include <string>
#include <cstring>
#include <math.h>
#include <dlfcn.h>
#include <iostream>
#include <sstream>

#include "../global.h"
#include "../manager.h"
#include "codecDescriptor.h"
#include "audiortp.h"
#include "audiolayer.h"
#include "ringbuffer.h"
#include "../user_cfg.h"
#include "../sipcall.h"

////////////////////////////////////////////////////////////////////////////////
// AudioRtp                                                          
////////////////////////////////////////////////////////////////////////////////
AudioRtp::AudioRtp() :_RTXThread(0), _symmetric(), _threadMutex()
{
}

AudioRtp::~AudioRtp (void) {
    delete _RTXThread; _RTXThread = 0;
}

int 
AudioRtp::createNewSession (SIPCall *ca) {

    ost::MutexLock m(_threadMutex);

    _debug("AudioRtp::Create new rtp session\n");

    // something should stop the thread before...
    if ( _RTXThread != 0 ) { 
        _debug("**********************************************************\n");
        _debug("! ARTP Failure: Thread already exists..., stopping it\n");
        _debug("**********************************************************\n");
        delete _RTXThread; _RTXThread = 0;
    }

    // Start RTP Send/Receive threads
    _symmetric = Manager::instance().getConfigInt(SIGNALISATION,SYMMETRIC) ? true : false;
    _RTXThread = new AudioRtpRTX (ca, _symmetric);
    try {
        if (_RTXThread->start() != 0) {
            _debug("! ARTP Failure: unable to start RTX Thread\n");
            return -1;
        }
    } catch(...) {
        _debugException("! ARTP Failure: when trying to start a thread");
        throw;
    }

    return 0;
}


bool
AudioRtp::closeRtpSession () {

    ost::MutexLock m(_threadMutex);
    // This will make RTP threads finish.
    _debug("AudioRtp::Stopping rtp session\n");
    

    try {
        delete _RTXThread; _RTXThread = 0;
    } catch(...) {
        _debugException("! ARTP Exception: when stopping audiortp\n");
        throw;
    }
    AudioLayer* audiolayer = Manager::instance().getAudioDriver();
    // audiolayer->stopStream();

    _debug("AudioRtp::Audio rtp stopped\n");
    return true;
}


AudioRtpRTX*
AudioRtp::getRTX()
{
    return _RTXThread;
}


void
AudioRtp::setRecording() {

    _debug("AudioRtp::setRecording\n");
    _RTXThread->_ca->setRecording();

}





////////////////////////////////////////////////////////////////////////////////
// AudioRtpRTX Class                                                          //
////////////////////////////////////////////////////////////////////////////////
AudioRtpRTX::AudioRtpRTX (SIPCall *sipcall, bool sym) : time(new ost::Time()), _ca(sipcall), _sessionSend(NULL), _sessionRecv(NULL), _session(NULL), _start(), 
    _sym(sym), micData(NULL), micDataConverted(NULL), micDataEncoded(NULL), spkrDataDecoded(NULL), spkrDataConverted(NULL), 
    converter(NULL), _layerSampleRate(),_codecSampleRate(), _layerFrameSize(), _audiocodec(NULL)
{

    setCancel(cancelDefault);
    // AudioRtpRTX should be close if we change sample rate
    // TODO: Change bind address according to user settings.
    // TODO: this should be the local ip not the external (router) IP
    std::string localipConfig = _ca->getLocalIp(); // _ca->getLocalIp();
    ost::InetHostAddress local_ip(localipConfig.c_str());
    if (!_sym) {
        _sessionRecv = new ost::RTPSession(local_ip, _ca->getLocalAudioPort());
        _sessionSend = new ost::RTPSession(local_ip, _ca->getLocalAudioPort());
        _session = NULL;
    } else {
        _debug ("%i\n", _ca->getLocalAudioPort());
        _session = new ost::SymmetricRTPSession (local_ip, _ca->getLocalAudioPort());
        _sessionRecv = NULL;
        _sessionSend = NULL;
    }
}

AudioRtpRTX::~AudioRtpRTX () {

    _debug("Delete AudioRtpRTX instance\n");
    _start.wait();
    _debug("Just passed AudioRtpRTX semaphore wait\n");

    try {
        this->terminate();
    } catch(...) {
        _debugException("! ARTP: Thread destructor didn't terminate correctly");
        throw;
    }
    _ca = 0;
    if (!_sym) {
        delete _sessionRecv; _sessionRecv = NULL;
        delete _sessionSend; _sessionSend = NULL;
    } else {
        _debug("Delete ost::RTPSession in AudioRtpRTX\n");
        delete _session;     _session = NULL;
    }

    _debug("Just killed AudioRtpRTX rtp sessions\n");

    delete [] micData;  micData = NULL;
    delete [] micDataConverted;  micDataConverted = NULL;
    delete [] micDataEncoded;  micDataEncoded = NULL;

    delete [] spkrDataDecoded; spkrDataDecoded = NULL;
    delete [] spkrDataConverted; spkrDataConverted = NULL;

    delete time; time = NULL;

    delete converter; converter = NULL;

    _debug("AudioRtpRTX instance deleted with all ring buffers, and converters\n");
}


    void
AudioRtpRTX::initBuffers()
{
    converter = new SamplerateConverter( _layerSampleRate , _layerFrameSize );

    int nbSamplesMax = (int) (_layerSampleRate * _layerFrameSize /1000);

    micData = new SFLDataFormat[nbSamplesMax];
    micDataConverted = new SFLDataFormat[nbSamplesMax];
    micDataEncoded = new unsigned char[nbSamplesMax];

    spkrDataConverted = new SFLDataFormat[nbSamplesMax];
    spkrDataDecoded = new SFLDataFormat[nbSamplesMax];
}


    void
AudioRtpRTX::initAudioRtpSession (void) 
{

    try {

        setRtpSessionMedia();

        /*
        if (_ca == 0) { return; }
        _audiocodec = _ca->getLocalSDP()->get_session_media ();

        if (_audiocodec == NULL) { return; }

        _codecSampleRate = _audiocodec->getClockRate();
        _codecFrameSize = _audiocodec->getFrameSize();

        ost::InetHostAddress remote_ip(_ca->getLocalSDP()->get_remote_ip().c_str());
        _debug("Init audio RTP session %s\n", _ca->getLocalSDP()->get_remote_ip().data());
        if (!remote_ip) {
            _debug("! ARTP Thread Error: Target IP address [%s] is not correct!\n", _ca->getLocalSDP()->get_remote_ip().data());
            return;
        }
      

        if (!_sym) {
            _sessionRecv->setSchedulingTimeout (10000);
            _sessionRecv->setExpireTimeout(1000000);

            _sessionSend->setSchedulingTimeout(10000);
            _sessionSend->setExpireTimeout(1000000);
        } else {
            _session->setSchedulingTimeout(10000);
            _session->setExpireTimeout(1000000);
        }
	*/

	setRtpSessionRemoteIp();

        /*
        if (!_sym) {
	    if ( !_sessionRecv->addDestination(remote_ip, (unsigned short) _ca->getLocalSDP()->get_remote_audio_port()) ) {
                _debug("AudioRTP Thread Error: could not connect to port %d\n",  _ca->getLocalSDP()->get_remote_audio_port());
                return;
            }
            if (!_sessionSend->addDestination (remote_ip, (unsigned short) _ca->getLocalSDP()->get_remote_audio_port())) {
                _debug("! ARTP Thread Error: could not connect to port %d\n", _ca->getLocalSDP()->get_remote_audio_port());
                return;
            }

            bool payloadIsSet = false;
            if (_audiocodec) {
                if (_audiocodec->hasDynamicPayload()) {
                    payloadIsSet = _sessionRecv->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) _audiocodec->getPayload(), _audiocodec->getClockRate()));
                } else {
                    payloadIsSet= _sessionRecv->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _audiocodec->getPayload()));
                    payloadIsSet = _sessionSend->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _audiocodec->getPayload()));
                }
            }
            _sessionSend->setMark(true);
        } else {

            if (!_session->addDestination (remote_ip, (unsigned short)_ca->getLocalSDP()->get_remote_audio_port() )) {
                return;
            }

            bool payloadIsSet = false;
            if (_audiocodec) {
                if (_audiocodec->hasDynamicPayload()) {
                    payloadIsSet = _session->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) _audiocodec->getPayload(), _audiocodec->getClockRate()));
                } else {
                    payloadIsSet = _session->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _audiocodec->getPayload()));
                }
            }
        }
	*/


    } catch(...) {
        _debugException("! ARTP Failure: initialisation failed");
        throw;
    }    

}

void
AudioRtpRTX::setRtpSessionMedia(void)
{

    _debug("setRtpSessionMedia()\n");

    if (_ca == 0) { return; }

    
    _audiocodec = _ca->getLocalSDP()->get_session_media ();

    if (_audiocodec == NULL) { return; }

    _codecSampleRate = _audiocodec->getClockRate();
    _codecFrameSize = _audiocodec->getFrameSize();

    if (!_sym) {

        bool payloadIsSet = false;
        if (_audiocodec) {
            if (_audiocodec->hasDynamicPayload()) {
                payloadIsSet = _sessionRecv->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) _audiocodec->getPayload(), _audiocodec->getClockRate()));
            } else {
                payloadIsSet= _sessionRecv->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _audiocodec->getPayload()));
                payloadIsSet = _sessionSend->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _audiocodec->getPayload()));
            }
        }
        _sessionSend->setMark(true);
    } else {

        bool payloadIsSet = false;
        if (_audiocodec) {
            if (_audiocodec->hasDynamicPayload()) {
                payloadIsSet = _session->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) _audiocodec->getPayload(), _audiocodec->getClockRate()));
            } else {
                payloadIsSet = _session->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _audiocodec->getPayload()));
            }
        }
    }

}

void
AudioRtpRTX::setRtpSessionRemoteIp(void)
{
    if (_ca == 0) { return; }

    ost::InetHostAddress remote_ip(_ca->getLocalSDP()->get_remote_ip().c_str());
    _debug("Init audio RTP session %s\n", _ca->getLocalSDP()->get_remote_ip().data());
    if (!remote_ip) {
        _debug("! ARTP Thread Error: Target IP address [%s] is not correct!\n", _ca->getLocalSDP()->get_remote_ip().data());
        return;
    }
    
    if (!_sym) {

	if ( !_sessionRecv->addDestination(remote_ip, (unsigned short) _ca->getLocalSDP()->get_remote_audio_port()) ) {
            _debug("AudioRTP Thread Error: could not connect to port %d\n",  _ca->getLocalSDP()->get_remote_audio_port());
            return;
        }
        if (!_sessionSend->addDestination (remote_ip, (unsigned short) _ca->getLocalSDP()->get_remote_audio_port())) {
            _debug("! ARTP Thread Error: could not connect to port %d\n", _ca->getLocalSDP()->get_remote_audio_port());
            return;
        }

        _sessionSend->setMark(true);
    } else {

        if (!_session->addDestination (remote_ip, (unsigned short)_ca->getLocalSDP()->get_remote_audio_port() )) {
            return;
        }

    }

}



float 
AudioRtpRTX::computeCodecFrameSize(int codecSamplePerFrame, int codecClockRate)
{
    return ( (float)codecSamplePerFrame * 1000.0 ) / (float)codecClockRate;
}

int
AudioRtpRTX::computeNbByteAudioLayer(float codecFrameSize)
{
    return (int)((float)_layerSampleRate * codecFrameSize * (float)sizeof(SFLDataFormat) / 1000.0);
}


int
AudioRtpRTX::processDataEncode(AudioLayer* audiolayer)
{

    // compute codec framesize in ms
    float fixed_codec_framesize = computeCodecFrameSize(_audiocodec->getFrameSize(), _audiocodec->getClockRate());

    // compute nb of byte to get coresponding to 20 ms at audio layer frame size (44.1 khz)
    int maxBytesToGet = computeNbByteAudioLayer(fixed_codec_framesize);
    
    // available bytes inside ringbuffer
    int availBytesFromMic = audiolayer->canGetMic();

    // set available byte to maxByteToGet
    int bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;

    if (bytesAvail == 0)
      return 0;

    // Get bytes from micRingBuffer to data_from_mic
    int nbSample = audiolayer->getMic( micData , bytesAvail ) / sizeof(SFLDataFormat);

    // nb bytes to be sent over RTP
    int compSize = 0;

    // test if resampling is required
    if (_audiocodec->getClockRate() != _layerSampleRate) {

        int nb_sample_up = nbSample;
        // _debug("_nbSample audiolayer->getMic(): %i \n", nbSample);
    
        // Store the length of the mic buffer in samples for recording
        _nSamplesMic = nbSample;


        // int nbSamplesMax = _layerFrameSize * _audiocodec->getClockRate() / 1000; 
	 nbSample = reSampleData(micData , micDataConverted, _audiocodec->getClockRate(), nb_sample_up, DOWN_SAMPLING);

        compSize = _audiocodec->codecEncode( micDataEncoded, micDataConverted, nbSample*sizeof(int16));

    } else {
        // no resampling required

        // int nbSamplesMax = _codecFrameSize;
        compSize = _audiocodec->codecEncode( micDataEncoded, micData, nbSample*sizeof(int16));

    }

    return compSize;
}


void
AudioRtpRTX::processDataDecode(AudioLayer* audiolayer, unsigned char* spkrData, unsigned int size, int& countTime)
{

    if (_audiocodec != NULL) {

        // Return the size of data in bytes 
        int expandedSize = _audiocodec->codecDecode( spkrDataDecoded , spkrData , size );

        // buffer _receiveDataDecoded ----> short int or int16, coded on 2 bytes
        int nbSample = expandedSize / sizeof(SFLDataFormat);

        // test if resampling is required 
        if (_audiocodec->getClockRate() != _layerSampleRate) {

            // Do sample rate conversion
            int nb_sample_down = nbSample;
            nbSample = reSampleData(spkrDataDecoded , spkrDataConverted, _codecSampleRate , nb_sample_down, UP_SAMPLING);

            // Store the number of samples for recording
            _nSamplesSpkr = nbSample;

	    // put data in audio layer, size in byte
            audiolayer->putMain (spkrDataConverted, nbSample * sizeof(SFLDataFormat));

        } else {

            // Stor the number of samples for recording
            _nSamplesSpkr = nbSample;

	    // put data in audio layer, size in byte
            audiolayer->putMain (spkrDataDecoded, nbSample * sizeof(SFLDataFormat));
        }

        // Notify (with a beep) an incoming call when there is already a call 
        countTime += time->getSecond();
        if (Manager::instance().incomingCallWaiting() > 0) {
            countTime = countTime % 500; // more often...
            if (countTime == 0) {
                Manager::instance().notificationIncomingCall();
            }
        }

    } else {
        countTime += time->getSecond();
    }
}

    void
AudioRtpRTX::sendSessionFromMic(int timestamp)
{
    // STEP:
    //   1. get data from mic
    //   2. convert it to int16 - good sample, good rate
    //   3. encode it
    //   4. send it
    

    timestamp += time->getSecond();
    // no call, so we do nothing
    if (_ca==0) { _debug(" !ARTP: No call associated (mic)\n"); return; }

    AudioLayer* audiolayer = Manager::instance().getAudioDriver();
    if (!audiolayer) { _debug(" !ARTP: No audiolayer available for mic\n"); return; }

    if (!_audiocodec) { _debug(" !ARTP: No audiocodec available for mic\n"); return; }

    int compSize = processDataEncode(audiolayer);

    // putData put the data on RTP queue, sendImmediate bypass this queue
    if (!_sym) {
        // _sessionSend->putData(timestamp, micDataEncoded, compSize);
        _sessionSend->sendImmediate(timestamp, micDataEncoded, compSize);
    } else {
        // _session->putData(timestamp, micDataEncoded, compSize);
        _session->sendImmediate(timestamp, micDataEncoded, compSize);
    }
    
}


void
AudioRtpRTX::receiveSessionForSpkr (int& countTime)
{

    if (_ca == 0) { return; }


    AudioLayer* audiolayer = Manager::instance().getAudioDriver();
    if (!audiolayer) { return; }

    const ost::AppDataUnit* adu = NULL;


    if (!_sym) {
        adu = _sessionRecv->getData(_sessionRecv->getFirstTimestamp());
    } else {
        adu = _session->getData(_session->getFirstTimestamp());
    }
    if (adu == NULL) {
        // _debug("No RTP audio stream\n");
        return;
    }

    unsigned char* spkrData  = (unsigned char*)adu->getData(); // data in char
    unsigned int size = adu->getSize(); // size in char

    processDataDecode(audiolayer, spkrData, size, countTime);
    
}


    int 
    AudioRtpRTX::reSampleData(SFLDataFormat *input, SFLDataFormat *output, int sampleRate_codec, int nbSamples, int status)
{
    if(status==UP_SAMPLING){
        return converter->upsampleData( input, output, sampleRate_codec , _layerSampleRate , nbSamples );
    }
    else if(status==DOWN_SAMPLING){
        return converter->downsampleData( micData , micDataConverted , sampleRate_codec , _layerSampleRate , nbSamples );
    }
    else
        return 0;
}



void
AudioRtpRTX::run () {

  //mic, we receive from soundcard in stereo, and we send encoded
  //encoding before sending
  AudioLayer *audiolayer = Manager::instance().getAudioDriver();
  _layerFrameSize = audiolayer->getFrameSize(); // en ms
  _layerSampleRate = audiolayer->getSampleRate();
  
  initBuffers();
  int step; 

  int sessionWaiting;

  //try {

    // Init the session
    initAudioRtpSession();


    // step = (int) (_layerFrameSize * _codecSampleRate / 1000);
    step = _codecFrameSize;
    // start running the packet queue scheduler.
    //_debug("AudioRTP Thread started\n");
    if (!_sym) {
        _sessionRecv->startRunning();
        _sessionSend->startRunning();
    } else {
        _session->startRunning();
        //_debug("Session is now: %d active\n", _session->isActive());
    }

    int timestamp = 0; // for mic
    // step = (int) (_layerFrameSize * _codecSampleRate / 1000);
    step = _codecFrameSize;

    int countTime = 0; // for receive
    
    int threadSleep = 0;
    if (_codecSampleRate != 0)
        threadSleep = (_codecFrameSize * 1000) / _codecSampleRate;
    else
      threadSleep = _layerFrameSize;

    TimerPort::setTimer(threadSleep);

    audiolayer->startStream();
    _start.post();
    _debug("- ARTP Action: Start call %s\n",_ca->getCallId().c_str());
    while (!testCancel()) {

     
      // printf("AudioRtpRTX::run() _session->getFirstTimestamp() %i \n",_session->getFirstTimestamp());
    
      // printf("AudioRtpRTX::run() _session->isWaiting() %i \n",_session->isWaiting());
      /////////////////////
      ////////////////////////////
      // Send session
      ////////////////////////////

      sessionWaiting = _session->isWaiting();

      sendSessionFromMic(timestamp); 
      timestamp += step;
      
      ////////////////////////////
      // Recv session
      ////////////////////////////
      receiveSessionForSpkr(countTime);
      
      // Let's wait for the next transmit cycle

      
      if(sessionWaiting == 1){
        // Record mic and speaker during conversation
        _ca->recAudio.recData(spkrDataConverted,micData,_nSamplesSpkr,_nSamplesMic);
      }
      else {
        // Record mic only while leaving a message
        _ca->recAudio.recData(micData,_nSamplesMic);
      }

      Thread::sleep(TimerPort::getTimer());
      // TimerPort::incTimer(20); // 'frameSize' ms
      TimerPort::incTimer(threadSleep);
      
    }
    
    // audiolayer->stopStream();
    _debug("- ARTP Action: Stop call %s\n",_ca->getCallId().c_str());
  //} catch(std::exception &e) {
    //_start.post();
    //_debug("! ARTP: Stop %s\n", e.what());
    //throw;
  //} catch(...) {
    //_start.post();
    //_debugException("* ARTP Action: Stop");
    //throw;
  //}
    
}


// EOF
