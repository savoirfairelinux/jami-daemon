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

  // something should stop the thread before...
  if ( _RTXThread != 0 ) { 
    _debug("! ARTP Failure: Thread already exists..., stopping it\n");
    delete _RTXThread; _RTXThread = 0;
    //return -1; 
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


void
AudioRtp::closeRtpSession () {
  ost::MutexLock m(_threadMutex);
  // This will make RTP threads finish.
  // _debug("Stopping AudioRTP\n");
  try {
    delete _RTXThread; _RTXThread = 0;
  } catch(...) {
    _debugException("! ARTP Exception: when stopping audiortp\n");
    throw;
  }
  AudioLayer* audiolayer = Manager::instance().getAudioDriver();
  audiolayer->stopStream();
}

////////////////////////////////////////////////////////////////////////////////
// AudioRtpRTX Class                                                          //
////////////////////////////////////////////////////////////////////////////////
AudioRtpRTX::AudioRtpRTX (SIPCall *sipcall, bool sym) : time(new ost::Time()), _ca(sipcall), _sessionSend(NULL), _sessionRecv(NULL), _session(NULL), _start(), 
		               _sym(sym), micData(NULL), micDataConverted(NULL), micDataEncoded(NULL), spkrDataDecoded(NULL), spkrDataConverted(NULL), 
		               converter(NULL), _layerSampleRate(),_codecSampleRate(), _layerFrameSize(), _audiocodec(NULL)
{
  setCancel(cancelDeferred);
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
    _session = new ost::SymmetricRTPSession (local_ip, _ca->getLocalAudioPort());
    _sessionRecv = NULL;
    _sessionSend = NULL;
  }
}

AudioRtpRTX::~AudioRtpRTX () {
  _start.wait();

  try {
    this->terminate();
  } catch(...) {
    _debugException("! ARTP: Thread destructor didn't terminate correctly");
    throw;
  }
  //_debug("terminate audiortprtx ended...\n");
  _ca = 0;
  if (!_sym) {
    delete _sessionRecv; _sessionRecv = NULL;
    delete _sessionSend; _sessionSend = NULL;
  } else {
    delete _session;     _session = NULL;
  }

  delete [] micData;  micData = NULL;
  delete [] micDataConverted;  micDataConverted = NULL;
  delete [] micDataEncoded;  micDataEncoded = NULL;

  delete [] spkrDataDecoded; spkrDataDecoded = NULL;
  delete [] spkrDataConverted; spkrDataConverted = NULL;

  delete time; time = NULL;
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
    if (_ca == 0) { return; }
    _audiocodec = Manager::instance().getCodecDescriptorMap().getCodec( _ca->getAudioCodec() );
    _codecSampleRate = _audiocodec->getClockRate();	

    _debug("Init audio RTP session\n");
    ost::InetHostAddress remote_ip(_ca->getRemoteIp().c_str());
    if (!remote_ip) {
      _debug("! ARTP Thread Error: Target IP address [%s] is not correct!\n", _ca->getRemoteIp().data());
      return;
    }

    // Initialization
    if (!_sym) {
      _sessionRecv->setSchedulingTimeout (10000);
      _sessionRecv->setExpireTimeout(1000000);

      _sessionSend->setSchedulingTimeout(10000);
      _sessionSend->setExpireTimeout(1000000);
    } else {
      _session->setSchedulingTimeout(10000);
      _session->setExpireTimeout(1000000);
    }

    if (!_sym) {
      if ( !_sessionRecv->addDestination(remote_ip, (unsigned short) _ca->getRemoteAudioPort()) ) {
	_debug("AudioRTP Thread Error: could not connect to port %d\n",  _ca->getRemoteAudioPort());
	return;
      }
      if (!_sessionSend->addDestination (remote_ip, (unsigned short) _ca->getRemoteAudioPort())) {
	_debug("! ARTP Thread Error: could not connect to port %d\n",  _ca->getRemoteAudioPort());
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

      //_debug("AudioRTP Thread: Added session destination %s\n", remote_ip.getHostname() );

      if (!_session->addDestination (remote_ip, (unsigned short) _ca->getRemoteAudioPort())) {
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
  } catch(...) {
    _debugException("! ARTP Failure: initialisation failed");
    throw;
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
  try {

    timestamp += time->getSecond();
    if (_ca==0) { _debug(" !ARTP: No call associated (mic)\n"); return; } // no call, so we do nothing
    AudioLayer* audiolayer = Manager::instance().getAudioDriver();
    if (!audiolayer) { _debug(" !ARTP: No audiolayer available for mic\n"); return; }

    if (!_audiocodec) { _debug(" !ARTP: No audiocodec available for mic\n"); return; }

    // we have to get 20ms of data from the mic *20/1000 = /50
    int maxBytesToGet = _layerSampleRate * _layerFrameSize * sizeof(SFLDataFormat) / 1000;
    // available bytes inside ringbuffer
    int availBytesFromMic = audiolayer->canGetMic();

    // take the lowest
    int bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;
    // Get bytes from micRingBuffer to data_from_mic
    //_debug("get data from mic\n");
    int nbSample = audiolayer->getMic( micData , bytesAvail ) / sizeof(SFLDataFormat);
    int nb_sample_up = nbSample;
    int nbSamplesMax = _layerFrameSize * _audiocodec->getClockRate() / 1000;

    //_debug("resample data\n");
    nbSample = reSampleData(_audiocodec->getClockRate(), nb_sample_up, DOWN_SAMPLING);	

    if ( nbSample < nbSamplesMax - 10 ) { // if only 10 is missing, it's ok
      // fill end with 0...
      memset( micDataConverted + nbSample, 0, (nbSamplesMax-nbSample)*sizeof(int16));
      nbSample = nbSamplesMax;
    }
    int compSize = _audiocodec->codecEncode( micDataEncoded , micDataConverted , nbSample*sizeof(int16));
    // encode divise by two
    // Send encoded audio sample over the network
    if (compSize > nbSamplesMax) { _debug("! ARTP: %d should be %d\n", compSize, nbSamplesMax);}
    if (!_sym) {
      _sessionSend->putData(timestamp, micDataEncoded, compSize);
    } else {
      _session->putData(timestamp, micDataEncoded, compSize);
    }
  } catch(...) {
    _debugException("! ARTP: sending failed");
    throw;
  }
}

  void
AudioRtpRTX::receiveSessionForSpkr (int& countTime)
{


  if (_ca == 0) { return; }
  try {
    AudioLayer* audiolayer = Manager::instance().getAudioDriver();
    if (!audiolayer) { return; }

    const ost::AppDataUnit* adu = NULL;
    // Get audio data stream

    if (!_sym) {
      adu = _sessionRecv->getData(_sessionRecv->getFirstTimestamp());
    } else {
      adu = _session->getData(_session->getFirstTimestamp());
    }
    if (adu == NULL) {
      //_debug("No RTP audio stream\n");
      return;
    }

    //int payload = adu->getType(); // codec type
    unsigned char* spkrData  = (unsigned char*)adu->getData(); // data in char
    unsigned int size = adu->getSize(); // size in char

    // Decode data with relevant codec
    unsigned int max = (unsigned int)(_codecSampleRate * _layerFrameSize / 1000);

    if ( size > max ) {
      _debug("We have received from RTP a packet larger than expected: %d VS %d\n", size, max);
      _debug("The packet size has been cropped\n");
      size=max;
    }

    if (_audiocodec != NULL) {

      int expandedSize = _audiocodec->codecDecode( spkrDataDecoded , spkrData , size );
      //buffer _receiveDataDecoded ----> short int or int16, coded on 2 bytes
      int nbInt16 = expandedSize / sizeof(int16);
      //nbInt16 represents the number of samples we just decoded
      if ((unsigned int)nbInt16 > max) {
	_debug("We have decoded an RTP packet larger than expected: %d VS %d. Cropping.\n", nbInt16, max);
	nbInt16=max;
      }
      int nbSample = nbInt16;

      // Do sample rate conversion
      int nb_sample_down = nbSample;
      nbSample = reSampleData(_codecSampleRate , nb_sample_down, UP_SAMPLING);
#ifdef DATAFORMAT_IS_FLOAT
#else
#endif
      
      audiolayer->playSamples( spkrDataConverted, nbSample * sizeof(SFLDataFormat), true);
      
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
    delete adu; adu = NULL;
  } catch(...) {
    _debugException("! ARTP: receiving failed");
    throw;
  }


}

  int 
AudioRtpRTX::reSampleData(int sampleRate_codec, int nbSamples, int status)
{
  if(status==UP_SAMPLING){
    return converter->upsampleData( spkrDataDecoded , spkrDataConverted , sampleRate_codec , _layerSampleRate , nbSamples );
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

  try {
    // Init the session
    initAudioRtpSession();
    step = (int) (_layerFrameSize * _codecSampleRate / 1000);
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
    int countTime = 0; // for receive
    TimerPort::setTimer(_layerFrameSize);

    audiolayer->startStream();
    _start.post();
    _debug("- ARTP Action: Start\n");
    while (!testCancel()) {
      ////////////////////////////
      // Send session
      ////////////////////////////
      sendSessionFromMic(timestamp);
      timestamp += step;
      ////////////////////////////
      // Recv session
      ////////////////////////////
      receiveSessionForSpkr(countTime);
      // Let's wait for the next transmit cycle
      Thread::sleep(TimerPort::getTimer());
      TimerPort::incTimer(_layerFrameSize); // 'frameSize' ms
    }
    //_debug("stop stream for audiortp loop\n");
    audiolayer->stopStream();
  } catch(std::exception &e) {
    _start.post();
    _debug("! ARTP: Stop %s\n", e.what());
    throw;
  } catch(...) {
    _start.post();
    _debugException("* ARTP Action: Stop");
    throw;
  }
}


// EOF
