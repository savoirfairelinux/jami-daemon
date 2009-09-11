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


int AudioRtpRTX::count_rtp = 0;

////////////////////////////////////////////////////////////////////////////////
// AudioRtp
////////////////////////////////////////////////////////////////////////////////
AudioRtp::AudioRtp() :_RTXThread (0), _symmetric(), _rtpMutex()
{

}

AudioRtp::~AudioRtp (void)
{
    ost::MutexLock m (_rtpMutex);

    if (_RTXThread != _RTXThread)
    {
        delete _RTXThread;
        _RTXThread = 0;
    }

}

void
AudioRtp::createNewSession (SIPCall *ca)
{

    ost::MutexLock m (_rtpMutex);

    _debug ("AudioRtp::Create new rtp session\n");

    // something should stop the thread before...

    if (_RTXThread != 0) {
        _debug ("**********************************************************\n");
        _debug ("! ARTP Failure: Thread already exists..., stopping it\n");
        _debug ("**********************************************************\n");
        delete _RTXThread;
        _RTXThread = 0;
    }

    // Start RTP Send/Receive threads
    _symmetric = Manager::instance().getConfigInt (SIGNALISATION,SYMMETRIC) ? true : false;

    _RTXThread = new AudioRtpRTX (ca, _symmetric);

}

int
AudioRtp::start (void)
{
    ost::MutexLock m (_rtpMutex);

    if (_RTXThread == 0) {
        _debug ("! ARTP Failure: Cannot start audiortp thread since not yet created\n");
        throw AudioRtpException();
    }


    try {
        if (_RTXThread->start() != 0) {
            _debug ("! ARTP Failure: unable to start RTX Thread\n");
            return -1;
        }
    } catch (...) {
        _debugException ("! ARTP Failure: when trying to start a thread");
        throw;
    }

    return 0;
}


bool
AudioRtp::closeRtpSession ()
{

    ost::MutexLock m (_rtpMutex);
    // This will make RTP threads finish.
    _debug ("AudioRtp::Stopping rtp session\n");

    try {
	if (_RTXThread != 0) {
            delete _RTXThread;
            _RTXThread = 0;
	}
    } catch (...) {
        _debugException ("! ARTP Exception: when stopping audiortp\n");
        throw;
    }

    _debug ("AudioRtp::Audio rtp stopped\n");

    return true;
}

void
AudioRtp::setRecording()
{

    _debug ("AudioRtp::setRecording\n");
    _RTXThread->_ca->setRecording();

}

////////////////////////////////////////////////////////////////////////////////
// AudioRtpRTX Class                                                          //
////////////////////////////////////////////////////////////////////////////////
AudioRtpRTX::AudioRtpRTX (SIPCall *sipcall, bool sym) : time (new ost::Time()), _ca (sipcall), _sessionSend (NULL), _sessionRecv (NULL), _session (NULL),
        _sym (sym), micData (NULL), micDataConverted (NULL), micDataEncoded (NULL), spkrDataDecoded (NULL), spkrDataConverted (NULL),
        converter (NULL), _layerSampleRate(),_codecSampleRate(), _layerFrameSize(), _audiocodec (NULL)
{

    setCancel (cancelDefault);
    // AudioRtpRTX should be close if we change sample rate
    // TODO: Change bind address according to user settings.
    // TODO: this should be the local ip not the external (router) IP
    std::string localipConfig = _ca->getLocalIp(); // _ca->getLocalIp();
    ost::InetHostAddress local_ip (localipConfig.c_str());

    _debug ("%i\n", _ca->getLocalAudioPort());
    _session = new ost::SymmetricRTPSession (local_ip, _ca->getLocalAudioPort());
    // _session = new ost::RTPSessionBase(local_ip, _ca->getLocalAudioPort());
    _sessionRecv = NULL;
    _sessionSend = NULL;

    //mic, we receive from soundcard in stereo, and we send encoded
    //encoding before sending
    _audiolayer = Manager::instance().getAudioDriver();
    _layerFrameSize = _audiolayer->getFrameSize(); // in ms
    _layerSampleRate = _audiolayer->getSampleRate();

    // initBuffers();

    // initAudioRtpSession();

    _payloadIsSet = false;
    _remoteIpIsSet = false;


    count_rtp++;
    // open files
    std::string s_input;
    std::string s_output;

    // convert count into string
    std::stringstream out;
    out << count_rtp;
    
    s_input = "/home/alexandresavard/Desktop/buffer_record/rtp_input_";
    s_input.append(out.str());

    s_output = "/home/alexandresavard/Desktop/buffer_record/rtp_output_";
    s_output.append(out.str());

    rtp_input_rec = new std::fstream();
    rtp_output_rec = new std::fstream();

    rtp_input_rec->open(s_input.c_str(), std::fstream::out);
    rtp_output_rec->open(s_output.c_str(), std::fstream::out);

}

AudioRtpRTX::~AudioRtpRTX ()
{

    ost::MutexLock m (_rtpRtxMutex);

    _debug ("Delete AudioRtpRTX instance in callid %s\n", _ca->getCallId().c_str());

    try {
        this->terminate();
    } catch (...) {
        _debugException ("! ARTP: Thread destructor didn't terminate correctly");
        throw;
    }

    _debug("Remove audio stream for call id %s\n", _ca->getCallId().c_str());
    _audiolayer->getMainBuffer()->unBindAll(_ca->getCallId());
    // Manager::instance().removeStream(_ca->getCallId());

    _debug("DELETE print micData address %p\n", micData);
    delete [] micData;
    micData = NULL;
    delete [] micDataConverted;
    micDataConverted = NULL;
    delete [] micDataEncoded;
    micDataEncoded = NULL;

    delete [] spkrDataDecoded;
    spkrDataDecoded = NULL;
    delete [] spkrDataConverted;
    spkrDataConverted = NULL;

    delete time;
    time = NULL;

    delete converter;
    converter = NULL;

    _ca = 0;
    // _session->terminate();

    delete _session;
    _session = NULL;

    _debug ("AudioRtpRTX instance deleted\n");

    rtp_input_rec->close();
    rtp_output_rec->close();

}


void
AudioRtpRTX::initBuffers()
{
    ost::MutexLock m (_rtpRtxMutex);
    
    _debug("AudioRtpRTX::initBuffers Init RTP buffers for %s\n", _ca->getCallId().c_str());
    
    converter = new SamplerateConverter (_layerSampleRate , _layerFrameSize);

    nbSamplesMax = (int) (_layerSampleRate * _layerFrameSize /1000);

    
    _debug("AudioRtpRTX::initBuffers NBSAMPLEMAX %i\n", nbSamplesMax);

    micData = new SFLDataFormat[nbSamplesMax];
    _debug("CREATE print micData address %p\n", micData);
    micDataConverted = new SFLDataFormat[nbSamplesMax];
    micDataEncoded = new unsigned char[nbSamplesMax];

    spkrDataConverted = new SFLDataFormat[nbSamplesMax];
    spkrDataDecoded = new SFLDataFormat[nbSamplesMax];

    for(int i = 0; i < nbSamplesMax; i++)
    {
	micData = new SFLDataFormat[nbSamplesMax];
	_debug("CREATE print micData address %p\n", micData);
	micDataConverted = new SFLDataFormat[nbSamplesMax];
	micDataEncoded = new unsigned char[nbSamplesMax];

	spkrDataConverted = new SFLDataFormat[nbSamplesMax];
	spkrDataDecoded = new SFLDataFormat[nbSamplesMax];
    }

    Manager::instance().addStream(_ca->getCallId());
    // _audiolayer->getMainBuffer()->bindCallID(_ca->getCallId());
}


void
AudioRtpRTX::initAudioRtpSession (void)
{

    try {

        _session->setSchedulingTimeout (100000);
        _session->setExpireTimeout (1000000);


    } catch (...) {
        _debugException ("! ARTP Failure: initialisation failed");
        throw;
    }

}

void
AudioRtpRTX::setRtpSessionMedia (void)
{

    if (_ca == 0) {
        _debug (" !ARTP: No call, can't init RTP media\n");
        return;
    }

    AudioCodecType pl = (AudioCodecType)_ca->getLocalSDP()->get_session_media()->getPayload();
    _audiocodec = Manager::instance().getCodecDescriptorMap().instantiateCodec(pl);

    if (_audiocodec == NULL) {
        _debug (" !ARTP: No audiocodec, can't init RTP media\n");
        return;
    }

    _debug ("Init audio RTP session: codec payload %i\n", _audiocodec->getPayload());

    // _audioCodecInstance = *_audiocodec;

    _codecSampleRate = _audiocodec->getClockRate();

    _codecFrameSize = _audiocodec->getFrameSize();

    if (_audiocodec->getPayload() == 9) {
        _payloadIsSet = _session->setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) _audiocodec->getPayload(), _audiocodec->getClockRate()));
    } else if (_audiocodec->hasDynamicPayload()) {
        _payloadIsSet = _session->setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) _audiocodec->getPayload(), _audiocodec->getClockRate()));
    } else if (!_audiocodec->hasDynamicPayload() && _audiocodec->getPayload() != 9) {
        _payloadIsSet = _session->setPayloadFormat (ost::StaticPayloadFormat ( (ost::StaticPayloadType) _audiocodec->getPayload()));
    }



}

void
AudioRtpRTX::setRtpSessionRemoteIp (void)
{

    if (!_remoteIpIsSet) {

        _debug ("++++++++++++++++++++++++++ SET IP ADDRESS ++++++++++++++++++++++++++++\n");

        if (_ca == 0) {
            _debug (" !ARTP: No call, can't init RTP media \n");
            return;
        }

        ost::InetHostAddress remote_ip (_ca->getLocalSDP()->get_remote_ip().c_str());

        _debug ("Init audio RTP session: remote ip %s\n", _ca->getLocalSDP()->get_remote_ip().data());

        if (!remote_ip) {
            _debug (" !ARTP Thread Error: Target IP address [%s] is not correct!\n", _ca->getLocalSDP()->get_remote_ip().data());
            return;
        }

        _debug ("++++Address: %s, audioport: %d\n", _ca->getLocalSDP()->get_remote_ip().c_str(), _ca->getLocalSDP()->get_remote_audio_port());

        _debug ("++++Audioport: %d\n", (int) _ca->getLocalSDP()->get_remote_audio_port());

        if (!_session->addDestination (remote_ip, (unsigned short) _ca->getLocalSDP()->get_remote_audio_port())) {
            _debug (" !ARTP Thread Error: can't add destination to session!\n");
            return;
        }

        _remoteIpIsSet = true;
    } else {
        _debug ("+++++++++++++++++++++++ IP ADDRESS ALREADY SET ++++++++++++++++++++++++\n");
    }

}



float
AudioRtpRTX::computeCodecFrameSize (int codecSamplePerFrame, int codecClockRate)
{
    return ( (float) codecSamplePerFrame * 1000.0) / (float) codecClockRate;
}

int
AudioRtpRTX::computeNbByteAudioLayer (float codecFrameSize)
{
    return (int) ( (float) _layerSampleRate * codecFrameSize * (float) sizeof (SFLDataFormat) / 1000.0);
}


int
AudioRtpRTX::processDataEncode()
{

    // compute codec framesize in ms
    float fixed_codec_framesize = computeCodecFrameSize (_audiocodec->getFrameSize(), _audiocodec->getClockRate());

    // compute nb of byte to get coresponding to 20 ms at audio layer frame size (44.1 khz)
    int maxBytesToGet = computeNbByteAudioLayer (fixed_codec_framesize);

    // available bytes inside ringbuffer
    int availBytesFromMic = _audiolayer->getMainBuffer()->availForGet(_ca->getCallId());

    // _debug("AudioRtpRTX::processDataEncode() callid: %s availBytesFromMic %i\n", _ca->getCallId().c_str(), availBytesFromMic);

    // _debug("AudioRtpRTX::processDataEncode: availBytesFromMic: %i\n", availBytesFromMic);
    // set available byte to maxByteToGet
    int bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;
    // _debug("bytesAvail %i\n", bytesAvail);
    if (bytesAvail == 0)
        return 0;

    // _debug("AudioRtpRTX::processDataEncode: bytesAvail: %i\n", bytesAvail);
    // Get bytes from micRingBuffer to data_from_mic
    int nbSample = _audiolayer->getMainBuffer()->getData (micData , bytesAvail, 100, _ca->getCallId()) / sizeof (SFLDataFormat);

    rtp_output_rec->write((char*)micData, bytesAvail);

    // _debug("AudioRtpRTX::processDataEncode: nbSample: %i\n", nbSample);

    // nb bytes to be sent over RTP
    int compSize = 0;

    // test if resampling is required
    if (_audiocodec->getClockRate() != _layerSampleRate) {

        int nb_sample_up = nbSample;
        //_debug("_nbSample audiolayer->getMic(): %i \n", nbSample);

        // Store the length of the mic buffer in samples for recording
        _nSamplesMic = nbSample;

        nbSample = reSampleData (micData , micDataConverted, _audiocodec->getClockRate(), nb_sample_up, DOWN_SAMPLING);

        compSize = _audiocodec->codecEncode (micDataEncoded, micDataConverted, nbSample*sizeof (int16));

    } else {
        // no resampling required
        compSize = _audiocodec->codecEncode (micDataEncoded, micData, nbSample*sizeof (int16));
    }

    return compSize;
}


void
AudioRtpRTX::processDataDecode (unsigned char* spkrData, unsigned int size, int& countTime)
{
    if (_audiocodec != NULL) {

        // Return the size of data in bytes
        int expandedSize = _audiocodec->codecDecode (spkrDataDecoded , spkrData , size);

        // buffer _receiveDataDecoded ----> short int or int16, coded on 2 bytes
        int nbSample = expandedSize / sizeof (SFLDataFormat);

        // test if resampling is required

        if (_audiocodec->getClockRate() != _layerSampleRate) {

            // Do sample rate conversion
            int nb_sample_down = nbSample;
            nbSample = reSampleData (spkrDataDecoded, spkrDataConverted, _codecSampleRate, nb_sample_down, UP_SAMPLING);

	    rtp_input_rec->write((char*)spkrDataConverted, nbSample * sizeof (SFLDataFormat));

            // Store the number of samples for recording
            _nSamplesSpkr = nbSample;



            // put data in audio layer, size in byte
            _audiolayer->getMainBuffer()->putData (spkrDataConverted, nbSample * sizeof (SFLDataFormat), 100, _ca->getCallId());

        } else {

            // Stor the number of samples for recording
            _nSamplesSpkr = nbSample;

            // put data in audio layer, size in byte
            _audiolayer->getMainBuffer()->putData (spkrDataDecoded, nbSample * sizeof (SFLDataFormat), 100, _ca->getCallId());
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
AudioRtpRTX::sendSessionFromMic (int timestamp)
{
    // STEP:
    //   1. get data from mic
    //   2. convert it to int16 - good sample, good rate
    //   3. encode it
    //   4. send it

    timestamp += time->getSecond();
    // no call, so we do nothing

    if (_ca==0) {
        _debug (" !ARTP: No call associated (mic)\n");
        return;
    }

    // AudioLayer* audiolayer = Manager::instance().getAudioDriver();
    if (!_audiolayer) {
        _debug (" !ARTP: No audiolayer available for MIC\n");
        return;
    }

    if (!_audiocodec) {
        _debug (" !ARTP: No audiocodec available for MIC\n");
        return;
    }

 

    int compSize = processDataEncode();

    // putData put the data on RTP queue, sendImmediate bypass this queue
    // _debug("AudioRtpRTX::sendSessionFromMic: timestamp: %i, compsize: %i\n", timestamp, compSize);
    if((compSize != 0) && (micDataEncoded != NULL))
        _session->putData (timestamp, micDataEncoded, compSize);
    // _session->sendImmediate(timestamp, micDataEncoded, compSize);


}


void
AudioRtpRTX::receiveSessionForSpkr (int& countTime)
{

    if (_ca == 0) {
        return;
    }

    if (!_audiolayer) {
        _debug (" !ARTP: No audiolayer available for SPEAKER\n");
        return;
    }

    if (!_audiocodec) {
        _debug (" !ARTP: No audiocodec available for SPEAKER\n");
        return;
    }

    const ost::AppDataUnit* adu = NULL;

    // int is_waiting = _session->isWaiting();

    // if (is_waiting != 0)
    adu = _session->getData (_session->getFirstTimestamp());
    // else
    //   return;

    

    if (adu == NULL)
        return;

    unsigned char* spkrData  = (unsigned char*) adu->getData(); // data in char

    unsigned int size = adu->getSize(); // size in char

    processDataDecode (spkrData, size, countTime);

    if (adu != NULL)
    {
        delete adu;
	adu = NULL;
    }

}


int
AudioRtpRTX::reSampleData (SFLDataFormat *input, SFLDataFormat *output, int sampleRate_codec, int nbSamples, int status)
{
    if (status==UP_SAMPLING) {
        return converter->upsampleData (input, output, sampleRate_codec , _layerSampleRate , nbSamples);
    } else if (status==DOWN_SAMPLING) {
        return converter->downsampleData (micData , micDataConverted , sampleRate_codec , _layerSampleRate , nbSamples);
    } else

        return 0;
}



void
AudioRtpRTX::run ()
{

    int sessionWaiting;

    initBuffers();
    initAudioRtpSession();
    setRtpSessionRemoteIp();
    setRtpSessionMedia();

    int timestep = _codecFrameSize;

    int countTime = 0; // for receive

    int threadSleep = 0;

    if (_codecSampleRate != 0)
        threadSleep = (_codecFrameSize * 1000) / _codecSampleRate;
    else
        threadSleep = _layerFrameSize;

    TimerPort::setTimer (threadSleep);

    _audiolayer->startStream();

    _audiolayer->getMainBuffer()->flush(_ca->getCallId());

    _session->startRunning();

    int timestamp = _session->getCurrentTimestamp(); // for mic

    _debug ("- ARTP Action: Start call %s\n",_ca->getCallId().c_str());

    while (!testCancel()) {

	for(int i = 0; i < nbSamplesMax; i++)
	{
	    micData[i] = 0;
	    micDataConverted[i] = 0;
	    micDataEncoded[i] = 0;

	    spkrDataConverted[i] = 0;
	    spkrDataDecoded[i] = 0;
	}

	// _debug("Main while loop for call: %s\n", _ca->getCallId().c_str());
        // Send session
        sessionWaiting = _session->isWaiting();

        sendSessionFromMic (timestamp);
        timestamp += timestep;
        // timestamp = _session->getCurrentTimestamp();

        // Recv session
        receiveSessionForSpkr (countTime);

        // Let's wait for the next transmit cycle

	/*
        if (sessionWaiting == 1) {
            // Record mic and speaker during conversation
            _ca->recAudio.recData (spkrDataConverted,micData,_nSamplesSpkr,_nSamplesMic);
        } else {
            // Record mic only while leaving a message
            _ca->recAudio.recData (micData,_nSamplesMic);
        }
	*/

        // Let's wait for the next transmit cycle
        Thread::sleep (TimerPort::getTimer());
	

        // TimerPort::incTimer(20); // 'frameSize' ms
        TimerPort::incTimer (threadSleep);
	
    }

    // _audiolayer->stopStream();
    _debug ("- ARTP Action: Stop call %s\n",_ca->getCallId().c_str());


}


// EOF
