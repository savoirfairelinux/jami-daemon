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

#include "../global.h"
#include "../manager.h"
#include "codecDescriptor.h"
#include "audiortp.h"
#include "audiolayer.h"
#include "ringbuffer.h"
#include "../user_cfg.h"
#include "../sipcall.h"
#ifdef USE_SAMPLERATE
 #include <samplerate.h>
#endif

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
  _RTXThread = new AudioRtpRTX (ca, Manager::instance().getAudioDriver(), _symmetric);

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
AudioRtpRTX::AudioRtpRTX (SIPCall *sipcall, AudioLayer* driver, bool sym) {
  setCancel(cancelDeferred);
  time = new ost::Time();
  _ca = sipcall;
  _sym = sym;
  _audioDevice = driver;
  if (_audioDevice!=0) {
//    _nbFrames = 20 * _audioDevice->getSampleRate()/1000; // 20 ms
    _nbFrames = _audioDevice->getSampleRate()/50; // 20 ms / 1000
  } else {
    _nbFrames = RTP_FRAMES2SEND;
  }
#ifdef USE_SAMPLERATE
  _floatBufferIn  = new float[_nbFrames*2];
  _floatBufferOut = new float[_nbFrames*2];
#endif

  // TODO: Change bind address according to user settings.
  // TODO: this should be the local ip not the external (router) IP
  std::string localipConfig = _ca->getLocalIp(); // _ca->getLocalIp();
  ost::InetHostAddress local_ip(localipConfig.c_str());

  //_debug("AudioRtpRTX ctor : Local IP:port %s:%d\tsymmetric:%d\n", local_ip.getHostname(), _ca->getLocalAudioPort(), _sym);

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
  _ca = NULL;

  if (!_sym) {
    delete _sessionRecv; _sessionRecv = NULL;
    delete _sessionSend; _sessionSend = NULL;
  } else {
    delete _session;     _session = NULL;
  }

  delete time; time = NULL;


#ifdef USE_SAMPLERATE
  delete [] _floatBufferIn;  _floatBufferIn = 0;
  delete [] _floatBufferOut; _floatBufferOut = 0;
#endif
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
AudioRtpRTX::sendSessionFromMic (unsigned char* data_to_send, int16* data_from_mic, int16* data_from_mic_to_codec, int timestamp)
{
  try {
    if (_ca==0) { return; } // no call, so we do nothing
    short sizeOfData = sizeof(int16);
    AudioLayer* audiolayer = Manager::instance().getAudioDriver();

    int availBytesFromMic = audiolayer->canGetMic();
    int fromChannel = audiolayer->getInChannel();
    int toChannel   = fromChannel;

    AudioCodec* audiocodec = _ca->getAudioCodec();
    if (audiocodec!=0) {
      toChannel = audiocodec->getChannel();
    }

    int maxBytesToGet = _nbFrames * fromChannel * sizeOfData; // * channels * int16/byte

    // take the lower
    int bytesAvail;
    if (availBytesFromMic < maxBytesToGet) {
      bytesAvail = availBytesFromMic;
    } else {
      bytesAvail = maxBytesToGet;
    }

    // Get bytes from micRingBuffer to data_from_mic
    audiolayer->getMic(data_from_mic, bytesAvail);

    int nbInt16 = bytesAvail/sizeof(int16);

#ifdef USE_SAMPLERATE
    // sample rate conversion here
    if (audiocodec && audiolayer->getSampleRate() != audiocodec->getClockRate()) {
      double         factord = (double)audiocodec->getClockRate()/audiolayer->getSampleRate();

      SRC_DATA src_data;
      src_data.data_in = _floatBufferIn;
      src_data.data_out = _floatBufferOut;
      src_data.input_frames = nbInt16/fromChannel;
      src_data.output_frames = _nbFrames;
      src_data.src_ratio = factord;
      src_short_to_float_array(data_from_mic, _floatBufferIn, nbInt16);
      src_simple (&src_data, SRC_SINC_MEDIUM_QUALITY, fromChannel);
      src_float_to_short_array (_floatBufferOut, data_from_mic, src_data.output_frames_gen*fromChannel);

      nbInt16 = src_data.output_frames_gen * fromChannel;
    }
#endif

    unsigned int toSize = audiolayer->convert(data_from_mic, fromChannel, nbInt16, &data_from_mic_to_codec, toChannel);

    if ( toSize != (RTP_FRAMES2SEND * toChannel) ) {
      // fill end with 0...
      bzero(data_from_mic_to_codec + toSize*sizeOfData, (RTP_FRAMES2SEND*toChannel-toSize)*sizeOfData);
      toSize = RTP_FRAMES2SEND * toChannel;
    }

    // Encode acquired audio sample
    if ( audiocodec != NULL ) {
      // for the mono: range = 0 to RTP_FRAME2SEND * sizeof(int16)
      // codecEncode(char *dest, int16* src, size in bytes of the src)
      int compSize = audiocodec->codecEncode(data_to_send, data_from_mic_to_codec, toSize*sizeOfData);
      // encode divise by two
      // Send encoded audio sample over the network
      if (compSize > RTP_FRAMES2SEND) { _debug("! ARTP: %d should be %d\n", compSize, _nbFrames);}
      //fprintf(stderr, "S");
      if (!_sym) {
        _sessionSend->putData(timestamp, data_to_send, compSize);
      } else {
        _session->putData(timestamp, data_to_send, compSize);
      }
    }
  } catch(...) {
    _debugException("! ARTP: sending failed");
    throw;
  }
}

void
AudioRtpRTX::receiveSessionForSpkr (int16* data_for_speakers_stereo, int16* data_for_speakers_recv, int& countTime)
{
  if (_ca == 0) { return; }
  try {
    AudioLayer* audiolayer = Manager::instance().getAudioDriver();
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

    int payload = adu->getType();
    unsigned char* data  = (unsigned char*)adu->getData();
    unsigned int size    = adu->getSize();

    // Decode data with relevant codec
    int toPut = 0;
    AudioCodec* audiocodec = _ca->getCodecMap().getCodec((CodecType)payload);
    if (audiocodec != 0) {
      // codecDecode(int16 *dest, char* src, size in bytes of the src)
      // decode multiply by two
      // size shall be RTP_FRAME2SEND or lower
      int expandedSize = audiocodec->codecDecode(data_for_speakers_recv, data, size);
      int nbInt16      = expandedSize/sizeof(int16);

      int toChannel = audiolayer->getOutChannel();
      int fromChannel = audiocodec->getChannel();

#ifdef USE_SAMPLERATE
      if ( audiolayer->getSampleRate() != audiocodec->getClockRate() ) {
        // convert here
        double         factord = (double)audiolayer->getSampleRate()/ audiocodec->getClockRate();

        SRC_DATA src_data;
        src_data.data_in = _floatBufferIn;
        src_data.data_out = _floatBufferOut;
        src_data.input_frames = nbInt16/fromChannel;
        src_data.output_frames = _nbFrames;
        src_data.src_ratio = factord;
        src_short_to_float_array(data_for_speakers_recv, _floatBufferIn, nbInt16);
        src_simple (&src_data, SRC_SINC_MEDIUM_QUALITY, fromChannel);
        src_float_to_short_array(_floatBufferOut, data_for_speakers_recv, src_data.output_frames_gen*fromChannel);
        nbInt16 = src_data.output_frames_gen*fromChannel;
      }
#endif

      toPut = audiolayer->convert(data_for_speakers_recv, fromChannel, nbInt16, &data_for_speakers_stereo, toChannel);

      // If the current call is the call which is answered
      // Set decoded data to sound device
      // expandedSize is in mono/bytes, since we double in stereo, we send two time more
      //fprintf(stderr, "R");
      audiolayer->putMain(data_for_speakers_stereo, toPut * sizeof(int16));
      //Manager::instance().getAudioDriver()->startStream();
  
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

  int16 *data_from_mic = new int16[_nbFrames*audiolayer->getInChannel()];
  int16 *data_from_mic_to_codec = new int16[_nbFrames*2]; // 2 = max channel
  unsigned char *char_to_send = new unsigned char[_nbFrames]; // two time more for codec

  //spkr, we receive from rtp in mono and we send in stereo
  //decoding after receiving
  // we could receive in stereo too...
  int16 *data_for_speakers_recv = new int16[_nbFrames*2];
  int16 *data_for_speakers_stereo = new int16[_nbFrames*2];

  try {
    //_debug("AudioRTP Thread is running\n");

    // Init the session
    initAudioRtpSession();

    // flush stream:

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
      sendSessionFromMic(char_to_send, data_from_mic, data_from_mic_to_codec, timestamp);
      timestamp += RTP_FRAMES2SEND;

      ////////////////////////////
      // Recv session
      ////////////////////////////
      receiveSessionForSpkr(data_for_speakers_stereo, data_for_speakers_recv, countTime);

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
  delete [] data_for_speakers_stereo; data_for_speakers_stereo = 0;
  delete [] data_for_speakers_recv;   data_for_speakers_recv   = 0;
  delete [] char_to_send;          char_to_send          = 0;
  delete [] data_from_mic_to_codec;   data_from_mic_to_codec   = 0;
  delete [] data_from_mic;  data_from_mic = 0;
}


// EOF
