/*
 *
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
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
AudioRtpRTX::AudioRtpRTX (SIPCall *sipcall, bool sym)
	 : _fstream("/tmp/audio.dat", std::ofstream::binary|std::ios::out|std::ios::app)
{
	setCancel(cancelDeferred);
	time = new ost::Time();
	_ca = sipcall;
	_sym = sym;
	//std::string s = "snd.dat";
	// AudioRtpRTX should be close if we change sample rate

	//_codecSampleRate = _ca->getAudioCodec()->getClockRate();

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

	// libsamplerate-related
	// Set the converter type for the upsampling and the downsampling
	// interpolator SRC_SINC_BEST_QUALITY
	_src_state_mic  = src_new(SRC_SINC_BEST_QUALITY, 1, &_src_err);
	_src_state_spkr = src_new(SRC_SINC_BEST_QUALITY, 1, &_src_err);

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
	//fd = fopen("snd_data", "wa");
	if (!_sym) {
		delete _sessionRecv; _sessionRecv = NULL;
		delete _sessionSend; _sessionSend = NULL;
	} else {
		delete _session;     _session = NULL;
	}

	delete [] _intBufferDown; _intBufferDown = NULL;
	delete [] _floatBufferUp; _floatBufferUp = NULL;
	delete [] _floatBufferDown; _floatBufferDown = NULL;
	delete [] _dataAudioLayer; _dataAudioLayer = NULL;

	delete [] _sendDataEncoded; _sendDataEncoded = NULL;
	delete [] _receiveDataDecoded; _receiveDataDecoded = NULL;

	delete time; time = NULL;

	// libsamplerate-related
	_src_state_mic  = src_delete(_src_state_mic);
	_src_state_spkr = src_delete(_src_state_spkr);
}

	void
AudioRtpRTX::initBuffers()
{
	int nbSamplesMax = (int) (_layerSampleRate * _layerFrameSize /1000);
	_dataAudioLayer = new SFLDataFormat[nbSamplesMax];
	_receiveDataDecoded = new int16[nbSamplesMax];
	_floatBufferDown  = new float32[nbSamplesMax];
	_floatBufferUp = new float32[nbSamplesMax];
	_sendDataEncoded = new unsigned char[nbSamplesMax];
	_intBufferDown = new int16[nbSamplesMax];
}

	void
AudioRtpRTX::initAudioRtpSession (void) 
{



	try {
		if (_ca == 0) { return; }
		AudioCodec* audiocodec = loadCodec(_ca->getAudioCodec());
		_codecSampleRate = audiocodec->getClockRate();	
	
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

			//_debug("AudioRTP Thread: Added session destination %s\n", remote_ip.getHostname() );

			if (!_session->addDestination (remote_ip, (unsigned short) _ca->getRemoteAudioPort())) {
				return;
			}

			bool payloadIsSet = false;
			if (audiocodec) {
				if (audiocodec->hasDynamicPayload()) {
					payloadIsSet = _session->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) audiocodec->getPayload(), audiocodec->getClockRate()));
				} else {
					payloadIsSet = _session->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) audiocodec->getPayload()));
				}
			}
		}
		unloadCodec(audiocodec);
	} catch(...) {
		_debugException("! ARTP Failure: initialisation failed");
		throw;
	}
}

AudioCodec*
AudioRtpRTX::loadCodec(int payload)
{
        using std::cerr;

	switch(payload){
	  case 0:
            handle_codec = dlopen( CODECS_DIR "/libcodec_ulaw.so", RTLD_LAZY);
	    break;
	  case 3:
	    handle_codec = dlopen(CODECS_DIR "/libcodec_gsm.so", RTLD_LAZY);
	    break;
	  case 8:
	    handle_codec = dlopen(CODECS_DIR "/libcodec_alaw.so", RTLD_LAZY);
	    break;
	  case 97:
            handle_codec = dlopen(CODECS_DIR "/libcodec_ilbc.so", RTLD_LAZY);
	    break;
	  case 110:
	    handle_codec = dlopen(CODECS_DIR "/libcodec_speex.so", RTLD_LAZY);
	    break;
	}

        if(!handle_codec){
                cerr<<"cannot load library: "<< dlerror() <<'\n';
        }
        dlerror();
        create_t* create_codec = (create_t*)dlsym(handle_codec, "create");
        const char* dlsym_error = dlerror();
        if(dlsym_error){
                cerr << "Cannot load symbol create: " << dlsym_error << '\n';
        }
        return create_codec();
}

void
AudioRtpRTX::unloadCodec(AudioCodec* audiocodec)
{
	using std::cerr;
        destroy_t* destroy_codec = (destroy_t*)dlsym(handle_codec, "destroy");
	const char* dlsym_error = dlerror();
	if(dlsym_error){
                cerr << "Cannot load symbol destroy" << dlsym_error << '\n';
        }
	destroy_codec(audiocodec);
	dlclose(handle_codec);
}

	
void
AudioRtpRTX::sendSessionFromMic(int timestamp)
{
	AudioCodec* audiocodec = loadCodec(_ca->getAudioCodec());
// STEP:
//   1. get data from mic
//   2. convert it to int16 - good sample, good rate
//   3. encode it
//   4. send it
try {
	int16* toSIP = NULL;

	timestamp += time->getSecond();
	if (_ca==0) { _debug(" !ARTP: No call associated (mic)\n"); return; } // no call, so we do nothing
	AudioLayer* audiolayer = Manager::instance().getAudioDriver();
	if (!audiolayer) { _debug(" !ARTP: No audiolayer available for mic\n"); return; }

	//AudioCodec* audiocodec = _ca->getAudioCodec();
	if (!audiocodec) { _debug(" !ARTP: No audiocodec available for mic\n"); return; }

	// we have to get 20ms of data from the mic *20/1000 = /50
	int maxBytesToGet = _layerSampleRate * _layerFrameSize * sizeof(SFLDataFormat) / 1000;

	// available bytes inside ringbuffer
	int availBytesFromMic = audiolayer->canGetMic();

	// take the lowest
	int bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;
        //printf("clock rate = %i\n", audiocodec->getClockRate());
	// Get bytes from micRingBuffer to data_from_mic
	int nbSample = audiolayer->getMic(_dataAudioLayer, bytesAvail) / sizeof(SFLDataFormat);
	int nb_sample_up = nbSample;
	int nbSamplesMax = _layerFrameSize * audiocodec->getClockRate() / 1000;
	//_fstream.write((char*) _dataAudioLayer, nbSample);
	
	nbSample = reSampleData(audiocodec->getClockRate(), nb_sample_up, DOWN_SAMPLING);	
	
	toSIP = _intBufferDown;
	
	if ( nbSample < nbSamplesMax - 10 ) { // if only 10 is missing, it's ok
		// fill end with 0...
		//_debug("begin: %p, nbSample: %d\n", toSIP, nbSample);
		memset(toSIP + nbSample, 0, (nbSamplesMax-nbSample)*sizeof(int16));
		nbSample = nbSamplesMax;
	}
	// debug - dump sound in a file
//_debug("AR: Nb sample: %d int, [0]=%d [1]=%d [2]=%d\n", nbSample, toSIP[0], toSIP[1], toSIP[2]);
	// for the mono: range = 0 to RTP_FRAME2SEND * sizeof(int16)
	// codecEncode(char *dest, int16* src, size in bytes of the src)
	int compSize = audiocodec->codecEncode(_sendDataEncoded, toSIP, nbSample*sizeof(int16));
	//printf("jusqu'ici tout vas bien\n");

	// encode divise by two
	// Send encoded audio sample over the network
	if (compSize > nbSamplesMax) { _debug("! ARTP: %d should be %d\n", compSize, nbSamplesMax);}
	if (!_sym) {
		_sessionSend->putData(timestamp, _sendDataEncoded, compSize);
	} else {
		_session->putData(timestamp, _sendDataEncoded, compSize);
	}
	toSIP = NULL;
} catch(...) {
	_debugException("! ARTP: sending failed");
	throw;
}
	unloadCodec(audiocodec);
}



void
AudioRtpRTX::receiveSessionForSpkr (int& countTime)
{

AudioCodec* audiocodec;

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

	int payload = adu->getType(); // codec type
	unsigned char* data  = (unsigned char*)adu->getData(); // data in char
	unsigned int size = adu->getSize(); // size in char
	
	//_fstream.write((char*) data, size);
	audiocodec = loadCodec(payload);
	// Decode data with relevant codec
	_codecSampleRate = audiocodec->getClockRate();
	int max = (int)(_codecSampleRate * _layerFrameSize);

	if ( size > max ) {
		_debug("We have received from RTP a packet larger than expected: %s VS %s\n", size, max);
		_debug("The packet size has been cropped\n");
		size=max;
	}
	
	//printf("size = %i\n", size);

	if (audiocodec != NULL) {
		int expandedSize = audiocodec->codecDecode(_receiveDataDecoded, data, size);
		//buffer _receiveDataDecoded ----> short int or int16, coded on 2 bytes
		int nbInt16 = expandedSize / sizeof(int16);
		//nbInt16 represents the number of samples we just decoded
		if (nbInt16 > max) {
			_debug("We have decoded an RTP packet larger than expected: %s VS %s. Cropping.\n", nbInt16, max);
			nbInt16=max;
		}
	
		SFLDataFormat* toAudioLayer;
		int nbSample = nbInt16;

		// Do sample rate conversion
		int nb_sample_down = nbSample;
		nbSample = reSampleData(_codecSampleRate , nb_sample_down, UP_SAMPLING);
#ifdef DATAFORMAT_IS_FLOAT
		toAudioLayer = _floatBufferUp;
#else
		toAudioLayer = _dataAudioLayer;
#endif


		audiolayer->putMain(toAudioLayer, nbSample * sizeof(SFLDataFormat));

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

  unloadCodec(audiocodec);

}

int 
AudioRtpRTX::reSampleData(int sampleRate_codec, int nbSamples, int status)
{
if(status==UP_SAMPLING)
	return upSampleData(sampleRate_codec, nbSamples);
else if(status==DOWN_SAMPLING)
	return downSampleData(sampleRate_codec, nbSamples);
else
	return 0;
}

////////////////////////////////////////////////////////////////////
//////////// RESAMPLING FUNCTIONS /////////////////////////////////
//////////////////////////////////////////////////////////////////

int
AudioRtpRTX::upSampleData(int sampleRate_codec, int nbSamples)
{
double upsampleFactor = (double) _layerSampleRate / sampleRate_codec;
int nbSamplesMax = (int) (_layerSampleRate * _layerFrameSize /1000);
if( upsampleFactor != 1 )
{
	SRC_DATA src_data;
	src_data.data_in = _floatBufferDown;
	src_data.data_out = _floatBufferUp;
	src_data.input_frames = nbSamples;
	src_data.output_frames = (int) floor(upsampleFactor * nbSamples);
	src_data.src_ratio = upsampleFactor;
	src_data.end_of_input = 0; // More data will come
	src_short_to_float_array(_receiveDataDecoded, _floatBufferDown, nbSamples);
	src_process(_src_state_spkr, &src_data);
	nbSamples  = ( src_data.output_frames_gen > nbSamplesMax) ? nbSamplesMax : src_data.output_frames_gen;		
	src_float_to_short_array(_floatBufferUp, _dataAudioLayer, nbSamples);
}

return nbSamples;
}	

int
AudioRtpRTX::downSampleData(int sampleRate_codec, int nbSamples)
{
double downsampleFactor = (double) sampleRate_codec / _layerSampleRate;
int nbSamplesMax = (int) (sampleRate_codec * _layerFrameSize / 1000);
if ( downsampleFactor != 1)
{
	SRC_DATA src_data;	
	src_data.data_in = _floatBufferUp;
	src_data.data_out = _floatBufferDown;
	src_data.input_frames = nbSamples;
	src_data.output_frames = (int) floor(downsampleFactor * nbSamples);
	src_data.src_ratio = downsampleFactor;
	src_data.end_of_input = 0; // More data will come
	src_short_to_float_array(_dataAudioLayer, _floatBufferUp, nbSamples);
	src_process(_src_state_mic, &src_data);
	nbSamples  = ( src_data.output_frames_gen > nbSamplesMax) ? nbSamplesMax : src_data.output_frames_gen;
	src_float_to_short_array(_floatBufferDown, _intBufferDown, nbSamples);
}
return nbSamples;

}

//////////////////////// END RESAMPLING //////////////////////////////////////////////////////

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

	audiolayer->flushMic();
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
	//_fstream.close();
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
