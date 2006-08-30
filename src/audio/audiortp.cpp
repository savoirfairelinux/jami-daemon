/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#include "../global.h"
#include "../manager.h"
#include "codecDescriptor.h"
#include "audiortp.h"
#include "audiolayer.h"
#include "ringbuffer.h"
#include "../user_cfg.h"
#include "../sipcall.h"
#include <samplerate.h>

////////////////////////////////////////////////////////////////////////////////
// AudioRtp                                                          
////////////////////////////////////////////////////////////////////////////////
AudioRtp::AudioRtp ()
{
  _RTXThread = 0;
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
}

////////////////////////////////////////////////////////////////////////////////
// AudioRtpRTX Class                                                          //
////////////////////////////////////////////////////////////////////////////////
AudioRtpRTX::AudioRtpRTX (SIPCall *sipcall, bool sym) {
  setCancel(cancelDeferred);
  time = new ost::Time();
  _ca = sipcall;
  _sym = sym;
  // AudioRtpRTX should be close if we change sample rate

  _receiveDataDecoded = new int16[RTP_20S_48KHZ_MAX];
  _sendDataEncoded   =  new unsigned char[RTP_20S_8KHZ_MAX];

  // we estimate that the number of format after a conversion 8000->48000 is expanded to 6 times
  _dataAudioLayer = new SFLDataFormat[RTP_20S_48KHZ_MAX];
  _floatBuffer8000  = new float32[RTP_20S_8KHZ_MAX];
  _floatBuffer48000 = new float32[RTP_20S_48KHZ_MAX];
  _intBuffer8000  = new int16[RTP_20S_8KHZ_MAX];

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
    delete _sessionRecv; _sessionRecv = 0;
    delete _sessionSend; _sessionSend = 0;
  } else {
    delete _session;     _session = 0;
  }

  delete [] _intBuffer8000; _intBuffer8000 = 0;
  delete [] _floatBuffer48000; _floatBuffer48000 = 0;
  delete [] _floatBuffer8000; _floatBuffer8000 = 0;
  delete [] _dataAudioLayer; _dataAudioLayer = 0;

  delete [] _sendDataEncoded; _sendDataEncoded = 0;
  delete [] _receiveDataDecoded; _receiveDataDecoded = 0;


  delete time; time = NULL;
}

void
AudioRtpRTX::initAudioRtpSession (void) 
{
  try {
    if (_ca == 0) { return; }

    //_debug("Init audio RTP session\n");
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

      AudioCodec* audiocodec = _ca->getAudioCodec();
      bool payloadIsSet = false;
      if (audiocodec) {
        if (audiocodec->hasDynamicPayload()) {
          payloadIsSet = _sessionRecv->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) audiocodec->getPayload(), audiocodec->getClockRate()));
        } else {
          payloadIsSet= _sessionRecv->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) audiocodec->getPayload()));
          payloadIsSet = _sessionSend->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) audiocodec->getPayload()));
        }
      }
      _sessionSend->setMark(true);
    } else {

      //_debug("AudioRTP Thread: Added session destination %s:%d\n", remote_ip.getHostname(), (unsigned short) _ca->getRemoteSdpAudioPort());

      if (!_session->addDestination (remote_ip, (unsigned short) _ca->getRemoteAudioPort())) {
        return;
      }

      AudioCodec* audiocodec = _ca->getAudioCodec();
      bool payloadIsSet = false;
      if (audiocodec) {
        if (audiocodec->hasDynamicPayload()) {
          payloadIsSet = _session->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) audiocodec->getPayload(), audiocodec->getClockRate()));
        } else {
          payloadIsSet = _session->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) audiocodec->getPayload()));
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
    if (_ca==0) { return; } // no call, so we do nothing
    AudioLayer* audiolayer = Manager::instance().getAudioDriver();
    if (!audiolayer) { return; }

    AudioCodec* audiocodec = _ca->getAudioCodec();
    if (!audiocodec) { return; }

    // we have to get 20ms of data from the mic *20/1000 = /50
    // rate/50 shall be lower than RTP_20S_48KHZ_MAX
    int maxBytesToGet = audiolayer->getSampleRate()/50*sizeof(SFLDataFormat);

    // available bytes inside ringbuffer
    int availBytesFromMic = audiolayer->canGetMic();

    // take the lower
    int bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;

    // Get bytes from micRingBuffer to data_from_mic
    int nbSample = audiolayer->getMic(_dataAudioLayer, bytesAvail) / sizeof(SFLDataFormat);

    int16* toSIP = 0;
    if (audiolayer->getSampleRate() != audiocodec->getClockRate() && nbSample) {
       SRC_DATA src_data;
       #ifdef DATAFORMAT_IS_FLOAT   
          src_data.data_in = _dataAudioLayer;
       #else
          src_short_to_float_array(_dataAudioLayer, _floatBuffer48000, nbSample);
          src_data.data_in = _floatBuffer48000; 
       #endif
       double factord = (double)audiocodec->getClockRate()/audiolayer->getSampleRate();
       src_data.src_ratio = factord;
       src_data.input_frames = nbSample;
       src_data.output_frames = RTP_20S_8KHZ_MAX;
       src_data.data_out = _floatBuffer8000;
       src_simple (&src_data, SRC_SINC_BEST_QUALITY/*SRC_SINC_MEDIUM_QUALITY*/, 1); // 1 = channel
       nbSample = src_data.output_frames_gen;
       //if (nbSample > RTP_20S_8KHZ_MAX) { _debug("Alert from mic, nbSample %d is bigger than expected %d\n", nbSample, RTP_20S_8KHZ_MAX); }
       src_float_to_short_array (_floatBuffer8000, _intBuffer8000, nbSample);
       toSIP = _intBuffer8000;
    } else {
      #ifdef DATAFORMAT_IS_FLOAT
        // convert _receiveDataDecoded to float inside _receiveData
        src_float_to_short_array(_dataAudioLayer, _intBuffer8000, nbSample);
        toSIP = _intBuffer8000;
       //if (nbSample > RTP_20S_8KHZ_MAX) { _debug("Alert from mic, nbSample %d is bigger than expected %d\n", nbSample, RTP_20S_8KHZ_MAX); }
      #else
        toSIP = _dataAudioLayer; // int to int
      #endif
    }

    if ( nbSample < RTP_20S_8KHZ_MAX ) {
      // fill end with 0...
      //_debug("begin: %p, nbSample: %d\n", toSIP, nbSample);
      //_debug("has to fill: %d chars at %p\n", (RTP_20S_8KHZ_MAX-nbSample)*sizeof(int16), toSIP + nbSample);
      memset(toSIP + nbSample, 0, (RTP_20S_8KHZ_MAX-nbSample)*sizeof(int16));
      //nbSample = RTP_20S_8KHZ_MAX;
    }

    // for the mono: range = 0 to RTP_FRAME2SEND * sizeof(int16)
    // codecEncode(char *dest, int16* src, size in bytes of the src)
    int compSize = audiocodec->codecEncode(_sendDataEncoded, toSIP, nbSample*sizeof(int16));

    // encode divise by two
    // Send encoded audio sample over the network
    if (compSize > RTP_20S_8KHZ_MAX) { _debug("! ARTP: %d should be %d\n", compSize, RTP_20S_8KHZ_MAX);}
    if (!_sym) {
      _sessionSend->putData(timestamp, _sendDataEncoded, compSize);
    } else {
      _session->putData(timestamp, _sendDataEncoded, compSize);
    }
    toSIP = 0;
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
      return;
    }

    int payload = adu->getType(); // codec type
    unsigned char* data  = (unsigned char*)adu->getData(); // data in char
    unsigned int size    = adu->getSize(); // size in char

    if ( size > RTP_20S_8KHZ_MAX ) {
      _debug("We have received from RTP a packet larger than expected: %s VS %s\n", size, RTP_20S_8KHZ_MAX);
      _debug("The packet size has been cropped\n");
      size=RTP_20S_8KHZ_MAX;
    }

    // Decode data with relevant codec
    AudioCodec* audiocodec = _ca->getCodecMap().getCodec((CodecType)payload);
    if (audiocodec != 0) {
      // codecDecode(int16 *dest, char* src, size in bytes of the src)
      // decode multiply by two, so the number of byte should be double
      // size shall be RTP_FRAME2SEND or lower
      int expandedSize = audiocodec->codecDecode(_receiveDataDecoded, data, size);
      int nbInt16      = expandedSize/sizeof(int16);
      if (nbInt16 > RTP_20S_8KHZ_MAX) {
        _debug("We have decoded a RTP packet larger than expected: %s VS %s. crop\n", nbInt16, RTP_20S_8KHZ_MAX);
        nbInt16=RTP_20S_8KHZ_MAX;
      }

      SFLDataFormat* toAudioLayer;
      int nbSample = nbInt16;
      int nbSampleMaxRate = nbInt16 * 6; // TODO: change it

      if ( audiolayer->getSampleRate() != audiocodec->getClockRate() && nbSample) {
        // convert here
        double         factord = (double)audiolayer->getSampleRate()/audiocodec->getClockRate();

        SRC_DATA src_data;
        src_data.data_in = _floatBuffer8000;
        src_data.data_out = _floatBuffer48000;
        src_data.input_frames = nbSample;
        src_data.output_frames = nbSampleMaxRate;
        src_data.src_ratio = factord;
        src_short_to_float_array(_receiveDataDecoded, _floatBuffer8000, nbSample);
        src_simple (&src_data, SRC_SINC_BEST_QUALITY/*SRC_SINC_MEDIUM_QUALITY*/, 1); // 1=mono channel
       
        nbSample = ( src_data.output_frames_gen > RTP_20S_48KHZ_MAX) ? RTP_20S_48KHZ_MAX : src_data.output_frames_gen;
        #ifdef DATAFORMAT_IS_FLOAT
          toAudioLayer = _floatBuffer48000;
	#else
          src_float_to_short_array(_floatBuffer48000, _dataAudioLayer, nbSample);
	  toAudioLayer = _dataAudioLayer;
	#endif
	
      } else {
        nbSample = nbInt16;
        #ifdef DATAFORMAT_IS_FLOAT
      	  // convert _receiveDataDecoded to float inside _receiveData
          src_short_to_float_array(_receiveDataDecoded, _floatBuffer8000, nbSample);
	  toAudioLayer = _floatBuffer8000;
        #else
	  toAudioLayer = _receiveDataDecoded; // int to int
        #endif
      }
      audiolayer->putMain(toAudioLayer, nbSample * sizeof(SFLDataFormat));
      //_debug("ARTP: %d\n", nbSample * sizeof(SFLDataFormat));

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

void
AudioRtpRTX::run () {
  //mic, we receive from soundcard in stereo, and we send encoded
  //encoding before sending
  AudioLayer *audiolayer = Manager::instance().getAudioDriver();

  try {
    // Init the session
    initAudioRtpSession();

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
    // TODO: get frameSize from user config 
    int frameSize = 20; // 20ms frames
    TimerPort::setTimer(frameSize);

    audiolayer->flushMic();
    audiolayer->startStream();
    _start.post();
    _debug("- ARTP Action: Start\n");
    while (!testCancel()) {
      ////////////////////////////
      // Send session
      ////////////////////////////
      sendSessionFromMic(timestamp);
      timestamp += RTP_20S_8KHZ_MAX;

      ////////////////////////////
      // Recv session
      ////////////////////////////
      receiveSessionForSpkr(countTime);

      // Let's wait for the next transmit cycle
      Thread::sleep(TimerPort::getTimer());
      TimerPort::incTimer(frameSize); // 'frameSize' ms
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
